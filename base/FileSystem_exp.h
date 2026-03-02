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

#ifndef __base_FileSystem_h
#define __base_FileSystem_h

#include <string>
#include <vector>
#include <map>

namespace base {

  class FileSystem {
  public:
    class File {
    public:
      File(std::string const &path);
      std::string const &path() { return _path; }
      std::string normalized_path() const;
      std::string unique_path() const;
      std::string path_case_on_disk() const;
    
    
    private:
      std::string const _path;
    };

    class SearchPath {
    public:
      SearchPath(std::vector<std::string> const &folders);

    private:
      std::vector<std::string> const _folders;
    };
      



  
    class Watcher {
    public:
      Watcher(std::string const &anchor = ".");
    
    
      void watch_file(std::string const &path);
      void forget_file(std::string const &path);



      virtual void file_changed(std::string const &path)
      {
        (void)path;
      }

      virtual void file_created(std::string const &path)
      {
        (void)path;
      }

      virtual void file_removed(std::string const &path)
      {
        (void)path;
      }
    private:
      std::string const _anchor;

    };

  protected:
    void file_created(std::string const &path);
    void file_removed(std::string const &path);
    void file_changed(std::string const &path);
    void file_moved(std::string const &from_path, std::string const &to_path);

  private:
    std::map<std::string, File> _file_map;
  
  };

}

#endif
