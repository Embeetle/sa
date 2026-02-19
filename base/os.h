// Copyright 2018-2023 Johan Cockx
#ifndef __base_os_h
#define __base_os_h

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <stdint.h>

namespace base {
  namespace os {
    // Assume slash as path separator on all OS's unless otherwise noted.

    const char *getenv(const char *name);

    typedef enum { OS_windows, OS_linux } OS;
    OS get_os();

    const char *get_os_name();

    // Empty for Linux,  ".exe" for Windows
    const char *get_exe_extension();

    // Remove exe extension for Window,  no-op for Linux.
    std::string_view strip_exe_extension(std::string_view word);

    // /dev/null for Linux,  NUL for Windows
    const char *get_null_device_path();

    // Path of directory for temporary files.
    const char *get_temp_directory_path();

    // Override path of directory for temporary files for testing purposes.
    void set_temp_directory_path(const char *path);

    // Return true iff data starts with path. On Windows, a / in path can be
    // matched by either a / or a \ in data.
    bool starts_with_path(const char *data, const char *path, size_t path_size);

    // Make sure all line endings in the string are plain '\n'.
    // On Windows, remove all \r characters.  On Linux,  do nothing.
    void normalize_line_endings(std::string &data);

    // On a case insensitive OS such as Windows, lowercase the path (except for
    // the drive letter). On a case sensitive OS like Linux, do nothing.
    void normalize_path_case(std::string &path);

    // Get line from istream, and remove trailing \r on Windows even if the
    // stream is in binary mode.
    std::istream &getline(std::istream &input, std::string &line);

    // If the given path exists on disk with different casing (uppercase vs
    // lowercase), set path_on_disk to the path on disk and return true.
    // Otherwise, return false.
    //
    // On Linux, this always returns false (and is extremely fast).  On Windows,
    // where paths are case insensitive, this checks the path and may return
    // true.
    //
    // The directory path is prepended to the given path before checking the
    // disk. It is not checked for casing and not preprended to the returned
    // path on disk.
    //
    // This is a slow action; take care when using it in a performance critical
    // context.
    //
    bool path_has_different_case_on_disk(
      std::string_view path,
      std::string &path_on_disk,
      std::string_view directory_path = ""
    );

    // File metadata.
    struct file_meta_data {
      uint64_t mtime_secs;
      uint64_t mtime_nsecs;
      uint64_t size_bytes;
    };

    // Get file metadata, return false iff file not accessible.
    bool get_file_meta_data(const char *path, file_meta_data &data);

    // Return the path's extension. This is the part of the path starting from
    // the last dot after the last slash (slash or backslash on Windows).  If
    // there is no such dot, return the empty string.
    const char *get_extension(const char *path);

    // Return the base name of a command.  On Windows, this strips any extension
    // listed in the PATHEXT environment variable.  On other platforms, it does
    // nothing.
    std::string get_command_base_name(const char *path);
    
    // Return true iff path is absolute.  On Linux,  check for initial slash.
    // On Windows,  also check for drive letter.
    bool is_absolute_path(const char *path);
    bool is_absolute_path(std::string_view path);
    
    // Return true iff both paths are in the same tree. Always true on Linux,
    // false on Windows when the paths have different drive letters. Assumes
    // both paths are absolute.
    bool in_same_tree(const char *path1, const char *path2);
    
    // Create a directory with the given path Creates at most one level.
    void create_directory(const char *path);

    // Return true if the given path has a drive letter.
    bool has_drive_prefix(const char *path);
    bool has_drive_prefix(std::string_view path);
    
    // Return the size of the drive letter prefix of the path. On Windows,
    // return 2 for paths starting with a drive letter.  In all other cases,
    // return 0.
    size_t drive_prefix_size(const char *path);
    size_t drive_prefix_size(std::string_view path);
    
    // Return true iff the given path is an absolute path. This is a pure string
    // computation without file access.  To be consistent with
    // get_normalized_path, the empty string is considered to be an absolute
    // path.
    bool is_absolute_path(const char *path);

    // Make the given path absolute if it isn't. A relative path is relative to
    // the given directory path. This is a pure string computation without file
    // access. It does not normalize the path. If both path and directory path
    // are relative,  the result is also relative.
    std::string get_absolute_path(
      const char *path,
      const std::string &directory_path
    );

    // Get normalized path for a given path.  This is a pure string computation,
    // no disk access.
    //  - remove ./
    //  - remove x/.. for any x
    //  - remove duplicate /
    //  - remove trailing / except for root directory
    //  - replace empty string by /
    //  - on Windows,  replace \ by /
    //  - on Windows,  make drive letter uppercase
    //
    // Invariant:
    // norm(path + '/' + relpath) == norm(norm(path) + '/' + norm(relpath))
    //
    // To achieve this invariant,  norm('') must be '/' not '.' or ''.
    std::string get_normalized_path(const std::string &path);

    // True iff path is normalized.
    bool is_normalized_path(const std::string &path);

    // Convert an array of strings to a command line with proper quoting of
    // special characters.
    //
    // On Windows, the OS calls to run a command (CreateProcess and others) take
    // a full command line in a single string, not an array of strings.
    //
    // The first string in the array - the program to execute - cannot contain
    // special characters " * : ? < > |. This is a limitation of Windows, not of
    // our code.
    //
    // Quoting is only used when necessary; in that sense, the returned command
    // string is minimal. When there is a choice on where to insert double
    // quotes, we use the "natural" approach: we insert double quotes before and
    // after the whole string.
    std::string quote_command_line(const char *args[]); // null-terminated
    std::string quote_command_arg(const char *filename);

    // Get a dictionary containing all currently defined environment variables.
    std::map<std::string, std::string> current_env();

    // Create a native representation for a set of environment variables.
    // This can be used as an argument for execute_and_capture below.
    // When no longer needed, the set needs to be destroyed using destroy_env.
    // Dict keys cannot contain '='.
    //
    // Note that creating the native representation requires iteration over all
    // environment variables as well as dynamic memory allocation.  If you
    // repeatedly need the same environment, consider creating it once and then
    // reusing it.
    void *create_native_env(const std::map<std::string, std::string> &env);

    // Destroy a native environment created by create_env.
    void destroy_native_env(void *env);

    // Return native env as string, for debugging.  This is a multiline string,
    // one environment variable per line.
    std::string print_native_env(void *env);

    // Execute command, calling the given functions in a separate thread to
    // process standard output and standard error. If the command can be started,
    // set *exit_code to the command's exit code and return 0.
    //
    // Exit codes depend on the executed command.  Successful execution is
    // usually represented by an exit code of 0. There are OS-specific
    // conventions (that are not necessarily followed by all commands). For
    // example, on Linux, /usr/include/sysexits.h defines these constants:
    //
    // #define EX_OK           0    /* successful termination */
    // #define EX_USAGE        64   /* command line usage error */
    // #define EX_DATAERR      65   /* data format error */
    // #define EX_NOINPUT      66   /* cannot open input */    
    // #define EX_NOUSER       67   /* addressee unknown */    
    // #define EX_NOHOST       68   /* host name unknown */
    // #define EX_UNAVAILABLE  69   /* service unavailable */
    // #define EX_SOFTWARE     70   /* internal software error */
    // #define EX_OSERR        71   /* system error (e.g., can't fork) */
    // #define EX_OSFILE       72   /* critical OS file missing */
    // #define EX_CANTCREAT    73   /* can't create (user) output file */
    // #define EX_IOERR        74   /* input/output error */
    // #define EX_TEMPFAIL     75   /* temp failure; user is invited to retry */
    // #define EX_PROTOCOL     76   /* remote error in protocol */
    // #define EX_NOPERM       77   /* permission denied */
    // #define EX_CONFIG       78   /* configuration error */
    //
    // Also on Linux, when a command terminates due to a signal, the exit code
    // is 128+signal_number. Exit code 2 is often used for bad command line
    // usage.
    //
    // If command execution fails (i.e. the command cannot be started), set
    // exit_code to -1 and return a non-zero error number (errno). This can be
    // converted to an error string using strerror(errno). Returned errno is
    // usually one of:
    //
    //    - ENOTDIR:  the requested work directory does not exist
    //    - ENOENT:   the requested command (args[0]) does not exist
    //    - EPERM:    the requested command (args[0]) is not executable
    //
    // although there are multiple other possible error numbers, often
    // OS-specific.
    //
    // If command execution succeeds (i.e the command can be started, although
    // it does not necessarily finish normally), call stdout_handler and
    // stderr_handler in separate threads, passing each an istream connected to
    // the command's output and error streams, respectively. When both functions
    // return, wait for the command to finish, set exit_code to the command's
    // return value and return zero.
    //
    // Both handlers *must* read all data from the istreams passed to them, even
    // if it is not used. Otherwise, the associated stream buffers may fill up,
    // causing the command to hang because it can no longer write output. The
    // point at which this becomes important depends on the amount of output
    // produced by the command and on the OS buffer sizes, which are OS
    // dependent. The easiest way to read and discard all data from an istream
    // is this C++ code:
    //
    //   #include <limits>
    //
    //   void handler(std::istream &in, void *context)
    //   {
    //       in.ignore(std::numeric_limits<std::streamsize>::max());
    //   }
    //
    // The args list must be terminated by a null pointer, and args[0] is the
    // command to be executed. It will be searched for on PATH if it does not
    // contain a slash.
    //
    // Streams for stdout and stderr are binary streams; on Windows, line
    // endings will probably include \r.
    int execute_and_capture(
      const char *args[],
      const char *work_directory,
      void (*stdout_handler)(std::istream&, void*), void *stdout_context,
      void (*stderr_handler)(std::istream&, void*), void *stderr_context,
      int& exit_code,
      void *env
    );

    // Auxiliary template, to allow execute_and_capture to be called with a
    // class instance and two method pointers instead of function pointers.
    // This is often more convenient, as the class instance can contain shared
    // data and store results.
    //
    // CAVEAT: potential for deadlock on Windows.
    //
    // On Windows, two simultanuous calls, where one locks an application mutex
    // before making the call, and the other locks the same mutex in one of the
    // handler threads, can cause a deadlock. Symptom could be that reading the
    // eof in the handler hangs. Possible cause is that the OS internally uses a
    // mutex while reading, so there will be two threads trying to lock both the
    // OS mutex and the appplication mutex, in a different order.
    //
    template <class Handler>
    int execute_and_capture(
      const char *args[],
      const char *work_directory,
      Handler *handler,
      void (Handler::*stdout_handler)(std::istream&),
      void (Handler::*stderr_handler)(std::istream&),
      int& exit_code,
      void *env
    )
    {
      struct Aux {
        Handler *handler;
        void (Handler::*stdout_handler)(std::istream&);
        void (Handler::*stderr_handler)(std::istream&);
        Aux(
          Handler *handler,
          void (Handler::*stdout_handler)(std::istream&),
          void (Handler::*stderr_handler)(std::istream&)
        ):
          handler(handler),
          stdout_handler(stdout_handler),
          stderr_handler(stderr_handler)
        {}
        static void handle_stdout(std::istream& in, void *self)
        {
          Aux *aux = reinterpret_cast<Aux*>(self);
          (aux->handler->*(aux->stdout_handler))(in);
        }
        static void handle_stderr(std::istream& in, void *self)
        {
          Aux *aux = reinterpret_cast<Aux*>(self);
          (aux->handler->*(aux->stderr_handler))(in);
        }
      };
      Aux aux(handler, stdout_handler, stderr_handler);
      return execute_and_capture(
        args, work_directory,
        Aux::handle_stdout, &aux,
        Aux::handle_stderr, &aux,
        exit_code,
        env
      );
    }

    // Get error message for code returned by execute_and_capture
    const char *strerror(int error_number);

    // Get thread ID.
    unsigned get_tid();
  }
}

#endif
