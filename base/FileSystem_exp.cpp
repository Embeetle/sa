#include "FileSystem.h"

void base::FileSystem::file_created(std::string const &path)
{
  (void)path;
}

void base::FileSystem::file_removed(std::string const &path)
{
  (void)path;
}

void base::FileSystem::file_changed(std::string const &path)
{
  (void)path;
}

void base::FileSystem::file_moved(
  std::string const &from_path,
  std::string const &to_path
)
{
  (void)from_path;
  (void)to_path;
}


#ifdef SELFTEST

#include <iostream>

class TestFileSystem: public base::FileSystem {
public:
  using base::FileSystem::file_created;
  using base::FileSystem::file_removed;
  using base::FileSystem::file_changed;
  using base::FileSystem::file_moved;
};

TestFileSystem fs;

class FlagExtractor: base::FileSystem::Watcher {
  FlagExtractor() : base::FileSystem::Watcher("/foo/projects/bar")
  {
    watch_file("Makefile");
    base::FileSystem::SearchPath make_search_path({ ".", "include" });
    watch_file("filetree.inc", make_search_path);
    
  }
  

};

int main(int argc, const char *argv[])
{
  std::cout << "Hello\n";
  (void)argc;
  (void)argv;

  fs.file_created("/foo/projects/bar/Makefile");
  fs.file_created("/foo/projects/bar/foo.c");
  fs.file_created("/foo/projects/bar/include/foo.h");


  std::cout << "Selftest succeeded\n";
  return 0;
}

#endif
