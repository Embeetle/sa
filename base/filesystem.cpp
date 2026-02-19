// Copyright 2018-2023 Johan Cockx
#include "filesystem.h"
#include "os.h"
#include "time_util.h"
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include "debug.h"

std::string base::get_working_directory()
{
  return get_normalized_path(std::filesystem::current_path().string());
}

bool base::path_exists(const std::string &path)
{
  struct stat buffer;
  return stat(path.data(), &buffer) == 0;
}

bool base::is_file(const std::string &path)
{
  struct stat buffer;
  if (stat(path.data(), &buffer) == 0) {
    return S_ISREG(buffer.st_mode);
  }
  return false;
}

bool base::is_readable(const std::string &path)
{
  std::ifstream file(path);
  return file.is_open();
}

bool base::is_directory(const std::string &path)
{
  struct stat buffer;
  if (stat(path.data(), &buffer) == 0) {
    return S_ISDIR(buffer.st_mode);
  }
  return false;
}

uint64_t base::get_signature(const std::string &path)
{
  os::file_meta_data data;
  if (!os::get_file_meta_data(path.data(), data)) {
    trace("get signature file-not-found " << path);
    return 0;
  }
  const unsigned nsec_shift = sizeof(uint64_t) * 8 - 30;
  const unsigned size_shift = sizeof(uint64_t) * 4;
  trace("get_signature sec=" << data.mtime_secs << " nsec=" << data.mtime_nsecs
    << " size=" << data.size_bytes << " " << path
  );
  return data.mtime_secs ^ (data.mtime_nsecs << nsec_shift)
    ^ (data.size_bytes >> size_shift) ^ (data.size_bytes << size_shift);
}

std::string base::get_extension(const std::string &path)
{
  return os::get_extension(path.data());
}

std::string base::get_command_base_name(std::string_view path)
{
  return os::get_command_base_name(std::string(path).data());
}

std::string base::get_command_base_name(const std::string &path)
{
  return os::get_command_base_name(path.data());
}

void base::create_directory(const std::string &path)
{
  trace("create directory " << path);
  os::create_directory(path.data());
}

static bool create_directories_for(const std::string &path, size_t skip)
{
  trace_nest("create directories for " << path << " skip=" << skip);
  std::string buffer = path;
  char *slash = (char*)buffer.data()
    + std::max(skip, base::os::drive_prefix_size(buffer.data()));
  for (;;) {
    slash = strchr(slash+1, '/');
    if (!slash) return true;
    *slash = '\0';
    if (!base::is_directory(buffer)) {
      for (;;) {
        base::create_directory(buffer);
        if (!base::is_directory(buffer)) {
          debug_atomic_writeln("could not create directory " << buffer
            << " skip=" << skip
          );
          *slash = '/';
          return false;
        }
        *slash = '/';
        slash = strchr(slash+1, '/');
        if (!slash) return true;
        *slash = '\0';
      }
    }
    *slash = '/';
  }
  assert(base::is_directory(path));
  return true;
}

void base::remove(const std::string &path)
{
  ::remove(path.data());
}

// Open a file for writing in binary mode. Use file.is_open() to test for
// success.
static void open_file_for_writing(std::ofstream &file, const std::string &path)
{
  trace("open " << path << " for writing");
  file.open(path, std::ios::binary);
}

void base::open_for_writing(
  std::ofstream &file,
  const std::string &path,
  size_t skip
)
{
  trace_nest("open for writing: " << path << " (skip " << skip << ")");
  assert(!file.is_open());
  assert(!skip || path[skip] == '/');
  open_file_for_writing(file, path);
  if (!file.is_open()) {
    trace("create directories for " << path << " " << skip);
    create_directories_for(path, skip);
    trace("try again");
    open_file_for_writing(file, path);
  }
}

void base::open_for_reading(std::ifstream &file, const std::string &path)
{
  trace("open for reading: " << path);
  assert(!file.is_open());
  file.open(path);
}

std::istream &base::getline(std::istream &input, std::string &line)
{
  return os::getline(input, line);
}

// Note: rand_r would be better than rand,  but is not supported on Windows.
// There are even better methods, to be explored.
static int init_rand()
{
  unsigned seed = (unsigned)(base::get_time() * 1e6);
  trace("seed: " << seed);
  srand(seed);
  return 0;
}

std::string base::open_temp_for_writing(std::ofstream &file
  , const std::string &prefix
  , const std::string &suffix
)
{
  trace_nest("open_temp_for_writing " << prefix << "%" << suffix);
  static int init = init_rand(); (void)init;
  FILE *fp = 0;
  std::string path;
  unsigned max_tries = 3;
  for (;;) {
    path = os::get_temp_directory_path() + std::string("/")
      + prefix + std::to_string(rand()) + suffix;
    trace("temp file: try " << path);
    // There is a race condition in the code below.  TODO: use a lock. Note:
    // mode "wx" does not work on Windows, fopen always fails if you try.
    if (path_exists(path)) {
      trace("path exists");
      continue;
    }
    fp = fopen(path.data(), "w");
    trace("fp=" << fp << " " << os::strerror(errno));
    if (fp) {
      fclose(fp);
      file.open(path, std::ios::binary);
      return path;
    }
    if (max_tries) {
      --max_tries;
    } else {
      return os::strerror(errno);
    }
  }
}

std::string base::normalize_path_case(std::string const &path)
{
  std::string norm = path;
  os::normalize_path_case(norm);
  return norm;
}

bool base::patch_path_to_case_on_disk(std::string &path)
{
  return os::path_has_different_case_on_disk(path, path);
}

bool base::is_absolute_path(const std::string &path)
{
  return os::is_absolute_path(path.data());
}

bool base::is_absolute_path(std::string_view path)
{
  return os::is_absolute_path(path);
}

bool base::is_absolute_path(const char *path)
{
  return os::is_absolute_path(path);
}

std::string base::join_paths(const std::string &path1, const std::string &path2)
{
  if (is_absolute_path(path2)) {
    return path2;
  } else {
    return path1 + "/" + path2;
  }
}

std::string base::get_absolute_path(
  const std::string &path,
  const std::string &directory_path
)
{
  if (is_relative_path(path)) {
    return get_normalized_path(join_paths(directory_path,path));
  }
  return path;
}

std::string base::get_relative_path(
  const std::string &item_path,
  const std::string &dir_path
)
{
  trace_nest("get relative path of " << item_path << " from " << dir_path);
  if (is_absolute_path(item_path)) {
    if (!is_absolute_path(dir_path)
      || !os::in_same_tree(item_path.data(), dir_path.data())
    ) {
      return item_path;
    }
  } else {
    if (is_absolute_path(dir_path)) {
      return item_path;
    }
  }
  // At this point, both paths are relative to the same unspecified directory,
  // or both paths are absolute and in the same tree.
  //
  // Find common prefix.
  //
  // Path segments end at '/' or end-of-string; let's call those path
  // terminators.  The common ancestor size 'common_size' is the position of the
  // last terminator that is common to both paths.
  //
  size_t common_size = common_ancestor_size(item_path, dir_path);
  trace("common ancestor size: " << common_size);
  //
  // Special case: everything matches, item equals dir
  if (common_size == item_path.size() && common_size == dir_path.size()) {
    return ".";
  }
  //
  // We will use 'pos' to count the number of additional levels 'nsub' in the
  // directory path; for each level, the relative path will be prefixed by
  // '../'.
  //
  size_t nsub = 0;
  {
    size_t pos = common_size;
    bool in_segment = false;
    while (pos < dir_path.length()) {
      if (dir_path[pos] == '/') {
        if (in_segment) {
          in_segment = false;
          ++nsub;
        }
      } else {
        in_segment = true;
      }
      ++pos;
    }
    if (in_segment) {
      ++nsub;
    }
  }
  trace("nsub=" << nsub);
  
  std::string rel_path;
  rel_path.reserve(3*nsub + item_path.size() - common_size);
  while (nsub--) {
    rel_path.append("../");
  }
  // Should the non-common part of item be appended? Compute the index 'rest'
  // from which to append it.
  size_t rest =
    //
    // If non-common part starts with '/', append the part after the '/';
    // rel_path already ends in '/' or is empty, so the leading '/' is not
    // needed.
    item_path[common_size] == '/' ? common_size+1 :
    //
    // If item path is '.', do not append anything. In this case, common is 0.
    item_path[common_size] == '.' && !item_path[common_size+1] ? common_size+1
    //
    // Maybe the directory ends in a '/', so 'common' already points beyond the
    // leading slash of item. 
    : common_size;
  if (rest == item_path.size()) {
    // Nothing to append;  remove trailing /.
    assert(rel_path.size() > 1);
    rel_path.pop_back();
  } else {
    // Append
    rel_path.append(item_path, rest);
  }
  trace("rel path: " << rel_path);
  return rel_path;
}

bool base::is_nested_in(
  const std::string &item_path,
  const std::string &dir_path
)
{
  trace("is nested_in [" << item_path << "] [" << dir_path << "]");
  //
  // Special case: if item path is absolute but dir is relative or vice versa,
  // we cannot determine nesting, so return false. However, there is no need to
  // check for this special case, as the next check will also fail.
  //
  // If given path does not start with dir path, it cannot be nested.
  if (strncmp(item_path.data(), dir_path.data(), dir_path.length())) {
    return false;
  }
  // We need to check that there is a / in item path after dir path. Example:
  //
  //  dir path: /foo      (length=4)
  // item path: /foos/bar (no / at position 4 => not nested)
  // item path: /foo/bar  (/ at position 4 => nested)
  //
  char next = item_path[dir_path.length()];
  if (next == '/' || next == 0) {
    return true;
  }
  // There is a special case for the top directory: it ends in '/', so a slash
  // was already matched
  //
  // dir path: /         (length=1)
  //     path: /foo      (nested, but no / at position 1)
  //
  // On Windows, there is a similar case. Note that due to the code above,
  // we are sure at this point that the drive letters are equal:
  //
  // dir path: C:/       (length=3)
  //     path: C:/foo    (nested, but no / at position 3)
  //
  // Note that these are the only cases where a normalized directory ends in /;
  // in all other cases, a trailing / is removed by normalization. This gives us
  // an easy check that is platform independent.
  //
  // We can only check the last character of the dir path if the path is
  // non-empty, so we should either assert that it is non-empty (or, stronger,
  // that it is normalized), or check for that case. For now, we check. An empty
  // path normalizes to top directory, so we handle it as the top directory.
  return dir_path.empty() || dir_path.back() == '/';
}

bool base::is_strictly_nested_in(
  const std::string &item_path,
  const std::string &dir_path
)
{
  trace("is_strictly_nested_in [" << item_path << "] [" << dir_path << "]");
  if (strncmp(item_path.data(), dir_path.data(), dir_path.length())) {
    return false;
  }
  if (item_path.size() == dir_path.size()) {
    return false;
  }
  return item_path[dir_path.size()] == '/'
    || (!dir_path.empty() && dir_path.back() == '/');
}

// Careful when enabling tracing: this function is called in other tracing
// output so may cause deadlock when both tracings are enabled.
std::string base::get_natural_path(
  const std::string &given_path,
  const std::string &dir_path
)
{
  trace_nest("get natural path of '" << given_path
    << "' in '" << dir_path << "'"
  );
  std::string item_path = get_absolute_path(given_path, dir_path);
  // Both paths are absolute, or at least relative to the same directory.
  if (is_nested_in(item_path, dir_path)) {
    trace("item is nested in dir");
    if (item_path.length() == dir_path.length()) {
      return ".";
    }
    // Top directory can have a trailing slash, other directories not.  If there
    // is no trailing slash, we still need to drop a slash from the item path,
    // so add 1.
    //
    // Example 1 (no trailing slash):
    //
    //  dir path: C:/foo      (size is 6)
    // item path: C:/foo/bar  (need to drop 7 chars from front of item path)
    //
    // Example 1 (trailing slash):
    //
    //  dir path: C:/         (size is 3)
    // item path: C:/foo/bar  (need to drop 3 chars from front of item path)
    //
    bool trailing_slash = !dir_path.empty() && dir_path.back() == '/';
    return item_path.substr(dir_path.length() + (trailing_slash ? 0 : 1));
  }
  // Not nested, so return given path.
  trace("item is not nested in dir");
  return item_path;
}

std::string base::get_normalized_path(const std::string &path)
{
  return os::get_normalized_path(path);
}

bool base::is_normalized_path(const std::string &path)
{
  return os::is_normalized_path(path);
}

size_t base::get_parent_path_size(const std::string &path)
{
  const char *begin = path.data();
  const char *end = strrchr(begin, '/');
  return end-begin;
}

std::string base::get_parent_path(const std::string &path)
{
  return path.substr(0, get_parent_path_size(path));
}

std::string_view base::get_directory_prefix(std::string_view path)
{
  size_t slash = path.rfind('/');
  return slash == path.npos ? "" : path.substr(0, slash+1);
}

std::string_view base::get_leaf_name(std::string_view path)
{
  size_t slash = path.rfind('/');
  return slash == path.npos ? path : path.substr(slash+1);
}

size_t base::common_ancestor_size(
  const std::string &path1,
  const std::string &path2
)
{
  size_t i = 0;
  while (path1[i] == path2[i]) {
    if (!path1[i]) {
      return i;
    }
    ++i;
  }
  if (!path1[i] && path2[i] == '/') {
    return i;
  }
  if (!path2[i] && path1[i] == '/') {
    return i;
  }
  if (i) {
    if (path1[i] == '/' && path2[i] == '/') {
      return i;
    }
    do {
      --i;
      if (path1[i] == '/') {
        assert(path2[i] == '/');
        return i;
      }
    } while (i);
  }
  return 0;  
}

std::string base::quote_command_line(const char *args[])
{
  return base::os::quote_command_line(args);
}
  
std::string base::quote_command_line(const std::vector<const char*> &args)
{
  std::vector<const char *> cmd;
  cmd.reserve(args.size()+1);
  for (auto arg: args) {
    cmd.push_back(arg);
  }
  cmd.push_back(0);
  return base::os::quote_command_line(cmd.data());
}
  
std::string base::quote_command_line(const std::vector<std::string> &args)
{
  // Careful using "trace" in this function: it is used in other "trace" calls
  // so may cause a deadlock.
  std::vector<const char *> cmd;
  cmd.reserve(args.size()+1);
  for (auto const &arg: args) {
    cmd.push_back(arg.data());
  }
  cmd.push_back(0);
  return base::os::quote_command_line(cmd.data());
}

std::string base::quote_command_line(const std::vector<std::string_view> &args)
{
  // Careful using "trace" in this function: it is used in other "trace" calls
  // so may cause a deadlock.
  std::vector<std::string> cmd;
  cmd.reserve(args.size());
  for (auto const &arg: args) {
    cmd.emplace_back(arg);
  }
  return base::quote_command_line(cmd);
}  

std::string base::quote_command_arg(const std::string &filename)
{
  // Careful using "trace" in this function: it is used in other "trace" calls
  // so may cause a deadlock.
  return quote_command_arg(filename.data());
}

std::string base::quote_command_arg(std::string_view filename)
{
  // Careful using "trace" in this function: it is used in other "trace" calls
  // so may cause a deadlock.
  return quote_command_arg(std::string(filename));
}

std::string base::quote_command_arg(const char *filename)
{
  // Careful using "trace" in this function: it is used in other "trace" calls
  // so may cause a deadlock.
  return base::os::quote_command_arg(filename);
}

size_t base::unquote_command_word(
  std::string &word,
  std::string_view line,
  size_t pos
)
{
  trace_nest("parse command word at " << pos << " in: " << line);
  word.clear();
  pos = line.find_first_not_of(" \t\v\n", pos);
  trace("word at " << pos);
  if (pos != line.npos && line[pos] == '#') {
    return line.npos;
  }
  const char escape = '\\';
  while (pos < line.size() && !std::isspace(line[pos])) {
    if (line[pos] == '"' || line[pos] == '\'') {
      char quote = line[pos];
      trace_nest("skip " << quote << "-quoted segment at: "<< line.substr(pos));
      pos += 1;
      for (;;) {
        if (line[pos] == quote) {
          pos += 1;
          break;
        }
        if (line[pos] == escape && pos+1 < line.size()
          && (line[pos+1] == quote || line[pos+1] == escape )
        ) {
          pos += 1;
        }
        word.push_back(line[pos]);
        pos += 1;
        if (pos == line.size()) break;
      }
    } else {
      word.push_back(line[pos]);
      pos += 1;
    }
  }
  trace("word: " << word);
  return pos;
}

#ifdef SELFTEST

struct TestData {
  const char *path;
  const char *dir;
  const char *rel_path;
  const char *abs_path;
  bool nested;
  bool strictly_nested;
};

std::ostream &operator<<(std::ostream &out, TestData const &data)
{
  out << data.path << " in " << data.dir;
  return out;
}

static void test(const TestData &data)
{
  trace_nest(data);
  assert(data.nested || !data.strictly_nested);

  if (data.rel_path) {
    auto rel_path = base::get_relative_path(data.path, data.dir);
    assert_(rel_path == data.rel_path,
      data << "\nRel path failed\nExpect: " << data.rel_path
      << "\nActual: " << rel_path
    );
  }
  auto abs_path = base::get_absolute_path(data.path, data.dir);
  assert_(abs_path == data.abs_path,
    data << "\nAbs path failed\nExpect: " << data.abs_path
    << "\nActual: " << abs_path
  );
  bool nested = base::is_nested_in(data.path, data.dir);
  assert_(nested == data.nested,
    data << "\nNesting failed\nExpect: " << data.nested
    << "\nActual: " << nested
  );
  bool strictly_nested = base::is_strictly_nested_in(data.path, data.dir);
  assert_(strictly_nested == data.strictly_nested,
    data << "\nStrict nesting failed\nExpect: " << data.strictly_nested
    << "\nActual: " << strictly_nested
  );
  auto nat_path = base::get_natural_path(data.path, data.dir);
  auto expected_nat_path =
    base::is_nested_in(abs_path, data.dir)
    ? base::get_relative_path(abs_path, data.dir) : abs_path;
  assert_(nat_path == expected_nat_path,
    data << "\nNat path failed\nExpect: "
    << expected_nat_path << "\nActual: " << nat_path
  );
}

int main(int argc, char *argv[])
{
  std::cout << "Hello" << std::endl;

  std::cout << "Testing signatures ...\n";
  std::ofstream out;
  const std::string paths[] = {
    "cache/ddd/x.c",
    "cache/xx/x.c",
    "cache/xx/y/x.c",
  };
  uint64_t signatures[sizeof(paths)/sizeof(*paths)];
  std::cout << base::get_signature("/usr/include") << std::endl;
  for (size_t i = 0; i < sizeof(paths)/sizeof(*paths); i++) {
    base::open_for_writing(out, paths[i]);
    out << paths[i];
    out.close();
    assert_(base::is_file(paths[i]), paths[i]);
    base::os::file_meta_data data;
    if (!base::os::get_file_meta_data(paths[i].data(), data)) {
      std::cout << "cannot get meta data for " << paths[i] << std::endl;
    } else {
      std::cout << data.mtime_secs << " " << data.mtime_nsecs
                << " " << data.size_bytes << std::endl;
    }
    signatures[i] = base::get_signature(paths[i]);
    std::cout << signatures[i] << " " << paths[i] << std::endl;
    for (size_t j = 0; j < i; ++j) {
      assert(signatures[i] != signatures[j]);
    }
  }
  for (size_t i = 0; i < sizeof(paths)/sizeof(*paths); i++) {
    assert(signatures[i] == base::get_signature(paths[i]));
  }
  for (int i = 1; i < argc; ++i) {
    std::cout << base::get_signature(argv[i]) << " " << argv[i] << std::endl;
  }
  std::cout << "Testing signatures done\n";

  std::cout << "Testing nesting ...\n";
  assert( base::is_nested_in("/xxx/Windows",     "/xxx/Windows"));
  assert( base::is_nested_in("/xxx/Windows/foo", "/xxx/Windows"));
  assert(!base::is_nested_in("/xxx/Windowsfoo",  "/xxx/Windows"));
  assert(!base::is_nested_in("/xxx/windows/foo", "/xxx/Windows"));
  std::cout << "Testing nesting done\n";
  
  std::cout << "Testing path normalization ...\n";
  switch (base::os::get_os()) {
    case base::os::OS_windows:
      assert_(base::get_normalized_path("c:\\foo/bar/..\\./..") == "C:/",
        base::get_normalized_path("c:\\foo/bar/..\\./..")
      );
      assert_(base::get_normalized_path("c:/foo/bar//..//") == "C:/foo",
        base::get_normalized_path("c:/foo/bar//..//")
      );
      break;
    case base::os::OS_linux:
      assert(base::get_normalized_path("/foo//bar/.././..//") == "/");
      assert(base::get_normalized_path("/foo/bar//..//") == "/foo");
      break;
  }
  const char *normalized[] = {
    "../../../../embeetle/beetle_core/source_analyzer/test/error.c"
  };
  for (auto path: normalized) {
    assert_(base::is_normalized_path(path),
      "'" << path << "' <> '" << base::get_normalized_path(path) << "'"
    );
  }
  std::cout << "Testing path normalization done\n";
  
  std::cout << "Testing parent path ...\n";
  assert(base::get_parent_path("/foo/bar/x.c") == "/foo/bar");
  assert(base::get_parent_path("/foo/x.c") == "/foo");
  assert(base::get_parent_path("/foo/") == "/foo");
  assert(base::get_parent_path("/") == "");
  assert(base::get_parent_path("") == "");
  std::cout << "Testing parent path done\n";

  std::cout << "Testing common ancestor size ...\n";
  assert(base::common_ancestor_size("foo/bar","foo/blob") == 3);
  assert(base::common_ancestor_size("foo/bar","fooo/blob") == 0);
  assert(base::common_ancestor_size("/foo/bar","/fooo/blob") == 0);
  assert(base::common_ancestor_size("/foo/bar","/foo/bar/bob") == 8);
  assert(base::common_ancestor_size("/foo/bar","/foo/bar") == 8);
  std::cout << "Testing common ancestor size done\n";

  std::cout << "Testing abs/rel/nat path ...\n";
  TestData test_data[] = {
    { "/foo/bar",   "/foo",         "bar",       "/foo/bar",    true,  true  },
    { "/foo/bar",   "/foo/bar",     ".",         "/foo/bar",    true,  false },
    { "/foo/bar",   "/foo/dot",     "../bar",    "/foo/bar",    false, false },
    { "/foo/bar",   "/foo/dot/zot", "../../bar", "/foo/bar",    false, false },
    { "/foo",       "/foo/bar",     "..",        "/foo",        false, false },
    { "/",          "/foo/bar",     "../..",     "/",           false, false },
    { "dot",        "foo/bar",      "../../dot", "foo/bar/dot", false, false },
    { "foo",        "foo/bar",      "..",        "foo/bar/foo", false, false },
    { "..",         "foo/bar",      "../../..",  "foo",         false, false },
    { "..",         "foo",          "../..",     ".",           false, false },
    { "bar",        "/foo",         "bar",       "/foo/bar",    false, false },
    { "/bar",       "foo",          "/bar",      "/bar",        false, false },
    { "/foo",       "D:/bar",       "/foo",      "/foo",        false, false },
    { "../foo/dot", "/bar",         "../foo/dot","/foo/dot",    false, false },
    { "../bar/dot", "/bar",         "../bar/dot","/bar/dot",    false, false },
    { ".",          "/bar",         ".",         "/bar",        false, false },
    { ".",          "dot",          "..",        "dot",         false, false },
    { "..",         "dot",          "../..",     ".",           false, false },
    { "/bar",       "/bar",         ".",         "/bar",        true,  false },
    { "dot",        "dot",          ".",         "dot/dot",     true,  false },
    { "/",          "/",            ".",         "/",           true,  false },
  };
  for (TestData &data: test_data) {
    test(data);
  }
  switch (base::os::get_os()) {
    case base::os::OS_windows: {
      TestData test_data[] = {
        { "C:/foo",     "C:/",      "foo",     "C:/foo",        true,  true  },
        { "C:/foo",     "C:/bar",   "../foo",  "C:/foo",        false, false },
        {
          "C:/msys64/home/Johan/work/src/foo.cpp",
          "C:/msys64/home/Johan/work/bld",
          "../src/foo.cpp",
          "C:/msys64/home/Johan/work/src/foo.cpp",
          false, false
        },
        { "C:/foo/bar", "C:/foo",   "bar",     "C:/foo/bar",    true,  true  },
        { "C:/foo",     "C:/",      "foo",     "C:/foo",        true,  true  },
        { "C:/foo",     "/bar",     "C:/foo",  "C:/foo",        false, false },
        { "C:/foo",     "D:/bar",   "C:/foo",  "C:/foo",        false, false },
      };
      for (TestData &data: test_data) {
        test(data);
      }
      break;
    }
    case base::os::OS_linux: {
      TestData test_data[] = {
        { "/foo",       "/",        "foo",     "/foo",           true,  true },
        { "C:/foo",     "C:/bar",   "../foo",  "C:/bar/C:/foo", false, false },
        {
          "/home/Johan/work/src/foo.cpp",
          "/home/Johan/work/bld",
          "../src/foo.cpp",
          "/home/Johan/work/src/foo.cpp",
          false, false
        },
        { "/var/foo",   "/var/bar", "../foo",  "/var/foo",      false, false },
        { "C:/foo",     "/bar",     "C:/foo",  "/bar/C:/foo",   false, false },
        { "C:/foo",     "D:/bar","../../C:/foo","D:/bar/C:/foo",false, false },
      };
      for (TestData &data: test_data) {
        test(data);
      }
      break;
    }
  }
  std::cout << "Testing abs/rel/nat path done\n";

  std::string command_line = "foo 'bar'z\"t\\\"op\" # and a comment";
  std::string word;
  size_t pos = 0;
  pos = base::unquote_command_word(word, command_line, 0);
  assert_(pos == 3, pos);
  assert_(word == "foo", word);
  pos = base::unquote_command_word(word, command_line, pos);
  assert_(pos == 17, pos);
  assert_(word == "barzt\"op", word);
  pos = base::unquote_command_word(word, command_line, pos);
  assert_(pos == command_line.npos, pos);
  command_line = " \tfoo' '  ";
  pos = base::unquote_command_word(word, command_line, 0);
  assert_(pos == 8, pos);
  assert_(word == "foo ", word);
  pos = base::unquote_command_word(word, command_line, pos);
  assert_(pos == command_line.npos, pos);
  std::cout << "Bye" << std::endl;
}

#endif
