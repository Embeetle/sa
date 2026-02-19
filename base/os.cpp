// Copyright 2018-2023 Johan Cockx
#include "os.h"
#include "string_util.h"
#include "filesystem.h"
#include <sstream>
#include <sys/stat.h>
#include <string>
#include <ctype.h>
#include <error.h>
#include <thread>
#include <filesystem>
#include <cstring>

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
#define WINDOWS 1
#else
#undef WINDOWS
#endif

#ifdef WINDOWS

#include <windows.h>
#include <cstdint>
#include <thread>
#include <io.h>
#include <fstream>
#include <ext/stdio_filebuf.h>
#include <strsafe.h>

#else

#include <cstring>
//#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <err.h>

#endif

#include "debug.h"

const char *base::os::getenv(const char *name)
{
  return ::getenv(name);
}

base::os::OS base::os::get_os()
{
#ifdef WINDOWS
  return OS_windows;
#else
  return OS_linux;
#endif
}

const char *base::os::get_os_name()
{
#ifdef WINDOWS
  return "windows";
#else
  return "linux";
#endif
}

const char *base::os::get_exe_extension()
{
#ifdef WINDOWS
  return ".exe";
#else
  return "";
#endif
}

std::string_view base::os::strip_exe_extension(std::string_view word)
{
#ifdef WINDOWS
  if (word.size() >= 4 && !strcasecmp(word.data()+word.size()-3, "exe")) {
    return word.substr(0,word.size()-4);
  }
#endif
  return word;
}

const char *base::os::get_null_device_path()
{
#ifdef WINDOWS
  return "NUL";
#else
  return "/dev/null";
#endif
}

// The directory for temporary files. Defaults to the directory provided by the
// OS for this purpose. Can be overridden for testing purposes.
static std::string temp_directory_path = base::get_normalized_path(
  std::filesystem::temp_directory_path().string()
);

const char *base::os::get_temp_directory_path()
{
  return temp_directory_path.data();
}

void base::os::set_temp_directory_path(const char *path)
{
  temp_directory_path = path;
}

bool base::os::starts_with_path(
  const char *data, const char *path, size_t path_size
)
{
  while (path_size) {
    if (*data != *path
#ifdef WINDOWS
      && !(*data == '\\' && *path == '/')
#endif
    ) {
      return false;
    }
    ++data;
    ++path;
    --path_size;
  }
  return true;
}

void base::os::normalize_line_endings(std::string &data)
{
#ifdef WINDOWS
  remove_char(data, '\r');
#else
  (void)data;
#endif
}

std::istream &base::os::getline(std::istream &input, std::string &line)
{
  std::istream &result = std::getline(input, line);
#ifdef WINDOWS
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
#endif
  return result;
}
  
bool base::os::get_file_meta_data(const char *path, file_meta_data &data)
{
  struct stat buffer;
  if (stat(path, &buffer)) {
    trace("get file data file-not-found " << path);
    return false;
  }
#ifdef WINDOWS
  data.mtime_secs = buffer.st_mtime;
  data.mtime_nsecs = 0;
#else
  const struct timespec &timestamp = buffer.st_mtim;
  data.mtime_secs = timestamp.tv_sec;
  data.mtime_nsecs = timestamp.tv_nsec;
#endif
  data.size_bytes = buffer.st_size;
  return true;
}

const char *base::os::get_extension(const char *path)
{
  const char *x = path + strlen(path);
  while (x > path && *x != '.' && *x != '/'
#ifdef WINDOWS
    && *x != '\\'
#endif
  ) x--;
  return *x == '.' ? x : "";
}

#ifdef WINDOWS
// strchrnul does not exist?
inline const char *strchrnul(const char *s, int c)
{
  const char *v = strchr(s, c);
  if (!v) v = s + strlen(s);
  return v;
}

// strcpy not accepted for safety reasons?
#undef strcpy
namespace base {
  namespace os {
    inline void strcpy(char *target, char const *source)
    {
      while ((*target++ = *source++));
    }
  }
}
#endif

std::string base::os::get_command_base_name(const char *path)
{
#ifdef WINDOWS
  trace_nest("get command base name for " << path);
  static const char *pathext = getenv("PATHEXT");
  if (pathext) {
    trace("PATHEXT=" << pathext);
    size_t path_len = strlen(path);
    const char *path_end = path + path_len;
    const char *ext = pathext;
    for (;;) {
      trace("check " << ext);
      const char *ext_end = strchrnul(ext, ';');
      size_t ext_len = ext_end - ext;
      if (!strncasecmp(path_end - ext_len, ext, ext_len)) {
        return std::string(path, path_len - ext_len);
      }
      if (!*ext_end) break;
      ext = ext_end + 1;
    }
  }
#endif
  return path;
}

void base::os::create_directory(const char *path)
{
  //unlink(path.data());
  trace("create directory " << path);
  // Note: mkdir returns 0 on success for all supported OS's. It returns
  // non-zero when the directory already exists. For our use case, where we
  // create directories on the path to a file we want to create, creating an
  // already existing directory is actually a success. It could occur because
  // another thread created the directory between the test for existance and the
  // call to create it. For this reason, we do not return a status from this
  // function, but let the application test afterwards whether the directory
  // exists.  This strategy can still fail if some threads remove directories
  // ...
#ifdef WINDOWS
  int rc = mkdir(path);
#else
  int rc = mkdir(path, -1);
#endif
  if (rc && errno != EEXIST) {
    debug_atomic_writeln("mkdir failed with code " << rc << " for " << path
      << ": " << strerror(errno)
    );
  }
}

#ifdef WINDOWS
static char get_drive_letter(const char *path)
{
  return *path;
}
#endif

bool base::os::has_drive_prefix(const char *path)
{
#ifdef WINDOWS
  return isalpha(path[0]) && path[1] == ':';
#else
  (void)path;
  return false;
#endif
}
  
bool base::os::has_drive_prefix(std::string_view path)
{
#ifdef WINDOWS
  return path.size() >= 2 && has_drive_prefix(path.data());
#else
  (void)path;
  return false;
#endif
}

size_t base::os::drive_prefix_size(const char *path)
{
#ifdef WINDOWS
  return has_drive_prefix(path) ? 2 : 0;
#else
  (void)path;
  return 0;
#endif
}

size_t base::os::drive_prefix_size(std::string_view path)
{
#ifdef WINDOWS
  return has_drive_prefix(path) ? 2 : 0;
#else
  (void)path;
  return 0;
#endif
}

bool base::os::in_same_tree(const char *path1, const char *path2)
{
  assert(is_absolute_path(path1));
  assert(is_absolute_path(path2));
#ifdef WINDOWS
  if (has_drive_prefix(path1)) {
    if (has_drive_prefix(path2)) {
      return get_drive_letter(path1) == get_drive_letter(path2);
    } else {
      return false;
    }
  } else {
    return !has_drive_prefix(path2);
  }
#else
  (void)path1;
  (void)path2;
  return true;
#endif
}
  
bool base::os::is_absolute_path(const char *path)
{
  assert(path);
  if (!*path) return true;
#ifdef WINDOWS
  if (has_drive_prefix(path) && (path[2] == '/' || path[2] == '\\')) return true;
#endif
  return path[0] == '/';
}

bool base::os::is_absolute_path(std::string_view path)
{
  if (path.size() == 0) return true;
#ifdef WINDOWS
  if (path.size() >= 3 && has_drive_prefix(path.data())
    && (path[2] == '/' || path[2] == '\\')
  ) return true;
#endif
  return path[0] == '/';
}

std::string base::os::get_absolute_path(
  const char *path,
  const std::string &directory_path
)
{
  if (is_absolute_path(path)) {
    return path;
  }
#ifdef WINDOWS
  if (has_drive_prefix(path)) {
    return directory_path + "/" + (path+2);
  }
#endif
  return directory_path + "/" + path;
}

#ifdef WINDOWS
#define pathcmp strcasecmp
#define pathncmp strncasecmp
#else
#define pathcmp strcmp
#define pathncmp strncmp
#endif

std::string base::os::get_normalized_path(const std::string &path)
{
  trace_nest("normalize path '" << path << "'");
  assert_(path.data(), "uninitialized string");
#ifdef WINDOWS
  std::string inpath = path;
  for (size_t i = 0; i < inpath.size(); i++) {
    if (inpath[i] == '\\') {
      inpath[i] = '/';
    }
  }
#else
  const std::string &inpath = path;
#endif
  const char *in = inpath.data();
  const char *end = in + inpath.size();
  trace("inpath size=" << inpath.size() << " data='" << in << "'");
  // Reserve enough space: we don't want re-allocations, because we will be
  // using pointer arithmetic.  Buf will not grow beyond inpath size, unless
  // inpath is empty.
  std::string buf(inpath.size()+1, '\0');
  char *out = buf.data();
#ifdef WINDOWS
  if (has_drive_prefix(in)) {
    *out++ = toupper(*in);
    *out++ = ':';
    trace("out buf: " << buf.data());
    in += 2;
  }
#endif
  // 'begin' points to the start of the output string without drive prefix.
  // It is used to handle special cases like a lonely '.' or '/'.
  char *const begin = out;
  // 'backup_stop' points to a position in the output string over which we
  // cannot backup when we find a '..'. The output string between 'begin' and
  // 'backup_stop' consists of zero or more repititions of '../'.
  char *backup_stop = begin;
  for (;;) {
    trace("in at " << (in-inpath.data()) << ": " << in);
    if (!*in) break;
    if (*in == '/') {
      assert(out == begin || out[-1] != '/');
      ++in;
      while (*in == '/') {
        trace("skip //");
        ++in;
      };
      if (in != end || out == begin) {
        trace("insert /");
        *out++ = '/';
        trace(" `-> out buf: " << buf.data());
      } else {
        trace("ignore trailing /");
      }
    } else {
      assert(out == begin || out[-1] == '/');
      const char *slash = strchrnul(in+1, '/');
      trace("next slash: " << slash);
      if (in[0] == '.') {
        if (slash == in+1) {
          // remove '.' logic is identical to remove '___/..' logic below
          in = slash;
          while (*in == '/') {
            trace("skip / after .");
            ++in;
          }
          if (!*in) {
            if (out == begin) {
              trace("keep lonely .");
              *out++ = '.';
            } else {
              trace("remove trailing /");
              --out;
            }
            break;
          }
          continue;
        } else if (in[1] == '.' && slash == in+2) {
          // Is there something to backup over? 
          if (out > backup_stop+1) {
            trace("backup for ___/..");
            out -= 2;
            while (out != begin && out[-1] != '/') {
              --out;
            }
            trace(" `-> out buf: " << std::string(buf.data(), out-begin));
            // remove '___/..' logic is identical to remove '.' logic above
            in = slash;
            while (*in == '/') {
              trace("skip / after ___/..");
              ++in;
            }
            if (!*in) {
              if (out == begin) {
                trace("keep lonely .");
                *out++ = '.';
              } else {
                trace("remove trailing /");
                --out;
              }
              break;
            }
            continue;
          } else {
            // There is nothing to backup over. '..' will be appended below.
            // Make sure it cannot be removed by a subsequent '..'.
            backup_stop += 3;
          }
        }
      }
      trace("copy upto slash: " << in);
      strncpy(out, in, slash-in);
      out += slash-in;
      trace(" `-> out buf: " << std::string(buf.data(), out-buf.data()));
      in = slash;
    }
  }
  if (out == begin) {
    trace("insert / for empty string");
    *out++ = '/';
  }
  assert_((size_t)(out - buf.data()) <= buf.size(), "normalize '" << path
    << "' " << (out - buf.data()) << " " << buf.size() << " '"
    << buf.data() << "'"
  );
  *out = '\0';
  trace("normalized: " << buf.data());
  return buf.data();
}

bool base::os::is_normalized_path(const std::string &path)
{
  // There are more efficient ways to implement this, but it is good enough for
  // now.
  return path == get_normalized_path(path);
}

void base::os::normalize_path_case(std::string &path)
{
#ifdef WINDOWS
  for (size_t i = drive_prefix_size(path.data()); i < path.size(); ++i) {
    path[i] = std::tolower(path[i]);
  }
#else
  (void)path;
#endif
}

bool base::os::path_has_different_case_on_disk(
  std::string_view raw_path,
  std::string &path_on_disk,
  std::string_view directory_path
)
{
  trace_nest("path_has_different_case_on_disk " << raw_path);
#ifdef WINDOWS
  std::string path = get_normalized_path((std::string)raw_path);
  trace("normalized path: '" << path << "'");
  // On Windows, we can only check the path case on disk level by level.
  //
  // "partial_path" is the path we have checked so far. It starts from the given
  // directory path, and adds subdirectories in the path to be checked one by
  // one until either the final level (directory or file) has been added. For
  // each level added, case on disk is checked.
  std::string partial_path(get_normalized_path((std::string)directory_path));
  //
  // "path_pos" is the position up to which "path" has been checked.
  size_t path_pos;
  //
  // "prefix_size" is the size of the partial path that must be skipped
  // when reporting the path on disk.
  size_t prefix_size = 0;
  if (directory_path.empty()) {
    // For an empty directory path, "path" might start with a drive letter.  We
    // are only checking for case differences in the path after the drive
    // letter, so skip the drive letter here.
    path_pos = drive_prefix_size(path);
    if (path_pos < path.size() && path[path_pos] == '/') path_pos += 1;
    partial_path = path.substr(0, path_pos);
    prefix_size = 0;
  } else {
    path_pos = 0;
    partial_path += "/";
    prefix_size = partial_path.size();
  }
  trace("initial_path: " << partial_path << " path-pos=" << path_pos);
  bool different = false;
  for (;;) {
    size_t next_pos = path.find('/', path_pos);
    std::string part = path.substr(path_pos, next_pos-path_pos);
    if (part == ".") {
      trace("skip '.'");
    } else if (part == "..") {
      trace("process '..': strip last part of " << partial_path);
      partial_path = get_normalized_path(partial_path + "..");
      prefix_size = partial_path.size();
    } else {
      partial_path += part;
      trace("partial_path: " << partial_path);
      WIN32_FIND_DATA data = {};
      HANDLE sh = FindFirstFileA(partial_path.data(), &data);
      if (sh == INVALID_HANDLE_VALUE) {
        // No such file or directory - just continue
      } else {
        FindClose(sh);
        for (size_t i = 0; i < part.size(); ++i) {
          if (part[i] != data.cFileName[i]) {
            different = true;
            partial_path[partial_path.size()-part.size()+i] = data.cFileName[i];
          }
        }
      }
    }
    if (next_pos == std::string_view::npos) break;
    partial_path += "/";
    path_pos = next_pos+1;
  }
  if (different) {
    path_on_disk = partial_path.substr(prefix_size);
    return true;
  }
#else
  (void)raw_path;
  (void)path_on_disk;
  (void)directory_path;
#endif
  return false;
}
  
namespace {
  static void aux(
    void (*handler)(std::istream&, void*),
    std::istream* stream,
    void *context
  )
  {
    handler(*stream, context);
  }

  // OS-specific type used for file handles.
#ifdef WINDOWS
  typedef HANDLE os_file_handle;
#else
  typedef int os_file_handle;
#endif

  // Stream buffer that reads from a OS-specific file handle.
  // Partial implementation: fixed buffer for input, output not implemented
  class os_streambuf: public std::streambuf
  {
    // Fixed buffer. TODO: allow user-settable buffer.
    char buffer[4096];
    os_file_handle handle;

  public:
    // Constructor from file handle.  The file must be open for reading and must
    // remain open while the buffer exists. It will not be closed automatically.
    os_streambuf(os_file_handle handle): handle(handle) {}

  protected:
    // Return current character without changing read position.  Called when
    // current character is not available in buffer, with the intention to fill
    // the buffer if possible. Otherwise, return eof.
    int underflow() override
    {
      assert(gptr() == egptr());
#ifdef WINDOWS
      DWORD nread = 0;
      if (!ReadFile(handle, buffer, (DWORD)sizeof(buffer), &nread, nullptr)) {
        // Error
        return traits_type::eof();
      }
#else
      ssize_t nread = read(handle, buffer, sizeof(buffer));
      if (nread == -1) {
        // errno set
        return traits_type::eof();
      }
#endif
      if (nread == 0) {
        // EOF
        return traits_type::eof();
      }
      setg(buffer, buffer, buffer + nread);
      return *buffer;
    }

    int overflow(int c)
    {
      // TODO: set buffer with room and/or flush buffer to make room
      return std::streambuf::overflow(c);
    }
  };

#ifdef WINDOWS

  class windows_streambuf: public std::streambuf
  {
    HANDLE handle;
    char buffer[4096];
    
  public:
    // Construct stream buffer that reads from a Windows file handle.  The file
    // must be open for reading and must remain open while the buffer exists. It
    // will not be closed automatically.
    windows_streambuf(HANDLE handle): handle(handle)
    {
    }

    ~windows_streambuf()
    {
    }

  protected:
    
    // Return current character without changing read position.  Called when
    // current character is not available in buffer, with the intention to fill
    // the buffer if possible. Otherwise, return eof.
    int underflow() override
    {
      assert(gptr() == egptr());

      DWORD nread = 0;
      if (!ReadFile(handle, buffer, (DWORD)sizeof(buffer), &nread, nullptr)) {
        // Error
        return traits_type::eof();
      } else if (nread == 0) {
        // EOF
        return traits_type::eof();
      }
      setg(buffer, buffer, buffer + nread);
      return *buffer;
    }

    int overflow(int c)
    {
      // TODO: set buffer with room and/or flush buffer to make room
      return std::streambuf::overflow(c);
    }
  };

#if 0
  // Get a string for the last Windows error.  The error message is typically
  // not very helpful (e.g. "The handle is invalid") so we currently don't use
  // this function.
  static std::string get_last_windows_error_message()
  {
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError(); 
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL
    );
    std::string message = (const char*) lpMsgBuf;
    LocalFree(lpMsgBuf);
    return message;
  }
#endif
#endif
}

std::string base::os::quote_command_arg(const char *filename)
{
  // Careful using "trace" in this function: it is used in other "trace" calls
  // so may cause a deadlock.
  std::stringstream buffer;
  const char *arg = filename;
  bool needs_quotes = arg[strcspn(arg, " \t()&|!;'*%$~#")];
  if (needs_quotes) {
    buffer.put('"');
  }
  const char *end = arg + strlen(arg);
  while (arg < end) {
    // Find first quote in arg. If none are found, write the remaining
    // string and exit the loop.
    const char *quote = strchr(arg, '"');
    if (!quote) {
      if (needs_quotes) {
        quote = end;
      } else {
        buffer << arg;
        break;
      }
    }
    // Move back over preceding backslashes.
    const char *backslash = quote;
    while (backslash > arg && backslash[-1] == '\\') {
      --backslash;
    }
    // Write arg up to first preceding backslash.
    while (arg < backslash) {
      buffer.put(*arg);
      ++arg;
    }
    // Write preceding backslashes.
    while (arg != quote) {
      buffer.put('\\');
      buffer.put('\\');
      ++arg;
    }
    // Write quote.
    if (quote < end) {
      buffer.put('\\');
      buffer.put('"');
      ++arg;
    }
  }
  if (needs_quotes) {
    buffer.put('"');
  }
  return buffer.str();
}

std::string base::os::quote_command_line(const char *args[])
{
  // Careful using "trace" in this function: it is used in other "trace" calls
  // so may cause a deadlock.
  std::stringstream buffer;

  // The command name - args[0] - has special escape rules on Windows:
  //  1. if it contains a space or control character,  it must be double-quoted
  //  2. there is no way to escape special characters " * : ? < > |
  //  3. if double-quoted,  a .exe extension must be explicitly added if there is
  //    no extension present
  // Rule 1 works on Linux too. 
  const char *command = args[0];
  assert(command);
  bool needs_quotes = false;
  for (const char *p = command; *p; ++p) {
    if (*p <= ' ') {
      needs_quotes = true;
      break;
    }
  }
  if (needs_quotes) {
    buffer.put('"');
  }
  buffer << command;
  if (needs_quotes) {
#ifdef WINDOWS
    // Check extension.  If there is none, add .exe.
    if (!*base::os::get_extension(command)) {
      buffer << ".exe";
    }
#endif
    buffer.put('"');
  }

  // A Parameter needs double quotes if it contains a space or tab or another
  // special character.  A double quote within the parameter must be escaped
  // with backslash.  One or more backslashes before a double quote - including
  // the final double quote if the whole parameter is quoted - must be escaped
  // with backslash.  No other characters may be escaped.
  for (unsigned i = 1; args[i]; ++i) {
    buffer.put(' ');
    const char *arg = args[i];
    bool needs_quotes = arg[strcspn(arg, " \t()&|!;'*%$~#")];
    if (needs_quotes) {
      buffer.put('"');
    }
    const char *end = arg + strlen(arg);
    while (arg < end) {
      // Find first quote in arg. If none are found, write the remaining
      // string and exit the loop.
      const char *quote = strchr(arg, '"');
      if (!quote) {
        if (needs_quotes) {
          quote = end;
        } else {
          buffer << arg;
          break;
        }
      }
      // Move back over preceding backslashes.
      const char *backslash = quote;
      while (backslash > arg && backslash[-1] == '\\') {
        --backslash;
      }
      // Write arg up to first preceding backslash.
      while (arg < backslash) {
        buffer.put(*arg);
        ++arg;
      }
      // Write preceding backslashes.
      while (arg != quote) {
        buffer.put('\\');
        buffer.put('\\');
        ++arg;
      }
      // Write quote.
      if (quote < end) {
        buffer.put('\\');
        buffer.put('"');
        ++arg;
      }
    }
    if (needs_quotes) {
      buffer.put('"');
    }
  }
  return buffer.str();
}

std::map<std::string, std::string> base::os::current_env()
{
  std::map<std::string, std::string> env;
  for (char *const*p = environ; *p; ++p) {
    char *epos = strchr(*p, '=');
    if (epos) {
      env[std::string(*p,0,epos-*p)] = epos+1;
    }
  }
  return env;
}

void *base::os::create_native_env(const std::map<std::string, std::string> &env)
{
#ifdef WINDOWS
  size_t size = 1;
  for (const auto &[key, value]: env) {
    size += key.size() + value.size() + 2;
  }
  trace("size=" << size);
  char *native_env = new char[size];
  size_t i = 0;
  for (const auto &[key, value]: env) {
    strcpy(native_env+i, key.data());
    i += key.size();
    native_env[i] = '=';
    ++i;
    strcpy(native_env+i, value.data());
    i += value.size() + 1;
  }
  trace("native env: " << native_env);
  trace("i=" << i);
  native_env[i] = 0;
  return native_env;
#else
  char **native_env = new char*[env.size()+1];
  size_t i = 0;
  for (const auto &[key, value]: env) {
    char *entry = new char[key.size() + value.size() + 2];
    strcpy(entry, key.data());
    entry[key.size()] = '=';
    strcpy(entry + key.size() + 1, value.data());
    native_env[i] = entry;
    ++i;
  }
  native_env[i] = 0;
  return native_env;
#endif
}

void base::os::destroy_native_env(void *native_env)
{
#ifdef WINDOWS
  delete [] (char*)native_env;
#else
  for (char **p = (char**)native_env; *p; ++p) {
    delete [] *p;
  }
  delete [] (char**)native_env;
#endif
}

std::string base::os::print_native_env(void *native_env)
{
  std::stringstream buf;
#ifdef WINDOWS
  for (char *p = (char*)native_env; *p; p += strlen(p) + 1) {
    buf << p << '\n';
  }
#else
  for (char **p = (char**)native_env; *p; ++p) {
    buf << *p << '\n';
  }
#endif
  return buf.str();
}

#ifdef WINDOWS

static std::vector<char> writable_string(const char* cstr)
{
  return std::vector<char>(cstr, cstr + std::strlen(cstr) + 1);
}

static std::vector<char> writable_string(const std::string &str)
{
  return writable_string(str.c_str());
}

#else

static void noop() {}

#endif

int base::os::execute_and_capture(
  const char *args[],
  const char *work_directory,
  void (*stdout_handler)(std::istream&, void*), void *stdout_context,
  void (*stderr_handler)(std::istream&, void*), void *stderr_context,
  int& exit_code,
  void *env
)
{
  trace_nest("enter execute_and_capture at " << work_directory << ": "
    << quote_command_line(args)
  );
  
  // Create pipes, start process and create os_streambuf's for pipes.
#ifdef WINDOWS
  //static std::mutex xac_mutex;
  //const std::lock_guard<std::mutex> lock(xac_mutex);
  if (!is_directory(work_directory)) {
    trace("not a directory: " << work_directory);
    exit_code = -1;
    return ENOTDIR;
  }
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  security_attributes.bInheritHandle = TRUE;
  security_attributes.lpSecurityDescriptor = nullptr;
  HANDLE stdout_rd = INVALID_HANDLE_VALUE;
  HANDLE stdout_wr = INVALID_HANDLE_VALUE;
  if (!CreatePipe(&stdout_rd, &stdout_wr, &security_attributes, 0)
    || !SetHandleInformation(stdout_rd, HANDLE_FLAG_INHERIT, 0)
  ) {
    trace("cannot create pipe 1");
    return EPIPE;
  }
  HANDLE stderr_rd = INVALID_HANDLE_VALUE;
  HANDLE stderr_wr = INVALID_HANDLE_VALUE;
  if (!CreatePipe(&stderr_rd, &stderr_wr, &security_attributes, 0)
    || !SetHandleInformation(stderr_rd, HANDLE_FLAG_INHERIT, 0)
  ) {
    trace("cannot create pipe 2");
    CloseHandle(stdout_rd);
    CloseHandle(stdout_wr);
    return EPIPE;
  }
  STARTUPINFO startup_info;
  ZeroMemory(&startup_info, sizeof(STARTUPINFO));
  startup_info.cb = sizeof(STARTUPINFO);
  startup_info.hStdInput = 0;
  startup_info.hStdOutput = stdout_wr;
  startup_info.hStdError = stderr_wr;
  startup_info.dwFlags |= STARTF_USESTDHANDLES;
  PROCESS_INFORMATION  process_info;
  ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));
  // String args to CreateProcess need to be zero-terminated non-const arrays!
  auto command_line = writable_string(quote_command_line(args));
  trace("command line: " << command_line.data());
  auto wd = writable_string(work_directory);
  auto success = CreateProcess(
    nullptr,              // No module name (use command line)
    command_line.data(),  // Command line with full path for command
    nullptr,              // Process handle not inheritable
    nullptr,              // Thread handle not inheritable
    TRUE,                 // Set handle inheritance to TRUE
    CREATE_NO_WINDOW,     // Creation flags: suppress flashing console windows
    env,                  // Use parent's environment block
    wd.data(),            // Start in this dir i.s.o. parent's working dir
    &startup_info,        // Pointer to STARTUPINFO structure
    &process_info         // Pointer to PROCESS_INFORMATION structure
  );
  if (!success) {
    DWORD rc = GetLastError();
    trace("CreateProcess failed");
    exit_code = -1;
    trace("Error code: " << rc);
    CloseHandle(stdout_wr);
    CloseHandle(stderr_wr);
    CloseHandle(stdout_rd);
    CloseHandle(stderr_rd);
    return rc;
  }
  CloseHandle(stdout_wr);
  CloseHandle(stderr_wr);
  CloseHandle(process_info.hThread);
  os_streambuf out_buffer(stdout_rd);
  os_streambuf err_buffer(stderr_rd);
#else
  int stdout_fds[2];
  int stderr_fds[2];
  if (pipe2(stdout_fds, O_CLOEXEC)) {
    // errno set
    trace("cannot create pipe 1");
    return errno;
  }
  if (pipe2(stderr_fds, O_CLOEXEC)) {
    // errno set
    trace("cannot create pipe 2");
    int pipe_errno = errno;
    close(stdout_fds[0]);
    close(stdout_fds[1]);
    return pipe_errno;
  }
  // Fork,  then exec in child.
  //
  // Errors in child process are hard to report, because we are in a different
  // process. When exec succeeds, it does not return.
  //
  // execvpe can fail with the following errno's:
  //
  // [E2BIG] - The number of bytes in the new process's argument list is larger
  //           than the system-imposed limit. This limit is specified by the
  //           sysctl(3) MIB variable KERN_ARGMAX.
  //
  // [EACCES] - Search permission is denied for a component of the path prefix.
  //          - The new process file is not an ordinary file.
  //          - The new process file mode denies execute permission.
  //          - The new process file is on a filesystem mounted with execution
  //            disabled (MNT_NOEXEC in <sys/mount.h>).
  //
  // [EFAULT] - The new process file is not as long as indicated by the size
  //            values in its header.
  //          - Path, argv, or envp point to an illegal address.
  //
  // [EIO]    - An I/O error occurred while reading from the file system.
  //
  // [ELOOP] - Too many symbolic links were encountered in translating the
  //           pathname. This is taken to be indicative of a looping symbolic
  //           link.
  //
  // [ENAMETOOLONG] - A component of a pathname exceeded {NAME_MAX}
  //           characters, or an entire path name exceeded {PATH_MAX}
  //           characters.
  //
  // [ENOENT] - The new process file does not exist.
  //
  // [ENOEXEC] - The new process file has the appropriate access permission, but
  //             has an unrecognized format (e.g., an invalid magic number in
  //             its header).
  //
  // [ENOMEM] - The new process requires more virtual memory than is allowed
  //            by the imposed maximum (getrlimit(2)).
  //          - malloc or friends fails
  //
  // [ENOTDIR] - A component of the path prefix is not a directory.
  //
  // [ETXTBSY] - The new process file is a pure procedure (shared text) file
  //             that is currently open for writing or reading by some process.
  //
  // In addition, the child process changes directory, which can also fail with
  // ENOENT. How can we report these errors?
  //
  // Do not call err() or exit(), because these will run destructors in the
  // child process, which will in general not work. To produce a readable
  // error message instead of the errno integer value, call warn(), or use
  // error(0,...) (see `man 3 error`). To exit, use _exit(code).
  //
  // To return the error code to the creating process, we have the following
  // options:
  //
  //   1. Use the exit code (8 bits max passed to wait(...) call) and risk
  //      confusion with exit codes returned by the command. We could decide to
  //      use the traditional codes (see below) to summarize any exec
  //      issues. Exec does not kill the process using signals, so codes >= 128
  //      are always from the started command.
  //
  //   2. Use stderr (or stdout) and start with a magic marker, that is
  //      hopefully never produced by the command itself. This would mean that
  //      the code reading stderr would always have to check for this magic
  //      marker first, but should not consume regular output. Can that be
  //      done?  Use a one byte magic marker and peek?
  //
  //   3. Create a separate pipe to return errno, and set the close-on-exec
  //      flag FD_CLOEXEC on it. Parent process will block until it reads
  //      either the exec errno or end-of-file.
  //
  // Traditional exit codes used to report exec issues are:
  //   126 permission denied / unable to execute
  //   127 command not found / PATH error
  //   128+n process killed by signal n
  // See https://itsfoss.com/linux-exit-codes/.
  //
  // For now we use these traditional codes.  Since we support starting in a
  // specified working directory, we add an extra code for use when changing the
  // working directory fails.  This means we can only distinguish three
  // different errors from the child process. Note that the windows
  // implementation currently detect the equivalent of 126 permission denied.
  //
  // Option 3 (separate pipe) is the most general solution, but even then, we
  // need to decide on a way to report the distinction between errors in
  // change-working-directory and in exec.
  //
  enum {
        CANNOT_CHANGE_WORK_DIRECTORY = 125,
        CANNOT_EXECUTE = 126,
        COMMAND_NOT_FOUND = 127,
  };
  const pid_t pid = fork();
  if (!pid) {
    // In child, try to exec command.
    //
    close(0);
    dup2(stdout_fds[1], 1);
    dup2(stderr_fds[1], 2);
    if (-1 == chdir(work_directory)) {
      error(0, errno, "cannot change directory to %s", work_directory);
      _exit(CANNOT_CHANGE_WORK_DIRECTORY);
    }
#if 0
    for (size_t i = 0; args[i]; ++i) {
      std::cerr << "#arg " << i << ": " << args[i] << "\n" << std::flush;
    }
#endif
    // The exec* functions expect arrays of 'char*' instead of 'const char*',
    // because they were designed in the early days of unix when const did not
    // exist. They do not actually modify the strings, at least for POSIX
    // systems, so it is safe to cast away the const'ness.
    execvpe(args[0], const_cast<char* const *>(args),
      reinterpret_cast<char* const *>(env)
    );
    // On success,  exec does not return. If we are here, there was an error.
    int exec_errno = errno;
    error_print_progname = noop;
    error(0, exec_errno, "cannot exec %s", args[0]);
    _exit(exec_errno == ENOENT ? COMMAND_NOT_FOUND : CANNOT_EXECUTE);
  }
  int fork_errno = errno;
  close(stdout_fds[1]);
  close(stderr_fds[1]);
  if (pid == -1) {
    // fork failed, errno set
    close(stdout_fds[0]);
    close(stderr_fds[0]);
    return fork_errno;
  }
  os_streambuf out_buffer(stdout_fds[0]);
  os_streambuf err_buffer(stderr_fds[0]);
#endif

  // Run threads to read pipes.
  {
    std::istream out_stream(&out_buffer);
    std::istream err_stream(&err_buffer);
    std::thread out_thread = std::thread(
      aux, stdout_handler, &out_stream, stdout_context
    );
    std::thread err_thread = std::thread(
      aux, stderr_handler, &err_stream, stderr_context
    );
    err_thread.join();
    out_thread.join();
  }

  // Wait for process to end, get exit code, cleanup.
#ifdef WINDOWS
  CloseHandle(stderr_rd);
  CloseHandle(stdout_rd);
  WaitForSingleObject(process_info.hProcess, INFINITE);
  {
    DWORD windows_exit_code;
    auto success = GetExitCodeProcess(process_info.hProcess, &windows_exit_code);
    assert(success);
    exit_code = windows_exit_code;
    trace("Exit code from process: " << exit_code);
  }
  CloseHandle(process_info.hProcess);
#else
  close(stderr_fds[0]);
  close(stdout_fds[0]);
  int rc;
  int status;
  do {
    rc = waitpid(pid, &status, 0);
  } while (rc == -1 && errno == EINTR);
  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
    trace("Parent got exit code " << exit_code);
    switch (exit_code) {
      case CANNOT_CHANGE_WORK_DIRECTORY:
        trace("CANNOT_CHANGE_WORK_DIRECTORY");
        exit_code = -1;
        return ENOTDIR;
      case COMMAND_NOT_FOUND:
        trace("COMMAND_NOT_FOUND");
        exit_code = -1;
        return ENOENT;
      case CANNOT_EXECUTE:
        trace("CANNOT_EXECUTE");
        exit_code = -1;
        return EPERM;
      default:
        break;
    };
  } else {
    // Process terminated by signal WTERMSIG(status).
    assert(WIFSIGNALED(status));
    trace("Child terminated by signal " << WTERMSIG(status));
    exit_code = 128 + WTERMSIG(status);
  }
#endif
  trace("command succesfully started, exit_code=" << exit_code);
  return 0;
}

const char *base::os::strerror(int error_number)
{
  return ::strerror(error_number);
}

unsigned base::os::get_tid()
{
#ifdef WINDOWS
  return (unsigned)pthread_self();
#else
  return ::syscall(__NR_gettid);
#endif
}

#ifdef SELFTEST

#include <string>
#include <iostream>


#ifdef WINDOWS_not_needed
extern char *_pgmptr;

static void err(int eval, const char *format, ...)
{
  fprintf(stderr, "%s: ", _pgmptr);
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fprintf(stderr, ": %s\n", strerror(errno));
  va_end(args);
  exit(eval);
}
#endif

static void report(const std::string &name, std::istream &in)
{
  std::string line;
  while (std::getline(in, line)) {
    std::cout << name << ": " << line << '\n';
  }
}

static void report_stdout(std::istream &in, void*)
{
  report("stdout", in);
}

static void report_stderr(std::istream &in, void*)
{
  report("stderr", in);
}

static void test_norm(const char *test, const char *expect)
{
  auto norm = base::os::get_normalized_path(test);
  assert_(norm == expect,
    "for " << test << " expected " << expect << " got " << norm
  );
  test_out(test << " correctly normalized to " << norm);
}

int main(int argc, char *argv[])
{
  std::cout << "Hello" << std::endl;
  (void)argc;
  (void)argv;
#define check_extension(path, extension) \
  assert_(base::get_extension(path) == extension, base::get_extension(path));
  check_extension("a/b.c", ".c");
  check_extension("a/b", "");
  check_extension("a.b/c", "");
  check_extension("a.b/c.d", ".d");
  check_extension("/a.b/c.d", ".d");
  check_extension("/a.b/c..d", ".d");
  check_extension("/a.b/c..", ".");
  check_extension("/", "");
#ifdef WINDOWS
  check_extension("/a.b/c.\\d", "");
  check_extension("a.b\\c", "");
#else
  check_extension("/a.b/c.\\d", ".\\d");
  check_extension("a.b\\c", ".b\\c");
#endif

  // test quote_command_line(...)
  struct {
    const char *args[10];
    const char *command;
  } tests[] = {
    { { "ls", "-l", ".", "/foo", 0 }, "ls -l . /foo" },
    { { "/bin/with space/ls", "-l", ".", "/foo", 0 },
#ifdef WINDOWS
      "\"/bin/with space/ls.exe\" -l . /foo"
#else
      "\"/bin/with space/ls\" -l . /foo"
#endif
    },
    { { "ls", "-l", ".", "/foo\"bar", 0 },
      "ls -l . /foo\\\"bar"
    },
    { { "ls", "-l", ".", "/foo \"bar", 0 },
      "ls -l . \"/foo \\\"bar\""
    },
    { { "ls", "-l", ".", "/foo\\\"bar", 0 },
      "ls -l . /foo\\\\\\\"bar"
    },
    { { "ls", "-l", ".", "/foo \"bar\\", 0 },
      "ls -l . \"/foo \\\"bar\\\\\""
    },
    { { "clang", "-D__INTMAX_C(c)=c ## LL", "-D__FLT32_MIN_EXP__=(-125)", 0 },
      "clang \"-D__INTMAX_C(c)=c ## LL\" \"-D__FLT32_MIN_EXP__=(-125)\""
    },
  };
  for (unsigned i = 0; i < sizeof(tests)/sizeof(*tests); ++i) {
    auto test = tests[i];
    std::string command = base::os::quote_command_line(test.args);
    std::cout << "    test " << i << ": " << command << std::endl;
    assert_(command == test.command, "expected " << i << ": " << test.command);
  }
  
  struct {
    const char *args[10];
    const char *work_directory;
    int exit_code;
  } commands[] = {
#ifdef WINDOWS
    { { "C:\\Windows\\System32\\ipconfig", 0 }, ".", 0},
    { { "dir", "C:\\", 0 }, ".", 0},
    { { "dir", "C:\\Program Files (x86)", 0 }, "..", 0},
    { { "echo", "\"quoted string\"", 0 }, "..", 0},
    { { "dir", "C:\\Program Files (x86)", "foobar", 0 }, "..", 2},
    //{ { "timeout", "/t", "30", "/nobreak", 0 }, ".", 0},
    //{ { "python", "foo.py", 0 }, ".", 0},
    //{ { "notepad", 0 }, ".", 0},
#else
    // Don't try `df` as this can legally fail with an error like "df:
    // /run/user/1000/doc: Operation not permitted"
    // Also Don't try `ifconfig` as it is not necessarily installed in docker
    { { "ps", "-ef", 0 }, ".", 0},
    { { "ls", "-l", ".", "/foo", 0 }, "/tmp", 2},
#endif
  };
  for (unsigned i = 0; i < sizeof(commands)/sizeof(*commands); ++i) {
    auto command = commands[i];
#ifdef WINDOWS
    std::cout << "Windows command line: "
              << base::os::quote_command_line(command.args) << "\n";
#endif
    int exit_code = 0;
    int rc = base::os::execute_and_capture(command.args, command.work_directory,
      report_stdout, 0, report_stderr, 0, exit_code, 0
    );
    assert_(!rc, "execute_and_capture failed: " << strerror(rc)
      << " for " << command.args[0] << " at "
      << command.work_directory
    );
    assert_(exit_code == command.exit_code,
      "expected " << command.exit_code << " got " << exit_code
    );
    std::cout << "Command returned exit code " << exit_code
              << " as expected" << std::endl;
  }

#ifdef WINDOWS
  const char *compiler = "bin/avr-gcc.EXE";
#else
  const char *compiler = "bin/avr-gcc";
#endif
  std::string base = base::os::get_command_base_name(compiler);
  std::cout << "base name: " << base << std::endl;
  assert(base == "bin/avr-gcc");
#ifdef WINDOWS
  assert(base::os::get_command_base_name("bin/avr-gcc.exe") == "bin/avr-gcc");
  assert(base::os::get_command_base_name("bin/avr-gcc.cMd") == "bin/avr-gcc");
#endif

  test_norm("", "/");
  test_norm(".", ".");
  test_norm("./", ".");
  test_norm(".//", ".");
  test_norm(".///", ".");
  test_norm("./foo", "foo");
  test_norm("foo/.", "foo");
  test_norm("foo/bar", "foo/bar");
  test_norm("foo//bar", "foo/bar");
  test_norm("foo///bar", "foo/bar");
  test_norm("/", "/");
  test_norm("//", "/");
  test_norm("///", "/");
  test_norm("/.", "/");
  test_norm("/foo", "/foo");
  test_norm("/..", "/..");
  test_norm("/../", "/..");
  test_norm("/../foo", "/../foo");
  test_norm("foo/..", ".");
  test_norm("./..", "..");
  test_norm("../foo", "../foo");
  test_norm("foo/..", ".");
  test_norm("foo/../bar", "bar");
  test_norm("/./foo", "/foo");
  test_norm("project", "project");
  test_norm("/foo/.//bar/../dot//", "/foo/dot");
  test_norm("foo/bar/top/../x/../..","foo");
  test_norm("/foo/bar//.//", "/foo/bar");
  test_norm("foo/bar//.//", "foo/bar");
  test_norm("/foo/bar//..//", "/foo");
  test_norm("foo/bar//..//", "foo");
  test_norm("foo/../bar", "bar");
  test_norm("/foo/../bar", "/bar");
  test_norm("/foo/./", "/foo");
  test_norm("/foo/./.", "/foo");
  test_norm("/foo/./..", "/");

  switch (base::os::get_os()) {
    case base::os::OS_windows:
      test_norm("c:\\foo/bar/..\\./..", "C:/");
      test_norm("c:/foo/bar//..//", "C:/foo");
      break;
    case base::os::OS_linux:
      test_norm("/foo//bar/.././..", "/");
      test_norm("/foo/bar//..//", "/foo");
      break;
  }

#ifdef WINDOWS
  std::cout << "test path_has_different_case_on_disk\n";
  std::string path_on_disk;
  std::string path = __FILE__;
  bool dif;

  std::cout << "Path: " << path << "\n";

  std::string tail = "os.cpp";
  std::string head = path.substr(0,path.length()-tail.length()-1);
  dif = base::os::path_has_different_case_on_disk(tail, path_on_disk, head);
  assert_(!dif, path_on_disk);
  dif = base::os::path_has_different_case_on_disk("OS.cpp", path_on_disk, head);
  assert(dif);
  assert_(path_on_disk == "os.cpp", path_on_disk);
  dif = base::os::path_has_different_case_on_disk("../os.cpp", path_on_disk,
    head + "/foo");
  assert_(!dif, path_on_disk);
  dif = base::os::path_has_different_case_on_disk(".\\os.cpp", path_on_disk,
    head);
  assert_(!dif, path_on_disk);
  dif = base::os::path_has_different_case_on_disk(path, path_on_disk, "");
  assert_(!dif, path_on_disk);
  path[path.size()-11] = 'B';
  dif = base::os::path_has_different_case_on_disk(path, path_on_disk, "");
  assert(dif);
  assert_(path_on_disk == __FILE__, path_on_disk);
  path[path.size()-11] = 'X';
  dif = base::os::path_has_different_case_on_disk(path, path_on_disk, "");
  assert_(!dif, path_on_disk);
  dif = base::os::path_has_different_case_on_disk(path, path_on_disk, "/X");
  assert_(!dif, path_on_disk);
#endif

  const char *normalized[] = {
    "../../../../embeetle/beetle_core/source_analyzer/test/error.c"
  };
  for (auto path: normalized) {
    assert_(base::os::is_normalized_path(path),
      "'" << path << "' <> '" << base::os::get_normalized_path(path) << "'"
    );
  }

  std::map<std::string,std::string> test_env;
  test_env["FOO"] = "foo";
  test_env["BAR"] = "bar";
  void *native_env = base::os::create_native_env(test_env);
  assert_(base::os::print_native_env(native_env) == "FOO=foo\nBAR=bar\n"
    || base::os::print_native_env(native_env) == "BAR=bar\nFOO=foo\n",
    base::os::print_native_env(native_env)
  );
  auto env = base::os::current_env();
  std::cout << env.size() << " env vars\n";
  std::cout << "LANGUAGE = " << env["LANGUAGE"] << "\n";
  {
    int exit_code = 0;
    const char *args[] = { "make", "foo", 0 };
    {
      void *native_env = base::os::create_native_env(env);
      int rc = base::os::execute_and_capture(args, ".",
        report_stdout, 0, report_stderr, 0, exit_code, native_env
      );
      std::cout << "rc=" << rc << " exit code=" << exit_code << "\n";
      base::os::destroy_native_env(native_env);
    }
    env["LANGUAGE"] = "en_US";
    {
      void *native_env = base::os::create_native_env(env);
      int rc = base::os::execute_and_capture(args, ".",
        report_stdout, 0, report_stderr, 0, exit_code, native_env
      );
      std::cout << "rc=" << rc << " exit code=" << exit_code << "\n";
      base::os::destroy_native_env(native_env);
    }
    env["LANGUAGE"] = "de_DE";
    env["FOO"] = "foo";
    {
      void *native_env = base::os::create_native_env(env);
      int rc = base::os::execute_and_capture(args, ".",
        report_stdout, 0, report_stderr, 0, exit_code, native_env
      );
      std::cout << "rc=" << rc << " exit code=" << exit_code << "\n";
      base::os::destroy_native_env(native_env);
    }
  }
  {
    std::cout << "Test error return for non-existing working dir\n";
    int exit_code = 0;
    const char *args[] = { "make", "foo", 0 };
    const char *dir = "bar";
    int rc = base::os::execute_and_capture(args, dir,
      report_stdout, 0, report_stderr, 0, exit_code, 0
    );
    std::cout << "exit_code=" << exit_code << " ENOTDIR=" << ENOTDIR
              << " rc=" << rc << ": " << base::os::strerror(rc) << "\n";
    assert(exit_code == -1);
    assert(rc == ENOTDIR);
    std::cout << "exit_code and rc are correct\n";
  }
  {
    std::cout << "Test error return for non-existing command\n";
    int exit_code = 0;
    const char *args[] = { "make33", "foo", 0 };
    const char *dir = ".";
    int rc = base::os::execute_and_capture(args, dir,
      report_stdout, 0, report_stderr, 0, exit_code, 0
    );
    std::cout << "exit_code=" << exit_code << " ENOENT=" << ENOENT
              << " rc=" << rc << ": " << base::os::strerror(rc) << "\n";
    assert(exit_code == -1);
    assert(rc == ENOENT);
    std::cout << "exit_code and rc are correct\n";
  }
  #ifndef WINDOWS
  // non-executable command is a bit harder to do on Windows - skip for now
  {
    std::cout << "Test error return for non-executable command\n";
    int exit_code = 0;
    const char *args[] = { ".", "foo", 0 };
    const char *dir = ".";
    int rc = base::os::execute_and_capture(args, dir,
      report_stdout, 0, report_stderr, 0, exit_code, 0
    );
    std::cout << "exit_code=" << exit_code << " EPERM=" << EPERM
              << " rc=" << rc << ": " << base::os::strerror(rc) << "\n";
    assert(exit_code == -1);
    assert(rc == EPERM);
    std::cout << "exit_code and rc are correct\n";
  }
  #endif
  std::cout << "Bye" << std::endl;
}

#endif
