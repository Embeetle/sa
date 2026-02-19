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
