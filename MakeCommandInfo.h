// Copyright 2018-2024 Johan Cockx
#ifndef __MakeCommandInfo_h
#define __MakeCommandInfo_h

#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>

#if 0
#include <map>
#include <iostream>
#endif

namespace sa {
  class MakeCommandInfo {

  public:
    
    // Set the make command and the directory in which to run it.
    //
    // This will update the default goal, named goals, list of makefiles and the
    // internal data used in targets_for_source and choose_target.
    void set_make_command(
      std::vector<std::string_view> command,
      std::string const &build_directory
    );

    // Build directory assumed by this make command info. The project's build
    // directory may have changed since this was copied.
    std::string const &build_directory() const { return _build_directory; }

    // The default goal is the first non-pattern goal seen by make,  unless
    // overridden using .DEFAULT_GOAL.
    std::string const &default_goal() const { return _default_goal; }

    // Named goals start with a letter, followed by letters, digits, dash and
    // underscore. We assume that any goals intended for explicit use on the
    // command line are named.
    std::set<std::string> const &named_goals() const { return _named_goals; }

    // Makefiles specified on the command line as well as included makefiles.
    // These are all absolute paths.
    std::vector<std::string> const &makefiles() const { return _makefiles; }

    // Return a list of targets that are compiled from the given source file
    // using this make command.
    //
    // There may be multiple targets, representing for example multiple object
    // or executable files that can be compiled from the given source file.
    // "Make" uses the ones that are declared as dependencies for the goal it is
    // given, but we don't know that here.  It is perfectly possible - although
    // not common - that the same source file is linked more than once, as
    // different object files with different compilation options, or that the
    // object file used depends on the goal; for example, there may be separate
    // goals for a debug executable and a deployment executable.
    //
    // The source file and targets are either absolute or relative to the build
    // directory, i.e. this make command's working directory.
    std::vector<std::string> targets_for_source(const std::string &source)
      const;

    // Choose a target - probably an object file - that will compile a given
    // source file.
    //
    // Since there are in general multiple options, and we don't know the
    // intended goal at this point, we choose a target heuristically.
    //
    // If there are no targets,  we append '.o' to the source path.
    //
    // The source file is specified by path, and the path must be
    // either absolute or relative to the build directory.
    //
    std::string choose_target(const std::string &source) const;
    
  public:
    // For internal use only. Must be public to be able to call it from a main
    // function.
    void selftest();

    // Classes below must be public to be able to declare an operator<< for
    // them. They are of no use publicly.

    // Representation of a pattern: either a path or a path template
    // prefix%suffix. Patterns are always normalized.
    class Pattern {
      std::string _data;

    public:
      Pattern(const std::string &prefix, const std::string &suffix);
      Pattern(const std::string &data);
      Pattern(): _data("%") {}

      const std::string &data() const { return _data; }

      bool is_pattern() const { return _data.find('%') != _data.npos; }

      // Check if the given string matches this pattern. If it does, set stem
      // and return true. Otherwise, do not touch stem and return false.
      bool match(const std::string &data, std::string &stem) const;

      // Return true iff the given string matches this pattern.  Same as
      // "match(data, _)" except that it doesn't return the stem.
      bool matches(const std::string &data) const;

      // Check if the given string matches this target, allowing for a path
      // prefix in the string. If it does, set stem path and name and return
      // true. Otherwise, do not touch stem path or name and return false.
      //
      // This special kind of matching is used in pattern rules, when the target
      // does not contain a slash.  For example, the prerequisite of this rule:
      //
      //   p%.o: foo/p%.c
      //
      // matches source path "tip/foo/px.c" yielding stem path "tip/" and stem
      // name "x", which results in target "tip/px.o" and stem "tip/x". The
      // matching algorithm matches the last slash in the prerequisite
      // "foo/p%.c" with the last slash in the source path "tip/foo/px.c".
      //
      bool match(const std::string &data,
        std::string &stem_path, std::string &stem_name
      ) const;

      // Return the target string with '%' replaced by the given stem.
      std::string fill(std::string &stem) const;

      bool operator==(const Pattern &other) const {return _data == other._data; }
      bool operator==(const std::string &other) const {return _data == other; }
    };

    // Representation of a vpath statement 'vpath prefix%suffix path path ...'
    // Paths can be separated by spaces or colons in the makefile.
    struct Vpath {
      Pattern pattern;
      std::vector<std::string> paths;
    };

    // Representation of a pattern rule. Multi-target rules are split into
    // multiple rule objects. Commands are currently not stored; this is just
    // about targets and prerequisites.
    struct Rule {
      Pattern target;
      std::vector<Pattern> prerequisites;

      // If the target is not a pattern, '%' characters in prerequisites are
      // taken literally.
      bool is_pattern_rule() const { return target.is_pattern(); }
    };

  protected:

    // Parse output of make command
    void handle_stdout(std::istream &in);
    void handle_stderr(std::istream &in);

    // Parsing aux methods
    void parse_specific_vpath(const std::string &line);
    void parse_rule(std::string line, std::istream &in);
    
    // Decide if the given target should be reported as a named goal.
    bool is_named_goal(const std::string &target) const;

    void fill_target_to_stem_map(
      const std::string &source,
      std::map<std::string, std::string> &map
    ) const;
    
    // Note: it doesn't matter that many rules may be able to generate the same
    // .o file, from the same or different source files. We just need to know
    // all the .o files that can be compiled from a given source file, assuming
    // that the prerequisites exist.  A later step will find the actual commands
    // used to create the .o file, taking into account which prerequisites
    // exist.
    //
    // If two source file, such as foo.c and foo.cpp, both compile to foo.o, we
    // will only be able to analyze one of them, because we cannot get `make` to
    // issue the commands for the other. TODO: is there a way around this issue?
    // Maybe when we parse the makefile ourselves, instead of using a dry-run?

    // Match a source file to a rule. For each match, update the target-to-stem
    // map. Take vpaths into account when deciding whether the source file
    // matches the rule. Due to vpaths, there can be multiple matches.
    void match_source_to_rule(
      const std::string &source,
      const Rule &rule,
      std::map<std::string, std::string> &target_to_stem_map
    ) const;
      
    void match_source_to_prereq(
      const std::string &source,
      const Pattern &target,
      const Pattern &prereq,
      std::map<std::string, std::string> &target_to_stem_map
    ) const;

    void match_path_to_prereq(
      const std::string &path,
      const Pattern &target,
      const Pattern &prereq,
      std::map<std::string, std::string> &target_to_stem_map
    ) const;
    
    // Add an entry to the target-to-stem map.
    void map_target_to_stem(
      const std::string &target,
      const std::string &stem,
      std::map<std::string, std::string> &target_to_stem_map
    ) const;
    
    void test_specific_vpath(
      const std::string &pattern,
      const std::string &target,
      const std::vector<std::string> &paths
    );

  private:
    // Path where make command is executed. This is used during initial analysis
    // as well as later while mapping source files to targets, so it must be
    // stored, in contrast to the make command itself.
    std::string _build_directory;

    // Results of initial analysis that can be fetched externally.
    std::string _default_goal;
    std::set<std::string> _named_goals;
    std::vector<std::string> _makefiles;

    // Results of initial analysis used to map source files to targets.
    std::vector<Vpath> _specific_vpaths; 
    std::vector<std::string> _general_vpath; 
    std::vector<Rule> _compilation_rules;

    // Path of temporary file used during initial analysis. Unused afterwards.
    std::string temp_path;
  };

  
}


#endif
