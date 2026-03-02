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

#ifndef __sa_File_h
#define __sa_File_h

#include "Entity.h"
#include "Unit.h"
#include "Occurrence.h"
#include "OccurrenceData.h"
#include "EmptyLoop.h"
#include "AnalysisStatus.h"
#include "FileMode.h"
#include "FileKind.h"
#include "Range.h"
#include "EditLog.h"
#include "Lockable.h"
#include "base/ptr.h"
#include "base/ptrset.h"
#include "base/MemberList.h"
#include <string>
#include <map>
#include <set>

namespace sa {

  class Occurrence;
  class EmptyLoop;
  class Unit;
  class Project;
  class Hdir;
  class Section;

  // A file is kept alive:
  //
  // - while it is known by the application: between get_file_handle and
  //   drop_file_handle.
  //
  // - while a runner (background analysis task) has grabbed and not dropped it.
  //   This ensures that the corresponding Unit is not destroyed while the
  //   runner is waiting or running.
  //
  // - while it has occurrences, local symbols, diagnostics or includers.
  //
  // It is deleted as soon as none of these conditions are true.
  //
  class File: public Entity {
  public:

    // Kind of file; this determines the kind of analysis to be applied. This is
    // a constant determined at creation time, based on the file extension.
    // Files in the other category do not contain a compilation unit
    // and cannot be compiled or linked.
    FileKind const file_kind;

    bool has_linkable_file_kind() const
    {
      return is_linkable_file_kind(file_kind);
    }

    bool is_source_file() const
    {
      return is_source_file_kind(file_kind);
    }

    bool is_preprocessed_file() const
    {
      return is_preprocessed_file_kind(file_kind);
    }

    // A file is a link candidate if it is force-included, or if it is automatic
    // and not an object file, or occurs in the link command.
    //
    // Automatic object files are not considered because it is too tricky to
    // recognize and avoid object files compiled from source files in the
    // project.
    bool is_link_candidate() const;

    // Create a new file (in header mode), guessing the file kind from the
    // extension. Path must be absolute and normalized.
    File(Project *project, const std::string &path);

    // The absolute path of the file.
    std::string const &get_path() const { return path; }

    // A string that uniquely identifies this file, usable as key for lookup
    // tables.
    std::string get_key() const;

    // Get the path of this file relative to the build directory.
    //
    // If the file is nested in the project directory, return a relative path.
    // This avoids issues with spaces in the name of the project directory.
    //
    // If the file is not nested in the project directory, return an absolute
    // path.
    std::string get_build_path() const;

    // The directory containing the file. This is the part of the path up to and
    // including the last slash. If the path does not contain a slash, return
    // the empty string.
    std::string get_directory() const;

    // The 'name' of a file is the file's path relative to the project source
    // directory if it is nested in the project source directory, and the file's
    // absolute path otherwise.
    std::string get_name() const;

    // Return true iff this is the main file of a compilation unit that is
    // linked into the elf file.
    bool is_linked() const;

    // Return true iff this file occurs in the link command, even if not added
    // via filetree.mk.  Such files are always linked, even if in exclude mode.
    bool is_in_link_command() const;

    // Get the list of linker sections found by analysis of this file. The
    // project must be locked (asserted) when calling this, and the list can
    // change as soon as the project is unlocked.
    const std::vector<base::ptr<Section>> &get_sections() const;
    
    // Get the file's mode.  Requires the project to be locked.
    FileMode get_mode() const;

    // Set the mode for this file.
    void set_mode(FileMode mode);

    // Set user data
    void set_user_data(void *data);

    // Return true iff the file's analysis data was read from cache.
    // Return false for files that are not analyzed.
    bool analysis_data_was_read_from_cache() const;

    // Request an update of the analysis of this file. Call this when the source
    // file or an included file has changed.
    //
    // Analysis will happen in the background; this function returns
    // immediately. Has no effect for files without analysis data. Project must
    // be locked by the caller.
    void request_update(const char *reason);

    // Notify this file that its analysis is out of date. If the file mode is
    // included or automatic, trigger a new analysis and unblock it; otherwise,
    // remove analysis data.
    void notify_out_of_date_and_unblock_unit(const char *reason);

    // Notify this file that its flags are out of data.
    void notify_flags_out_of_date(const char *reason);

    // Request an update of the flags for this file. If the flags have changed,
    // reanalyze the file. Call this for example when include directories have
    // changed.
    //
    // Flag update and analysis will happen in the background; this function
    // returns immediately. Has no effect for files without analysis data.
    // Project must be locked by the caller.
    void request_flags_update(const char *reason);

    // Request a re-analysis of this file. If this file is included by other
    // files, reload these other files as well. Analysis will happen in the
    // background; this function will return immediately.  Project must be
    // locked by the caller.
    void reload(const char *reason);

    // Request an analysis of this file if it was not analyzed yet. Can be
    // useful for files in exclude mode; for files in include or automatic mode,
    // analysis is started automatically.  Has no effect for header files.
    // Project must be locked by the caller.
    void enable_analysis(const char *reason);
    
    // Find occurrence data for an occurrence overlapping the given location
    // within a given tolerance in this file.
    OccurrenceData find_occurrence_data(
      unsigned offset, unsigned begin_tol, unsigned end_tol
    );

    Range find_empty_loop(unsigned offset);

    unsigned get_completions(
      unsigned pos,
      void (*add_completion)(const char *completion, void *user_data),
      void *user_data,
      const char *context
    );

    // Get the list of includers of this file.
    const Vector<Occurrence*> &get_includers() const;

    // Add this files and its includers - recursively - to the given set.
    // Assume that files already in the set have already been processed
    // recursively, so no need to re-examine their includers.
    void add_to_set_with_includers(std::set<File*> &files);

    // Track occurrences in this file. Only occurrences whose kind is in the
    // occurrence kind set *and* whose entity kind is in the entity kind set
    // will be tracked. Initially, both sets are empty.  To stop tracking, make
    // at least one of them empty again.
    //
    // Occurrences are tracked by calling the project's add_occurrence_in_file
    // and remove_occurrence_in_file callbacks. These callbacks must be valid
    // functions unless one of the sets is empty.
    //
    // When the occurrences to be tracked change,  the appropriate callbacks
    // are immediately executed to add or remove occurrences.
    void track_occurrences_in_file(
      OccurrenceKindSet occurrence_kinds,
      EntityKindSet entity_kinds
    );

    // Override for Entity: track occurrences of (not in) this file.
    void track_occurrences_of_entity(
      OccurrenceKindSet occurrence_kinds
    ) override;

    // Get an existing or new occurrence of the specified kind, entity and
    // location.
    //
    // Do not lock the project to get an occurrence; the implementation does
    // that automatically when required. This allows for optimalisation - for
    // example by using a separate per-file mutex - without changing the
    // callers.  Optimalisation can be important, especially for parallel source
    // code analysis, because it is expected that source code analysis adds many
    // occurrences.
    base::ptr<Occurrence> get_occurrence(
      OccurrenceKind kind,
      OccurrenceStyle style,
      const base::ptr<Entity> &entity,
      const Range &range,
      const base::ptr<Hdir> &hdir = 0
    );

    // Drop an occurrence, making it unavailable for reuse. To be called from
    // the occurrence destructor.
    void drop_occurrence(Occurrence *occurrence);

    base::ptr<EmptyLoop> get_empty_loop(const Range &range);
    void drop_empty_loop(EmptyLoop *empty_loop);

    // Get an existing or new diagnostic with the specified properties.  The
    // offset for the location of the diagnostic in this file. Note that this
    // does not display the diagnostic in the application: all it does is to
    // return a (possibly shared) representation of the diagnostic.
    //
    // The project must be locked to get a diagnostic (asserted).
    base::ptr<sa::Diagnostic> get_diagnostic(
      const std::string &message,
      Severity severity,
      const Location &location
    );

    // Drop a diagnostic, making it unavailable for reuse. To be called from
    // diagnostic destructor.
    void drop_diagnostic(Diagnostic *diagnostic);

    // Return an existing or new local symbol with the specified kind, name,
    // ref_location and scope, with ref_location in this file. If such a symbol
    // does not exist yet, create with the given user name.
    //
    // A local symbol is only known within a compilation unit.  If such a symbol
    // is declared or defined in a header, we treat it as the same symbol for
    // all including units on condition that the name, kind, location of the
    // first occurrence and scope match.  This leads to more natural behavior
    // from a user point of view.
    //
    // Doing the same if kind, name or scope don't match would complicate our
    // data structure.  It would mean that a symbol can have multiple names,
    // kinds or scopes. We avoid this complication by creating multiple symbols
    // that happen to have the same location of their first occurrence.
    //
    // This method internally locks the project; do not call it while the
    // project is locked.
    base::ptr<LocalSymbol> get_local_symbol(
      EntityKind kind,
      const std::string &user_name,
      unsigned ref_location,
      base::ptr<Occurrence> ref_scope
    );

    // Make local symbol unavailable for reuse.  Call only while project is
    // locked.
    void erase_local_symbol(LocalSymbol *symbol);

    // Return true iff this is in the project folder.
    bool is_in_project_folder() const;

    // Return true iff this is a toolchain file.  Currently approximated by
    // assuming that all non-project files are toolchain files. May need to be
    // revised when we support external libraries or project-local toolchains.
    bool is_toolchain_file() const { return !is_in_project_folder(); }
    
    // Insert an occurrence in this file. Occurrences are inserted in their file
    // when they are instantiated (in a compilation unit) for the first time. 
    void insert_occurrence_in_file(Occurrence *occurrence);
    
    // Remove an occurrence in this file. Occurrences are removed from their
    // file when the last instance (in a compilation unit) is removed.
    void remove_occurrence_in_file(Occurrence *occurrence);

    // Update an occurrence in this file. To be called when a property of a
    // tracked occurrence changes, because the tracker needs to be notified of
    // the change.
    //
    // An example is the occurrence->is_linked() flag, which will change when
    // the file's link status changes. Other examples are the location of the
    // occurrence and the effective entity kind. These "other" examples are not
    // handled yet.
    void update_occurrence_in_file(Occurrence *occurrence);

    // Insert an empty loop in this file.
    void insert_empty_loop_in_file(EmptyLoop *empty_loop);
    
    // Remove an empty loop in this file.
    void remove_empty_loop_in_file(EmptyLoop *empty_loop);

    // Increment or decrement non-utf8 count. File is non-utf-8 as long as the
    // count is non-zero.
    void increment_non_utf8();
    void decrement_non_utf8();

    // Edit file by replacing the range with the new text.  Since file contents
    // are currently not stored in the SA, this only modifies occurrences
    // accordingly.
    void edit(Range range, const char *new_content);

    // Update the number of occurrences in the link command. This is called for
    // both explicit occurrences (written by the user) and implicit occurrences
    // (added by the toolchain). Can be used for example for objects, archives,
    // source files, and linker scripts. Will enable or disable analysis when
    // appropriate, as well as call inc/dec_inclusion_count.
    void inc_in_link_command_count();
    void dec_in_link_command_count();

    // Update inclusion count; non-zero inclusion count sets inclusion status to
    // included and reports it via callback.
    void inc_inclusion_count(const char *reason);
    void dec_inclusion_count(const char *reason);

    // Update link count; non-zero link count sets link status to included and
    // reports it via callback.
    void inc_link_count();
    void dec_link_count();

    // If this file is included or linked, update its inclusion or link status.
    //
    // This is only necessary in exceptional circumstances.  For example, a file
    // mentioned in the link command is used even if the client code reports it
    // as removed. When the client code later adds it again, it will assume -
    // like for all new files - that the file is not included nor linked. Call
    // this method to fix that.
    void update_status_if_used();

    // True if there is at least one tracked entity kind and one tracked
    // occurrence kind.
    bool is_tracked() const;

    base::ptr<Unit> get_unit() const { return unit;}

    // The file's analysis status.
    AnalysisStatus get_analysis_status() const
    {
      return unit ? unit->get_analysis_status() : AnalysisStatus_none;
    }

#ifndef NDEBUG
    void increment_known();
    void decrement_known();
#endif

  protected:

    // Destructor.
    ~File();

    // Override for Entity: insert a #include-occurrence of this file.
    void insert_instance_of_entity(
      bool linked,
      Occurrence *occurrence,
      bool is_first_instance,
      bool is_first_linked_instance
    ) override;

    // Override for Entity: remove a #include-occurrence of this file.
    void remove_instance_of_entity(
      bool linked,
      Occurrence *occurrence,
      bool is_last_instance,
      bool is_last_linked_instance
    ) override;

    // Overrides for Entity
    std::string get_entity_name() const override { return get_name(); }
    File *as_file() override { return this; }

    // Override from RefCounted. Remove the file from the project and delete it.
    void zero_ref_count() override;

    void add_tracked_occurrence(Occurrence *occurrence);
    void remove_tracked_occurrence(Occurrence *occurrence);
    void update_tracked_occurrence(Occurrence *occurrence);

    void apply_edits();

    void get_completions_here(
      std::string const &prefix,
      void (*add_completion)(const char *completion, void *user_data),
      void *user_data,
      unsigned pos,
      std::set<Entity*> &done
    );

  private:

    // The file's absolute path.
    std::string path;

    // The file's mode
    FileMode _mode = FileMode_exclude;

    // The file's compilation unit, or null if this file's code isn't analyzed
    // (header file, excluded file, ...)
    base::ptr<Unit> unit;
    
    // List of occurrences of this file in include statements
    MemberList<Occurrence, &Occurrence::entity_index> _includers;

    // List of empty loops in this file
    MemberList<EmptyLoop, &EmptyLoop::loop_index> _empty_loops;

    // The number of instances of this file #include'd into compilation units
    // that are linked into the elf file plus number of times it is
    // mentioned on the command line or included implicitly by the toolchain.
    unsigned inclusion_count = 0;

    // The number of times this linkable file is linked into the elf file,
    // either because it is force-included or because the linker decided to
    // automatically include it.
    unsigned link_count = 0;

    // File is non-utf8 as long as count is non-zero.
    unsigned non_utf8_count = 0;

    // Set of occurrence kinds for which to track occurrences in this file.
    OccurrenceKindSet tracked_occurrence_kinds;

    // Set of entity kinds for which to track occurrences in this file.
    EntityKindSet tracked_entity_kinds;

    // De-duplication of occurrences.
    //
    // Occurrences are values and cannot change. The maps below (one per
    // occurrence kind) hold one occurrence for each value currently in use.
    // The get_occurrence(...)  method returns an existing or new occurrence
    // with a given value.
    //
    // Occurrences are reference-counted. References are in a compilation unit
    // or an analyzer. When the reference count goes to zero, the occurrence
    // destructor executes and the occurrence removes itself from the map.
    //
    // Occurrences have an instance count and an included instance count.  It is
    // therefore possible to determine for each occurrence whether it exists in
    // a compilation unit or in an included compilation unit. It is also
    // possible to iterate over occurrences in (included) compilation units by
    // filtering occurrences in the map; there is no need to store a separate
    // list of occurrences in compilation units. 
    //
    // Storage-wise, it would be more efficient to have a map where each key is
    // part of the corresponding value. STL does not provide such a map, so we
    // will have to implement it ourselves.  With a common 64 bit machine
    // (sizeof(unsigned)=4, sizeof(void*)=8), that would save us 32 bytes per
    // occurrence.  Occurrence size (excluding key) is currently 180 bytes. TODO
    //
  public:
    struct OccurrenceKey
    {
      Range range;
      Entity *entity;
      Hdir *hdir;
      OccurrenceKind kind;
      OccurrenceStyle style;
      
      bool operator<(const OccurrenceKey &other) const;

      bool operator!=(const OccurrenceKey &other) const
      {
        return *this < other || other < *this;
      }
      bool operator==(const OccurrenceKey &other) const
      {
        return !(*this != other);
      }
    };

#ifdef CHECK
    void check() const;
#endif
  private:
    // De-duplication of allocated local symbols.
    base::ptrset<LocalSymbol*> _local_symbols;

    // De-duplication of occurrences, per kind of occurrence.
    std::map<OccurrenceKey, Occurrence*>
      _occurrence_map[NumberOfOccurrenceKinds];

    // De-duplication of empty loops.
    std::map<Range, EmptyLoop*> _empty_loop_map;

    // De-duplication of diagnostics.
    //
    // Diagnostic are values and cannot change. The map below holds one
    // diagnostic for each value currently in use.  The get_diagnostic(...)
    // method returns an existing or new diagnostic with a given value.
    //
    // Diagnostics are reference-counted. References are in a compilation unit
    // or an analyzer. When the reference count goes to zero, the diagnostic
    // destructor executes and removes itself from the map.
    //
    // Diagnostics can only be added and removed while the project is locked.
    struct DiagnosticKey
    {
      std::string message;
      Severity severity;
      Location location;
      
      bool operator<(const DiagnosticKey &other) const;
    };
    std::map<DiagnosticKey, Diagnostic*> _diagnostic_map;

    unsigned known_count = 0;

    EditLog edit_log;

    // True iff occurrences of this file are tracked.
    bool tracked = false;

#ifdef CHECK
    void notify_ref_count() const override;
#endif
  };

  inline std::ostream &operator<<(std::ostream &out, const File &file)
  {
    return out << file.get_name();
  }
}

#endif
