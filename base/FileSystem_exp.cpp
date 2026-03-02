// Copyright © 2018-2026 Johan Cockx
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// SPDX-License-Identifier: GPL-3.0-or-later

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
