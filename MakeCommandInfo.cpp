#include "MakeCommandInfo.h"
#include "FileKind.h"
#include "environment.h"
#include "base/os.h"
#include "base/filesystem.h"
#include "base/string_util.h"
#include "base/debug.h"

static const std::string default_goal_marker = "@ Default goal: ";
static std::string makefiles_marker = "MAKEFILE_LIST := ";
static std::string specific_vpath_marker = "vpath ";
static std::string vpath_marker = "# General ('VPATH' variable) search path:";
static std::string rules_marker = "# Implicit Rules";
static std::string end_of_rules_marker = "# files hash-table stats:";

namespace sa {
  static std::ostream &operator<<(
    std::ostream &out, const sa::MakeCommandInfo::Pattern &target
  )
  {
    return out << target.data();
  }

  static std::ostream &operator<<(
    std::ostream &out, const sa::MakeCommandInfo::Vpath &vpath
  )
  {
    return out << "vpath " << vpath.pattern << " " << vpath.paths;
  }

  std::ostream &operator<<(
    std::ostream &out, const sa::MakeCommandInfo::Rule &rule
  )
  {
    out << rule.target << ":";
    for (auto const &dep: rule.prerequisites) {
      out << " " << dep;
    }
    return out;
  }
}

void sa::MakeCommandInfo::set_make_command(
  std::vector<std::string_view> command,
  std::string const &build_directory
)
{
  trace_nest("MakeCommandInfo::set_make_command: cd "
    << base::quote_command_arg(build_directory) << " && "
    << base::quote_command_line(command)
  );
  _build_directory = build_directory;
  assert(base::is_absolute_path(_build_directory));
  _default_goal.clear();
  _named_goals.clear();
  _makefiles.clear();
  _specific_vpaths.clear();
  _general_vpath.clear();
  _compilation_rules.clear();
  
  std::vector<const char*> args;
  for (auto &arg: command) {
    args.push_back(arg.data());
  }
  // Select a goal that always exists; we do not want make to complain that it
  // cannot build the requested goal.
  args.push_back(".");
  //
  // In addition to the normal building, print internal information about rules
  // and variables.
  args.push_back("--print-data-base");
  //
  // Do not build anything,  return non-zero status if not up-to-date.
  args.push_back("--question");
  {
    std::ofstream temp_file;
    temp_path = base::open_temp_for_writing(temp_file, "", ".mk");
    if (!temp_file.is_open()) {
      // fatal error - how to handle?
      assert_(false, "cannot open temp file: " << temp_path);
    }
    temp_file << "$(info " << default_goal_marker << "$(.DEFAULT_GOAL))\n";
    //
    // TODO: override HDIRS HDIR_FLAGS OFILES ... here? Or find another way to
    // ignore the contents of a not-yet-up-to-date filetree.mk? Maybe add
    // filetree.mk as a command line argument (with -f) instead of including it
    // from the main makefile?
    //
    // Careful: the makefile might contain a rule like:
    //
    //     $(OFILES): %.o: %.c; gcc -c $< -o $@
    //
    // In that case, it is essential that OFILES contains all potential OFILES
    // rather than none. In other words: if the makefile includes filetree.mk
    // and does not override OFILES (or any other variable defined there), then
    // for each source file added to the SA, determine the object files that
    // would be added to OFILES, and add it for analysis. If OFILES gets
    // overridden or re-assigned in the main makefile, things become even more
    // complex; how far do we want to go?
    //
    // Note: rules like the above get expanded into a number of non-pattern
    // rules in the output of make --print-data-base.
  }
  args.push_back("-f");
  args.push_back(temp_path.data());
  debug_atomic_writeln("make info command: cd "
    << base::quote_command_arg(build_directory) << " && "
    << base::quote_command_line(args)
  );
  args.push_back(0);
  int exit_code = 0;
  int error_number = base::os::execute_and_capture(
    args.data(), _build_directory.data(),
    this, &MakeCommandInfo::handle_stdout, &MakeCommandInfo::handle_stderr,
    exit_code, standard_environment
  );
  trace("error number " << error_number
    << " " << base::os::strerror(error_number)
  );
  trace("exit code " << exit_code);
  (void)error_number;
  //base::remove(temp_path);
}

std::string sa::MakeCommandInfo::choose_target(
  const std::string &source
) const
{
  trace_nest("Choose target for " << source);
  // Current heuristics:
  //
  // - prefer object file as target over file kinds;
  //
  // - prefer matches with a shorter stem.  Note that we are comparing here
  //   between rules for different targets, so this is different from what make
  //   does when choosing the best rule for a given target.
  //
  // TODO: only consider matches with a recipe, ideally a recipe that contains
  // something that looks like a compilation command. TODO
  //
  std::map<std::string, std::string> map;
  fill_target_to_stem_map(source, map);
  //
  std::string best_target;
  std::string best_stem;
  bool found = false;
  bool found_object = false;
  //
  for (const auto &[target, stem]: map) {
    trace("Consider " << target << " with stem " << stem);
    bool is_object = is_object_file(target.data());
    if (!found || stem.length() < best_stem.length()
      || (is_object && !found_object)
    ) {
      found = true;
      found_object = is_object;
      best_target = target;
      best_stem = stem;
    }
  }
  if (!found) {
    trace("no rule for " << source << " so fallback");
    best_target = source + ".o";
  }
  trace("Select " << best_target << " for " << source);
  return best_target;
}
                                                
std::vector<std::string> sa::MakeCommandInfo::targets_for_source(
  const std::string &source
) const
{
  trace_nest("targets for source " << source);
  std::map<std::string, std::string> map;
  fill_target_to_stem_map(source, map);
  
  std::vector<std::string> targets;
  for (auto const &key_value_pair: map) {
    targets.emplace_back(key_value_pair.first);
  }
  trace("targets: " << targets);
  return targets;
}

void sa::MakeCommandInfo::fill_target_to_stem_map(
  const std::string &source,
  std::map<std::string, std::string> &map
) const
{
  trace_nest("fill target to stem map for source " << source);
  trace("consider " << _compilation_rules.size() << " rules");
  for (auto const &rule: _compilation_rules) {
    trace("rule: " << rule);
  }
  // The rules in the makefile can specify the same source file as a dependency
  // in two ways: as a path relative to the build folder, or as an absolute
  // path. We normalize paths before matching, just like `make` normalizes
  // target patterns before matching (eliminating things like /./ or /foo/../)
  // so there is only one relative form and one absolute form to be matched.
  //
  // On Linux, rules can also refer to the same file via a symbolic link, but we
  // treat symbolic links as if they create a copy of the thing they point to,
  // so matches via a symbolic link are not treated as rules for the same source
  // file prerequisite.
  //
  // To avoid computing absolute and relative paths for the same file again for
  // each rule, we compute them once before the loop over rules.
  //
  assert_(base::is_normalized_path(source),
    source << " (" << source.size() << ")"
  );
  std::string rel_source = base::get_relative_path(source, _build_directory);
  std::string abs_source = base::get_absolute_path(source, _build_directory);
  
  for (auto const &rule: _compilation_rules) {
    match_source_to_rule(rel_source, rule, map);
    match_source_to_rule(abs_source, rule, map);
  }
}

void sa::MakeCommandInfo::match_source_to_rule(
  const std::string &source,
  const Rule &rule,
  std::map<std::string, std::string> &map
) const
{
  trace("match " << rule << " to " << source);
  for (auto const& prereq: rule.prerequisites) {
    match_source_to_prereq(source, rule.target, prereq, map);
  }
}

void sa::MakeCommandInfo::match_source_to_prereq(
  const std::string &source,
  const Pattern &target,
  const Pattern &prereq,
  std::map<std::string, std::string> &map
) const
{
  //
  // A rule prerequisite matches a source path if it is equal to the source
  // path, possibly with an applicable vpath removed. So we must try to match
  // the prerequisite to the original source path as well as the source path
  // with any vpath removed. For specific vpaths, the resulting path must match
  // the vpath pattern.
  //
  // Note that vpath paths can be absolute or relative (to the build directory),
  // so we need to try the matching for the absolute source path and for the
  // relative source path. This is handled by calling this function for both
  // source paths.
  //
  // A potential optimization is to sort the paths in a vpath into two sets, one
  // with the absolute paths and one with the relative paths, and to consider
  // only one set depending on the given source path.  TODO.
  //
  // VPATH and vpath are applied alike for prerequisite patterns with or without
  // '%'.
  //
  match_path_to_prereq(source, target, prereq, map);
  //
  // Try vpaths for relative prerequisites only.
  if (base::is_relative_path(prereq.data())) {
    for (auto const &vpath: _specific_vpaths) {
      // Possible optimization: already match vpath.target suffix to source.  If
      // the suffix does not match, removing a vpath path prefix will not make
      // it match, so the loop over vpath paths can be skipped. Only worthwhile
      // for vpaths with many paths. TODO?
      for (auto const &path: vpath.paths) {
        if (base::is_strictly_nested_in(source, path)) {
          std::string short_source(source, path.length()+1);
          if (vpath.pattern.matches(short_source)) {
            trace("try vpath " << vpath.pattern << " " << path);
            match_path_to_prereq(short_source, target, prereq, map);
          }
        }
      }
    }
    for (auto const &path: _general_vpath) {
      if (base::is_strictly_nested_in(source, path)) {
        std::string short_source(source, path.length()+1);
        trace("try VPATH " << path);
        match_path_to_prereq(short_source, target, prereq, map);
      }
    }
  }
}

void sa::MakeCommandInfo::match_path_to_prereq(
  const std::string &path,
  const Pattern &target,
  const Pattern &prereq,
  std::map<std::string, std::string> &map
) const
{
  if (target.data().find('/') == std::string::npos) {
    // Target without slash uses special prerequisite matching, with stem_path
    // and stem_name.
    std::string stem_path;
    std::string stem_name;
    if (prereq.match(path, stem_path, stem_name)) {
      std::string target_path = stem_path + target.fill(stem_name);
      map_target_to_stem(target_path, stem_path + stem_name, map);
    }
  } else {
    // Target with slash uses ordinary prerequisite matching. Note: this is
    // true, even if the slash comes after the percent.
    std::string stem;
    if (prereq.match(path, stem)) {
      // Do not attempt to match an absolute stem. The target's prefix would be
      // inserted before the absolute path, which leads to a non-sensical path.
      // For example:
      //
      //   foo/%.o: %.c
      //
      // Matching /tmp/foo.c against the prerequisite %.c yields stem /tmp/foo
      // and target foo//tmp/foo.o.
      if (base::is_relative_path(stem)) {
        std::string target_path = target.fill(stem);
        map_target_to_stem(target_path, stem, map);
      }
    }
  }
}
  
void sa::MakeCommandInfo::map_target_to_stem(
  const std::string &target,
  const std::string &stem,
  std::map<std::string, std::string> &target_to_stem_map
) const
{
  // In accordance with make's algorithm, if there are multiple rules to build
  // the same target, the one with the shortest stem wins. In case of a tie, the
  // first one wins.
  auto it = target_to_stem_map.find(target);
  if (it == target_to_stem_map.end() || stem.length() < it->second.length()) {
    trace(" `-> accept " << target << " with stem " << stem);
    target_to_stem_map[target] = stem;
  }
}

// Check if the given string matches this target. If it does, set stem and
// return true. Otherwise, do not touch stem and return false.
bool sa::MakeCommandInfo::Pattern::match(
  const std::string &data, std::string &stem
) const
{
  size_t pos = _data.find('%');
  if (pos == _data.npos) {
    if (_data == data) {
      stem = "";
      return true;
    }
  }
  size_t prefix_size = pos;
  size_t suffix_size = _data.length() - pos - 1;
  if (std::equal(_data.rbegin(), _data.rbegin() + suffix_size, data.rbegin())
    && std::equal(_data.begin(), _data.begin() + prefix_size, data.begin())
  ) {
    stem = data.substr(pos, data.length() - prefix_size - suffix_size);
    return true;
  }
  return false;
}

// Return true iff the given string matches this target. Same as "match(...)"
// except that it doesn't return the stem.
bool sa::MakeCommandInfo::Pattern::matches(const std::string &data) const
{
  size_t pos = _data.find('%');
  if (pos == _data.npos) {
    // non-pattern: test full equality
    if (_data == data) {
      return true;
    }
  }
  // pattern: test prefix and suffix
  size_t prefix_size = pos;
  size_t suffix_size = _data.length() - pos - 1;
  if (std::equal(_data.rbegin(), _data.rbegin() + suffix_size, data.rbegin())
    && std::equal(_data.begin(), _data.begin() + prefix_size, data.begin())
  ) {
    return true;
  }
  return false;
}

// Check if the given string matches this target, allowing for a path prefix in
// the string. If it does, set stem path and name and return true. Otherwise, do
// not touch stem path or name and return false.
//
// This kind of matching is used in pattern rules, when the target does not
// contain a slash.  For example, the prerequisite of this rule:
//
//   p%.o: foo/p%.c
//
// matches source path "tip/foo/px.c" yielding stem path "tip/" and stem name
// "x", which results in target "tip/px.o" and stem "tip/x". The matching
// algorithm matches the last slash in the prerequisite "foo/p%.c" with the last
// slash in the source path "tip/foo/px.c".
//

bool sa::MakeCommandInfo::Pattern::match(
  const std::string &source, std::string &stem_path, std::string &stem_name
) const
{
  auto &prereq = _data;
  
  size_t prereq_percent_pos = prereq.find('%');
  if (prereq_percent_pos == prereq.npos) {
    if (prereq == source) {
      stem_path = "";
      stem_name = "";
      return true;
    }
  }
  // Match suffix.
  size_t suffix_size = prereq.length() - prereq_percent_pos - 1;
  if (std::equal(
      prereq.rbegin(),
      prereq.rbegin() + suffix_size,
      source.rbegin()
    )
  ) {
    // Determine prereq and source layout:
    // 
    //             ,----> full prefix size == prereq_percent_pos == 5
    //         vvvvv
    //   p%.o: foo/p%.c
    //            ^^^
    //            ||`---> prereq_percent_pos == 5
    //            |`----> prereq_prefix_pos == 4 (pos does not include path!)
    //            `-----> prereq_slash_pos == 3 (can be npos)
    //
    //              ,--------> stem path size == 4
    //              |    ,---> stem name size == 1
    //           vvvv    v
    //   source: tip/foo/px.c
    //                  ^^
    //                  |`---> source_prefix_pos == 8
    //                  `----> source_slash_pos == 7 (can be npos)
    //
    // Compute stem prefix position (ignoring path) for both, based on the
    // position of the last slash: it is the position just beyong the last
    // slash, or zero if there is no last slash.
    //
    size_t prereq_slash_pos = prereq.rfind('/', prereq_percent_pos);
    size_t prereq_prefix_pos =
      prereq_slash_pos == prereq.npos ? 0 : prereq_slash_pos+1;
    size_t source_slash_pos = source.rfind('/');
    size_t source_prefix_pos =
      source_slash_pos == source.npos ? 0 : source_slash_pos+1;
    //
    // Match full prefix at once. To avoid negative string indexes, first verify
    // that source has room for the full prefix.
    if (source_prefix_pos >= prereq_prefix_pos) {
      size_t stem_path_size = source_prefix_pos - prereq_prefix_pos;
      if (std::equal(
          prereq.begin(),
          prereq.begin() + prereq_percent_pos,
          source.begin() + stem_path_size
        )
      ) {
        stem_path = source.substr(0, stem_path_size);
        //
        size_t stem_name_pos = stem_path_size + prereq_percent_pos;
        size_t stem_name_size = source.length() - stem_name_pos - suffix_size;
        stem_name = source.substr(stem_name_pos, stem_name_size);
        return true;
      }
    }
  }
  return false;
}

// Return the target string with '%' replaced by the given stem.
std::string sa::MakeCommandInfo::Pattern::fill(std::string &stem) const
{
  size_t pos = _data.find('%');
  if (pos == _data.npos) {
    return _data;
  } else {
    return _data.substr(0,pos) + stem + _data.substr(pos+1);
  }
}

// Split string at spaces (one or more), add substrings to list, except if empty
// or equal to `skip`.
static void split(
  const std::string &data,
  std::vector<std::string> &list,
  const std::string &skip = ""
)
{
  size_t pos = 0;
  for (;;) {
    size_t next = data.find(' ', pos);
    if (next > pos) {
      list.emplace_back(data, pos, next-pos);
      if (list.back() == skip) list.pop_back();
      if (next == std::string::npos) break;
    }
    pos = next + 1;
  }
}

// Split string at colons,  add substrings to list.
static void split_path(
  const std::string &data,
  std::vector<std::string> &list
)
{
  size_t pos = 0;
  for (;;) {
    size_t next = data.find(':', pos);
    list.emplace_back(data, pos, next-pos);
    trace("[" << list.back() << "]");
    if (next == std::string::npos) break;
    pos = next + 1;
  }
}

void sa::MakeCommandInfo::parse_specific_vpath(
  const std::string &line
)
{
  trace_nest("parse_specific_vpath: " << line);
  Vpath vpath;
  size_t pos = line.find_first_not_of(" ", specific_vpath_marker.length());
  size_t next = line.find(' ', pos);
  if (next != std::string::npos) {
    vpath.pattern = std::string(line, pos, next-pos);
    pos = line.find_first_not_of(" ", next+1);
    split_path(line.substr(pos), vpath.paths);
    trace("found " << vpath);
    _specific_vpaths.emplace_back(vpath);
  }
}

bool sa::MakeCommandInfo::is_named_goal(const std::string &target) const
{
  if (!isalpha(target[0])) return false;
  for (auto c: target) {
    if (!isalnum(c) && c != '_' && c != '-') return false;
  }
  return true;
}

static bool can_be_compiled_file(std::string const &path)
{
  sa::FileKind kind = sa::guess_gcc_file_kind(path);
  return kind == sa::FileKind_object || kind == sa::FileKind_executable;
}

static bool is_compilation_output_extension(std::string const &extension)
{
  return can_be_compiled_file(extension);
}

// Parse rule, add to compilation rules if it produces an object file or binary.
void sa::MakeCommandInfo::parse_rule(std::string line, std::istream &in)
{
  trace_nest("Parse rule");
  bool has_recipe = false;
  Rule rule;
  while (in && !line.empty()) {
    trace("Rule line: " << line);
    if (line[0] == '#') {
      // skip comment
    } else if (line[0] == '\t') {
      trace("  `-> is recipe line");
      has_recipe = true;
    } else {
      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        // This is a rule. It might
        // be explicit: target+ :[:] dep+
        // or implicit: .x.y:
        // or implicit: .z:
        if (line[0] == '.') {
          // Implicit rule (should not have deps after colon)
          size_t dot_pos = line.find('.', 1);
          if (dot_pos == std::string::npos) {
            // style: .z:
            // target is %, prerequisite is %.z
            // The target has no extension, so might be a binary file.
            std::string dep(line, 0, colon_pos);
            if (is_source_file(dep)) {
              //rule.target = "%"; // this is the default
              rule.prerequisites.emplace_back("%" + dep);
            }
          } else {
            // style: .x.y:
            // target is %.y, prerequisite is %.x
            std::string target_extension(line,dot_pos,colon_pos-dot_pos);
            // Assumption: compilation rules produce a .o or .obj file
            if (is_compilation_output_extension(target_extension)) {
              std::string dep(line, 0, dot_pos);
              if (is_source_file(dep)) {
                rule.target = Pattern("%" + target_extension);
                rule.prerequisites.emplace_back("%" + dep);
              }
            }
          }
        } else {
          // one or two colons?
          size_t colon_cnt =
            colon_pos+1 < line.length() && line[colon_pos+1] == ':' ? 2 : 1;
          (void)colon_cnt;
          std::vector<std::string> targets;
          split(std::string(line,0,colon_pos), targets);
          if (!targets.empty()) {
            std::vector<std::string> deps;
            // Note: prerequisites after '|' are order-only; see// https: ...
            //www.gnu.org/software/make/manual/html_node/Prerequisite-Types.html
            // The '|' itself is not a prerequisite
            split(std::string(line, colon_pos + colon_cnt), deps, "|");
            for (auto const &target: targets) {
              trace_nest("target: " << target);
              if (is_named_goal(target)) {
                trace("goal: " << target);
                _named_goals.emplace(target);
              }
              if (can_be_compiled_file(target)) {
                rule.target = target;
                for (auto const &dep: deps) {
                  // For compilation rules, we are only interested in source
                  // file prerequisites.
                  if (is_source_file(dep)) {
                    trace(dep << " can be source file!");
                    rule.prerequisites.emplace_back(dep);
                  }
                }
              }
            }
          }
        }
      }
    }
    base::getline(in, line);
  }
  // A compilation rule must have a recipe and at least one source file
  // prerequisite.
  //
  if (!rule.prerequisites.empty()) {
    if (has_recipe) {
      trace("Add rule: " << rule);
      _compilation_rules.emplace_back(rule);
    } else {
      // TODO: if a rule has no recipe, it may still define extra prerequisites
      // for another rule matching the same target, and one of these could be a
      // source file! Need to figure out how to handle that case.
      trace("TODO: handle prerequisites in rule without recipe: " << rule);
    }
  }
}

void sa::MakeCommandInfo::handle_stdout(std::istream &in)
{
  trace_nest("handle stdout");
  std::string line;
  for (;;) {
    base::getline(in, line);
    if (!in) break;
    trace("OUT: " << line);
    if (base::begins_with(line, default_goal_marker)) {
      _default_goal = line.substr(default_goal_marker.length());
      trace("set default goal to: " << _default_goal);
    } else if (base::begins_with(line, makefiles_marker)) {
      trace("Makefiles: " << line.substr(makefiles_marker.length()));
      std::vector<std::string> rel_makefiles;
      split(line.substr(makefiles_marker.length()), rel_makefiles, temp_path);
      for (std::string const &rel_makefile: rel_makefiles) {
        trace("rel makefile: " << rel_makefile);
        std::string abs_makefile =
          base::get_absolute_path(rel_makefile, _build_directory);
        // A makefile might try to include a directory, or a non-existant
        // makefile.  Reporting inclusion status on a directory may cause the
        // client to crash.
        if (base::is_file(abs_makefile)) {
          trace("Makefile: " << abs_makefile);
          _makefiles.emplace_back(abs_makefile);
        }
      }
    } else if (base::begins_with(line, specific_vpath_marker)) {
      parse_specific_vpath(line);
    } else if (line == vpath_marker) {
      base::getline(in, line);
      if (!in) break;
      trace("OUT (vpath): " << line);
      if (base::begins_with(line, "# ")) {
        split_path(line.substr(2), _general_vpath);
      }
    } else if (line == rules_marker) {
      for (;;) {
        base::getline(in, line);
        if (!in || line == end_of_rules_marker) break;
        parse_rule(line, in);
      }
    }
  }
}

void sa::MakeCommandInfo::handle_stderr(std::istream &in)
{
  // Although we are not interested in error output, we must process it to avoid
  // blocking the sending command when system buffers are full.
  in.ignore(std::numeric_limits<std::streamsize>::max());
}

sa::MakeCommandInfo::Pattern::Pattern(const std::string &data)
  : _data(base::get_normalized_path(data))
{
}

sa::MakeCommandInfo::Pattern::Pattern(
  const std::string &prefix, const std::string &suffix
)
  : _data(base::get_normalized_path(prefix + "%"+ suffix))
{
}

#ifdef SELFTEST

#include <iostream>
#include <algorithm>
#include "base/debug.h"

void sa::MakeCommandInfo::test_specific_vpath(
  const std::string &pattern,
  const std::string &target,
  const std::vector<std::string> &paths
)
{
  parse_specific_vpath(pattern);
  assert_(_specific_vpaths.back().pattern == Pattern(target),
    _specific_vpaths.back().pattern << " != " << Pattern(target)
  );
  assert(_specific_vpaths.back().paths == paths);
  _specific_vpaths.pop_back();
}

void sa::MakeCommandInfo::selftest()
{
  assert_(default_goal() == "default", "'" << default_goal() << "'");
  assert_(makefiles() == std::vector<std::string>(
      {base::get_absolute_path("makefile", build_directory())}
    ), makefiles()
  );
  assert_(_general_vpath == std::vector<std::string>({"foo/bar", ".."}),
    _general_vpath
  );

  std::vector<std::string> main_targets = targets_for_source("main.c");
  //assert_(main_targets == std::vector<std::string>({"main.o"}), main_targets);
  assert_(std::find(main_targets.begin(), main_targets.end(), "main.o")
    != main_targets.end(), main_targets
  );
  std::string main_target = choose_target("main.c");
  assert_(main_target == "main.o" || main_target == "bar/main.o", main_target);

  auto start_targets = targets_for_source("start.S");
  assert_(std::find(start_targets.begin(), start_targets.end(), "start.o")
    != start_targets.end(), start_targets
  );

  test_specific_vpath("vpath %.c src:src/code", "%.c", {"src", "src/code"});
  test_specific_vpath("vpath foo%bar .", "foo%bar", {"."});
  test_specific_vpath("vpath foo  x:", "foo", {"x", ""});
  test_specific_vpath("vpath foo% :. ", "foo%", {"", ". "});
}

int main()
{
  std::cout << "Hello\n";
  const char *toolprefix;
  std::string cwd = base::get_working_directory();
  std::string abs = base::get_absolute_path("test/pic32", cwd);
  const char *build_dir = abs.data();
  if (base::os::get_os() == base::os::OS_windows) {
    toolprefix = 
      "TOOLPREFIX=C:/msys64/home/johan/embeetle/beetle_tools/windows/"
      "mips_mti_toolchain_11.2.0_64b/bin/mips-mti-elf-"
      ;
  } else {
    toolprefix = 
      "TOOLPREFIX=/home/johan/embeetle/beetle_tools/windows/"
      "mips_mti_toolchain_11.2.0_64b/bin/mips-mti-elf-"
      ;
  }
  sa::MakeCommandInfo info;
  info.set_make_command({ "make", "-f", "makefile", toolprefix },  build_dir);
  info.selftest();

  std::cout << "Selftest succeeded\n";
  return 0;
}

#endif
