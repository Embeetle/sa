// Copyright 2018-2023 Johan Cockx
#ifndef __base_filesystem_h
#define __base_filesystem_h

#include <fstream>
#include <string>
#include <vector>
#include <stdint.h>

// API for filesystem access.
//
// A path refers to an existing or non-existing object in the file
// system. Multiple paths can refer to the same object, and the rules depend on
// the local OS.  For example, paths that differ in case only refer to the same
// object on Windows, and symlinks or hard links can cause different paths to
// refer to the same object on Linux. Also, relative paths refer to the same
// object as at least one absolute path.
//
// Paths passed to this API must use '/' as separator, and all other characters
// must be legal for use in a file name in the local OS.  Behavior for illegal
// characters is undefined.
//
// A path starting with '/' is an absolute path.  On Windows, a path starting
// with a drive letter followed by ':/' is also an absolute path.  All other
// paths are relative. A relative path starting with a drive letter is not
// legal.
//
// '.' is the working directory, '..' is the parent directory and consecutive
// '/'-characters are interpreted as a single '/'.
//
// A normalised path is a path that does not contain '/../', '/.' or '//' and
// does not end in '/'.  Normalisation removes these sequences from the path
// without changing the designated object in the file system.
//
// The real path of a file object is unique.  It is always an absolute path.  On
// Linux, it does not contain symlinks or hard links. On Windows, it always has
// an uppercase drive letter, and all other letters are lowercase. Different
// real paths always refer to different objects.
//
namespace base {
  // Return the working directory
  std::string get_working_directory();

  // Return true iff the given path exists in the file system.
  bool path_exists(const std::string &path);

  // Return true iff the given path is readable in the file system.
  bool is_readable(const std::string &path);
    
  // Return true iff the given path is a regular file
  bool is_file(const std::string &path);

  // Return true iff the given path is a directory
  bool is_directory(const std::string &path);

  // Create a directory with the given path. Return true iff succesful.
  // Creates at most one level. Use is_directory(path) to test for success.
  void create_directory(const std::string &path);
    
  // Remove a file or folder.
  void remove(const std::string &path);
    
  // Open a file with the given path for writing.  If the file exists, truncate
  // it.  If it doesn't exist, create it. If the file's parent directory does
  // not exist, create it, recursively. Use file.is_open to check for success.
  //
  // The file is always opened in binary mode, to ensure linux-style line
  // endings.
  //
  // A non-zero 'skip' value can be used to speed up the creation of parent
  // directories. If 'skip' is non-zero, 'path[skip]' must be a slash '/'
  // (asserted). The directory designated by 'path[0..skip-1]' will not be
  // created if it doesn't exist.
  //
  // Use file.is_open to check for success.
  //
  void open_for_writing(std::ofstream &file, const std::string &path,
    size_t skip = 0
  );

  // Open a file with the given path for reading.  Use file.is_open to check for
  // success.
  //
  void open_for_reading(std::ifstream &file, const std::string &path);

  // Get line from istream, and remove trailing \r on Windows even if the
  // stream is in binary mode.
  std::istream &getline(std::istream &input, std::string &line);

  // Open a temporary file for writing, and return its path. Use file.is_open to
  // check for success. On failure, return error message.
  std::string open_temp_for_writing(std::ofstream &file
    , const std::string &prefix = ""
    , const std::string &suffix = ""
  );

  // On a case insensitive OS such as Windows, lowercase the path (except for
  // the drive letter). On a case sensitive OS like Linux, do nothing.
  std::string normalize_path_case(std::string const &path);

  // Patch path to use case on disk. This is a no-op except for Windows. Only
  // the part of the path that already exists is changed. Return true iff there
  // are any changes.
  //
  // This is a slow action; take care when using it in a performance critical
  // context.
  bool patch_path_to_case_on_disk(std::string &path);

  // Get an integer value that changes when the file contents change.  There is
  // no guarantee that it is the same for files with identical contents.  If the
  // given path does not exist, return zero.
  uint64_t get_signature(const std::string &path);

  // Return the path's extension. This is the part of the path starting from the
  // last dot after the last slash (slash or backslash on Windows).  If there is
  // no such dot, return the empty string.
  std::string get_extension(const std::string &path);

  // Return the base name of a command.  On Windows,  this strips any extension
  // listed in the PATHEXT environment variable.  On other platforms,  it does
  // nothing.
  std::string get_command_base_name(std::string_view path);
  std::string get_command_base_name(std::string const &path);

  // Return true iff the given path is an absolute path. This is a pure string
  // computation without file access. To be consistent with get_normalized_path,
  // the empty string is considered to be an absolute path.
  bool is_absolute_path(const std::string &path);
  bool is_absolute_path(std::string_view path);
  bool is_absolute_path(const char *path);
  inline bool is_relative_path(const std::string &path)
  {
    return !is_absolute_path(path);
  }
  inline bool is_relative_path(std::string_view path)
  {
    return !is_absolute_path(path);
  }
  inline bool is_relative_path(const char *path)
  {
    return !is_absolute_path(path);
  }

  // Join paths: if path2 is relative, prefix it with path1. This is a pure
  // string computation without file access. It does not normalize the path.
  std::string join_paths(const std::string &path1, const std::string &path2);

  // Return the absolute path for the given path.
  //
  // If the given path is already absolute, keep it.  Otherwise, assume it is
  // relative to the directory path. Prepend the directory path and normalize
  // the result.
  //
  // This is a pure string computation without file access.  Paths must be
  // normalized.
  //
  std::string get_absolute_path(
    const std::string &given_path,
    const std::string &directory_path
  );

  // For the given path, return the relative path with respect to the given
  // directory.
  //
  // If both the given path and the directory path are relative, assume that
  // they are relative to the same unspecified directory.
  //
  // If the given path is relative and the directory is absolute, assume that it
  // is relative to the directory path.
  //
  // If the given path is absolute and the directory is relative, return the
  // given path.
  //
  // If both paths are absolute and in a different tree (different drive
  // letter), return the given path.
  //
  // Otherwise, remove the common part of the paths and prepend with the
  // appropriate number of '../'.
  //
  // This is a pure string computation without file access. Paths must be
  // normalized.
  //
  std::string get_relative_path(
    const std::string &given_path,
    const std::string &directory_path
  );

  // Return true iff the given path is nested in or equal to the given
  // directory.
  //
  // If both the given path and the directory path are relative, assume that
  // they are relative to the same directory.
  //
  // If one path is relative and the other is not, return false.
  //
  // In case of doubt, return false.  This happens:
  //
  //  - when one path is relative and the other is not
  //
  //  - when both paths are relative,  and the given path starts with ..
  //
  // For example:
  //
  //   directory path: foo
  //   item path:      ../bar/foo/dot
  //
  // We don't know if the parent directory of foo is bar or not.  If it is, the
  // path is nested, otherwise, it isn't.  In this case, we return false.  This
  // only happens for relative directories. It can happen for absolute item
  // path, though.
  //
  // Another example:
  //
  //   directory path: foo
  //   item path:      foo/dot
  //
  // The item path descends into the same directory, so in this case, nesting is
  // sure and we return true, regardless of where both paths are located.
  //
  // This is a pure string computation without file access. Paths are assumed to
  // be normalized.
  //
  bool is_nested_in(
    const std::string &path,
    const std::string &directory_path
  );
    
  // Return true iff the given path is nested in but not equal to the given
  // directory. See is_nested_in for details.
  bool is_strictly_nested_in(
    const std::string &path,
    const std::string &directory_path
  );
    
  // If the given item path is nested in the given directory path, return the
  // path relative to the directory. Otherwise, return the absolute path.
  //
  // A relative given path is relative to the given directory path.
  // It might still return an absolute path if it starts with '../'.
  //
  // This is a pure string computation without file access. Paths are assumed to
  // be normalized.
  //
  std::string get_natural_path(
    const std::string &item_path,
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

  // Get parent path size, i.e. the number of characters before the last /,
  // or zero if the string has no /.
  size_t get_parent_path_size(const std::string &path);
    
  // Get parent path, i.e. truncate the path just before the last /.
  // If the path does not contain a /,  return the empty string.
  std::string get_parent_path(const std::string &path);

  // Get the part of the path up to and including the last slash, or empty.
  std::string_view get_directory_prefix(std::string_view path);

  // Get the part of the path after the last slash, or all.
  std::string_view get_leaf_name(std::string_view path);

  // Get the string length of the longest common ancestor of two paths.  Both
  // paths start with the common ancestor. If the common ancestor size is
  // non-zero, it is followed in both paths by a slash / or end of string.  This
  // is a pure string computation without filesystem access.  Paths are assumed
  // to be normalized.
  size_t common_ancestor_size(
    const std::string &path1,
    const std::string &path2
  );

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
  std::string quote_command_line(const std::vector<std::string> &args);
  std::string quote_command_line(const std::vector<const char*> &args);
  std::string quote_command_line(const std::vector<std::string_view> &args);
  std::string quote_command_line(const char *args[]);
  std::string quote_command_arg(const std::string &filename);
  std::string quote_command_arg(std::string_view filename);
  std::string quote_command_arg(const char *filename);

  // Get the first word from a command line starting at a given position,
  // skipping comments and handling quoted strings and escapes. Return the
  // position of the next character after the word, or npos if no word was
  // found.
  //
  // Efficiency: chars will be appended one by one to word, so try to keep
  // capacity for intensive use.
  size_t unquote_command_word(
    std::string &word,
    std::string_view line,
    size_t pos = 0
  );
}

#endif
