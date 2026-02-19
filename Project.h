// Copyright 2018-2024 Johan Cockx
#ifndef __sa_Project_h
#define __sa_Project_h

#include "source_analyzer.h"
#include "Diagnostic.h"
#include "Lockable.h"
#include "link_status_map_type.h"
#include "base/ptrset.h"
#include <map>
#include <set>
#include <string>
#include <fstream>

#include <cstring>
#include <cstdint>
#include "base/debug.h"

namespace sa {
  class FlagExtractor;
  class Unit;
  class Linker;
  class Unit;
  class Hdir;
  class SymbolUid;
  class FileLocation;
  class ExternSymbol;
  class LocalSymbol;
  class GlobalSymbol;
  class ClassSymbol;

  class Project: public Lockable, public base::PrefailAction {
  public:
    // Linker.
    Linker *const linker;

    // Flag extractor.
    FlagExtractor *const flag_extractor;

    Project(
      const std::string &project_path,
      const std::string &cache_path,
      const std::string &resource_path,
      const std::string &lib_path,
      ProjectStatus_callback project_status_callback,
      InclusionStatus_callback inclusion_status_callback,
      LinkStatus_callback link_status_callback,
      AnalysisStatus_callback analysis_status_callback,
      AddSymbol_callback add_symbol_callback,
      DropSymbol_callback drop_symbol_callback,
      LinkerStatus_callback linker_status_callback,
      HdirUsage_callback hdir_usage_callback,
      AddDiagnostic_callback add_diagnostic_callback,
      RemoveDiagnostic_callback remove_diagnostic_callback,
      MoreDiagnostics_callback more_diagnostics_callback,
      AddOccurrenceInFile_callback add_occurrence_in_file_callback,
      RemoveOccurrenceInFile_callback remove_occurrence_in_file_callback,
      OccurrencesInFileCount_callback occurrences_in_file_count_callback,
      SetAlternativeContent_callback set_alternative_content_callback,
      AddOccurrenceOfEntity_callback add_occurrence_of_entity_callback,
      RemoveOccurrenceOfEntity_callback remove_occurrence_of_entity_callback,
      OccurrencesOfEntityCount_callback occurrences_of_entity_count_callback,
      SetOccurrenceOfEntityLinked_callback
        set_occurrence_of_entity_linked_callback,
      ReportInternalError_callback report_internal_error_callback,
      SetMemoryRegion_callback set_memory_region_callback,
      SetMemorySection_callback set_memory_section_callback,
      Utf8_callback utf8_callback,
      void *user_data
    );

    ~Project();

    const std::string &get_toolchain_prefix() const;
    void set_toolchain_prefix(const std::string &prefix);

    // Return absolute path in which the make command will be invoked.
    const std::string &get_build_path() const;

    // Set the path in which the make command will be invoked. If relative,
    // assume it is relative to the project path.
    void set_build_path(const std::string &prefix);

    // Return the absolute path of the project folder, with forward slashes, no
    // . or .. and no trailing slash. The project folder contains all source
    // files of the project proper, excluding toolchain headers.
    const std::string &get_project_path() const { return project_path; }

    // Return a normalized path for any file, usable for communication with the
    // user. Returns a relative path for files nested in the project directory,
    // and an absolute path otherwise.  If the incoming path is relative, it is
    // assumed to be relative to the project directory. On Windows, the case is
    // not changed.
    std::string get_natural_path(const std::string &path);

    // Return a normalized path for any file, usable from the build directory.
    // Returns a path relative to the build directory for files nested in the
    // project directory, and an absolute path otherwise.  If the incoming path
    // is relative, it is assumed to be relative to the project directory. On
    // Windows, the case is not changed.
    std::string get_build_path(const std::string &path);

    // Return a unique normalized path for any file, usable as a key in lookup
    // tables. Return the natural path as defined above, except that it is
    // lowercased on case-insensitive operating systems like Windows.
    std::string get_unique_path(const std::string &path);

    // Return the absolute path of the cache folder, with forward slashes, no
    // . or .. and no trailing slash. The cache folder is used to cache analysis
    // results.
    const std::string &get_cache_path() const;

    // Return the absolute path of the resource folder for Clang.  This folder
    // contains Clang-specific system header files that are required for
    // analysis.
    const std::string get_clang_resource_path() const;

    // Return the absolute path of the executable used to analyze assembly file.
    const std::string get_asm_analyzer_path() const;

    // Return the absolute path of the executable used to analyze binary files
    const std::string get_bin_analyzer_path() const;

    // Return the absolute path of the executable used to analyze linker scripts.
    const std::string get_linker_script_analyzer_path() const;

    // Return the file for a given path.  Create it if it doesn't exist yet.
    // Add it to the file map for reuse. Path will be normalized. A relative
    // path is relative to the project path (path of top level project
    // directory). New files wil be in header mode, with no user data.
    //
    // Lock the project before calling this method.
    base::ptr<File> get_file(const std::string &path);

    // Erase the file from the file map.
    //
    // Lock the project before calling this method.
    void erase_file(File *file);

    // Set the make command
    void set_make_command(std::vector<std::string_view> args);

    // Re-analyze the make command
    void analyze_make_command();

    // Report a changed file inclusion status to the application.
    void report_file_inclusion_status(File const *file, unsigned status);

    // Report a changed unit link status to the application.
    void report_file_link_status(File const *file, unsigned status);

    // Returns an Hdir for a given path.  Create it if it doesn't exist yet.
    // Path is either absolute or relative to the project path. It does not need
    // to be normalized.
    base::ptr<Hdir> get_hdir(const std::string &path);

    void *get_user_data() const { return user_data; }

    // Remove an hdir. Call this before the hdir is deleted.
    void erase_hdir(const Hdir *hdir);

    // Report changed hdir usage to user application
    void report_hdir_usage(const Hdir *hdir, InclusionStatus status);

    // Return an existing or new global symbol with the specified link-name. If
    // such a symbol does not exist yet, create it.
    //
    // This method internally locks the project; do not call it while the
    // project is locked.
    base::ptr<sa::GlobalSymbol> get_global_symbol(
      const std::string &link_name
    );

    // Same, assuming project is already locked.
    base::ptr<sa::GlobalSymbol> _get_global_symbol(
      const std::string &link_name
    );

    // Return a global symbol with the specified link-name. If such a symbol
    // does not exist yet, return null.
    //
    // Lock the project before calling this method.
    base::ptr<sa::GlobalSymbol> find_global_symbol(
      const std::string &link_name
    ) const;

    // Return an existing or new local symbol with the specified kind, name, 
    // ref_location and scope. If such a symbol does not exist yet, create
    // with the given user name.
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
      const FileLocation &ref_location,
      Occurrence *ref_scope
    );

    // Create application data for a symbol using the add symbol callback.
    void add_symbol(Symbol *symbol);

    // Notify the application that user data for a symbol is no longer required.
    // After this call, the symbol may be deleted, so the application should
    // discard any copy of the symbol handle.
    void drop_symbol(Symbol *symbol);

    // Update for a new analysis state for a unit in this project.
    // This is used to compute the project status.
    void update_for_analysis_status_change(
      Unit *unit,
      AnalysisStatus old_status,
      AnalysisStatus new_status,
      const char *reason
    );

    // Update link candidate status. Call only when changed.
    // This is used to compute the project status.
    void update_link_candidate_status(
      File *file,
      bool new_is_link_candidate,
      AnalysisStatus analysis_status
    );

    // Erase the symbol in the symbol map.
    void erase_global_symbol(GlobalSymbol *symbol);
    void erase_local_symbol(LocalSymbol *symbol);

    // Get an existing or new project diagnostic, i.e. a diagnostic not located
    // in a specific file.  Note that this does not display the diagnostic in
    // the application: all it does is to return a (possibly shared)
    // representation of the diagnostic.
    base::ptr<sa::Diagnostic> get_diagnostic(
      const std::string &message, Severity severity,
      Category category = Category_none
    );
    
    // Release a diagnostic, making it unavailable for reuse. To be called from
    // diagnostic destructor.
    void release_diagnostic(Diagnostic *diagnostic);
    
    // Set new linker results.
    void set_linker_results(
      const link_status_map_type &link_status_map,
      base::ptr<GlobalSymbol> main
    );

    // Set alternative content for binary file. Content must remain valid during
    // this call.
    void set_alternative_content(
      File const *file,
      const char *content
    );

    // Set the maximum number of diagnostics of a given severity that should
    // be reported to the application. Diagnostics for which user data is null
    // after the add diagnostic callback are not counted.
    void set_diagnostic_limit(Severity severity, size_t new_limit);

    // Get the maximum number of diagnostics of a given severity that should
    // be reported to the application. 
    size_t get_diagnostic_limit(Severity severity);
      
    // Add a list of diagnostics for a compilation unit.  Try to keep
    // diagnostics in their original order.
    void add_unit_diagnostics(
      std::vector<base::ptr<Diagnostic>> &diagnostics
    );

    // Remove a list of diagnostics for a compilation unit.
    void remove_unit_diagnostics(
      std::vector<base::ptr<Diagnostic>> &diagnostics
    );

    // Add a link diagnostic (undefined or multiply defined symbol) for an
    // occurrence. At most one link diagnostic per occurrence is allowed.
    void add_link_diagnostic(Occurrence *occurrence, Diagnostic *diagnostic);
    
    // Remove the link diagnostic for an occurrence.
    void remove_link_diagnostic(Occurrence *occurrence);

    // Use a callback to track an occurrence in a file. If the remove occurrence
    // callback is non-null,  register returned user data for later untracking.
    void add_occurrence_in_file(Occurrence *occurrence);

    // Use a callback to untrack an occurrence in a file based on the user data
    // returned when it was tracked. If the remove occurrence callback is null,
    // do nothing.
    void remove_occurrence_in_file(Occurrence *occurrence);

    // Use callbacks to retrack an occurrence in a file.  To be called when
    // properties of an occurrence have changed. No-op when the occurrence is
    // not tracked.
    void update_occurrence_in_file(Occurrence *occurrence);

    // Add a tracked occurrence to the application
    void add_occurrence_of_entity(Occurrence *occurrence);

    // Remove a tracked occurrence from the application
    void remove_occurrence_of_entity(Occurrence *occurrence);

    // Set the linked status of a tracked occurrence in the application
    void set_occurrence_of_entity_linked(Occurrence *occurrence, bool linked);

    // Set the maximum number of occurrences in a file to be reported.
    void set_occurrences_in_file_limit(size_t new_limit);

    // Get the maximum number of occurrences in a file to be reported.
    size_t get_occurrences_in_file_limit();

    // Set the maximum number of occurrences of an entity to be reported.
    void set_occurrences_of_entity_limit(size_t new_limit);

    // Get the maximum number of occurrences of an entity to be reported.
    size_t get_occurrences_of_entity_limit();
    
    // Add a compilation-unit-to-missing-header association
    void add_missing_header(const std::string &header, Unit*);

    // Remove a compilation-unit-to-missing-header association
    void remove_missing_header(const std::string &header, Unit*);

    // Remove a unit, specifically missing headers for that unit and any
    // registration of the unit in the flag extractor.
    void remove_unit(Unit*);

    // Notify the project that a file exists.  This can trigger a re-analysis of
    // compilation units with missing headers, as well as compilation units that
    // are currently being analyzed. Call this whenever it is possible that the
    // file did not exist previously, in other words if it may be reported as a
    // missing header in an existing analysis.  It will only do something if it
    // matches a missing header.
    void notify_file_exists(const std::string &path);

    // Notify the project that an hdir was added.  This triggers a flags update
    // and re-analysis of compilation units with missing headers that can be
    // resolved by this hdir.  It may or may not add the hdir to the map of used
    // hdirs, depending on analysis results. It always added to the set of added
    // hdirs.
    void add_hdir(const std::string &path);

    // Notify the project that an hdir was removed.  This triggers a flags
    // update and re-analysis of compilation units including files from this
    // hdir. The hdir may or may not be removed from the map of used hdirs,
    // depending on analysis results. It is always removed from the set of added
    // hdirs.
    void remove_hdir(const std::string &path);

    // Return the set of added hdirs.
    std::set<base::ptr<Hdir>> const &added_hdirs() const;

#ifdef MAINTAIN_TOOLCHAIN_LIST_IN_PROJECT
    // Mechanism to maintain a list of toolchains (usually just one) by
    // collecting information from all compilation units.  Currently not
    // required, but kept in the code because it might become useful again in
    // the future and is not complete trivial to implement.
    
    // Add a toolchain.
    void add_toolchain(const std::string &path);

    // Remove a toolchain. Toolchains are counted: if a toolchain is added N
    // times, it must also be removed N times before it is no longer a
    // toolchain.
    void remove_toolchain(const std::string &path);

    // Return true iff the specified file is located in a toolchain folder
    bool is_toolchain_file(const File *file);
#endif

    // Report the current linker status; to be called by the linker
    void report_linker_status(LinkerStatus status);

    // Set the current project status. Report the new status when it changes.
    void set_project_status(ProjectStatus status);
    
    // If file is a linker script or makefile, reload it.
    void reload_as_config_file(File *file);

    // Set memory region;  report to client with callback.
    void set_memory_region(
      const std::string &name,
      bool present,
      size_t origin,
      size_t size
    );

    // Set memory section;  report to client with callback.
    void set_memory_section(
      const std::string &name,
      bool present,
      const std::string &runtime_region,
      const std::string &load_region
    );

    // Report an internal error. Override for PrefailAction. Also available for
    // use from other threads than the thread creating the project.
    void fail(const char *message) override;

    // Report whether a files is valid utf-8 or not. Initially, all files are
    // assumed to be valid UTF-8. All changes are reported.
    void report_utf8(File const *file, bool is_valid_utf8);

  protected:
    
    // Increment the pending file count and update linker status as needed.
    void inc_pending_file_count();

    // Decrement the pending file count and update linker status as needed.
    void dec_pending_file_count();

    // Increment the failed file count and update project status as needed.
    void inc_failed_file_count();

    // Decrement the failed file count and update project status as needed.
    void dec_failed_file_count();

    // Report a new analysis state for a file in this project.
    void report_file_analysis_status(
      File const *file, 
      AnalysisStatus old_status,
      AnalysisStatus new_status
    );
      
    // Add a diagnostic at the front of the list.
    void add_diagnostic_at_front(Diagnostic *diagnostic);

    // Add a diagnostic at the back of the list.
    void add_diagnostic_at_back(Diagnostic *diagnostic);

    // Add a diagnostic to the application.
    void add_diagnostic_before(Diagnostic *diagnostic, Diagnostic *before);
    void add_diagnostic_after(Diagnostic *diagnostic, Diagnostic *after);

    // Remove a diagnostic from the application.
    void remove_diagnostic(Diagnostic *diagnostic);

    // Show a diagnostic in the application.
    void show_diagnostic(Diagnostic *diagnostic);

    // Hide a diagnostic in the application.
    // Call the remove diagnostic callback.
    void hide_diagnostic(Diagnostic *diagnostic);

    // Call the more diagnostics callback
    void report_hidden_diagnostics(Severity severity);

    // Grab occurrence user data for tracking in file. This reports the
    // occurrence to the application if it was not reported yet, and increments
    // its reference count in the tracking_in_file_map.
    void *grab_tracking_in_file_user_data(Occurrence *occurrence);
    
    // Drop occurrence user data for tracking in file. This decrements its
    // reference count in the tracking_in_file_map, and reports disappearance of
    // the occurrence to the application if the reference count reaches zero.
    void drop_tracking_in_file_user_data(Occurrence *occurrence);

    void *report_occurrence_in_file(Occurrence *occurrence);
    void withdraw_occurrence_in_file(Occurrence *occurrence, void *user_data);
    
    void *report_occurrence_of_entity(Occurrence *occurrence);
    void withdraw_occurrence_of_entity(Occurrence *occurrence, void *user_data);

    // Report more or less occurrences to match current limit
    void check_occurrences_in_file_limit();
    void check_occurrences_of_entity_limit();

  private:
    const std::string project_path;
    const std::string cache_path;
    const std::string resource_path;
    const std::string lib_path;
    std::string toolchain_prefix;
    std::string build_path;
    std::string binary_analysis_command_name;
    void *user_data;

    const ProjectStatus_callback project_status_callback;
    const InclusionStatus_callback inclusion_status_callback;
    const LinkStatus_callback link_status_callback;
    const AnalysisStatus_callback analysis_status_callback;
    const AddSymbol_callback add_symbol_callback;
    const DropSymbol_callback drop_symbol_callback;
    const LinkerStatus_callback linker_status_callback;
    const HdirUsage_callback hdir_usage_callback;
    const AddDiagnostic_callback add_diagnostic_callback;
    const RemoveDiagnostic_callback remove_diagnostic_callback;
    const MoreDiagnostics_callback more_diagnostics_callback;
    const AddOccurrenceInFile_callback add_occurrence_in_file_callback;
    const RemoveOccurrenceInFile_callback remove_occurrence_in_file_callback;
    const OccurrencesInFileCount_callback occurrences_in_file_count_callback;
    const SetAlternativeContent_callback set_alternative_content_callback;
    const AddOccurrenceOfEntity_callback add_occurrence_of_entity_callback;
    const RemoveOccurrenceOfEntity_callback remove_occurrence_of_entity_callback;
    const OccurrencesOfEntityCount_callback occurrences_of_entity_count_callback;
    const SetOccurrenceOfEntityLinked_callback
      set_occurrence_of_entity_linked_callback;
    const ReportInternalError_callback report_internal_error_callback;
    const SetMemoryRegion_callback set_memory_region_callback;
    const SetMemorySection_callback set_memory_section_callback;
    const Utf8_callback utf8_callback;

    // Data members below can be accessed from a background thread as well as
    // from the main thread. Always lock the mutex before accessing them, and
    // unlock asap!

    // Path-to-file map.
    std::map<std::string, File*> _file_map;

    // Set of allocated global symbols, for uniquization.
    std::map<std::string, GlobalSymbol*> _global_symbols;

    // Path-to-hdir map.  This maps contains only hdirs that are actually used.
    // It may contain hdirs that were never added or removed by the application,
    // but were instead added by the makefile or the compiler.
    std::map<std::string, Hdir*> _hdir_map;

    // Set of manually added hdirs. These will override the hdirs in filetree.mk
    // for analysis purposes.
    std::set<base::ptr<Hdir>> _hdir_set;

    // Missing hfiles,  plus for each missing hfile a map from code analyzers 
    // in which it occurs to the number of occurrences
    std::map<std::string, std::map<Unit*, size_t>> _missing_headers;

    // Toolchains used in this project mapped to number of compilation units
    std::map<std::string, size_t> _toolchains;

    // Project status
    ProjectStatus project_status = ProjectStatus_ready;

    // Link status for the main function, used to control insertion and removal
    // of an "undefined main" diagnostic. The "undefined main" diagnostic is
    // special because there is no occurrence to attach it to. Initially, there
    // is no diagnostic, so initial status should be defined.
    LinkStatus main_link_status = LinkStatus_defined;

    // Diagnostic for link status of main function or null.
    base::ptr<Diagnostic> main_link_diagnostic;

    // Number of linker-relevant files waiting to be analyzed.
    // Linker-relevant files are files in automatic or include mode.
    unsigned _nr_pending_files = 0;

    // Number of linker-relevant files for which analysis failed.
    // Linker-relevant files are files in automatic or include mode.
    unsigned _nr_failed_files = 0;

    // Set of link names of global symbols used or defined in the current
    // project (i.e. with link status not equal to none). Used to efficiently
    // update symbol link status without iterating over *all* global symbols.
    std::set<std::string> _global_names;

    // De-duplication of project diagnostics (i.e. diagnostics not located in a
    // specific file).
    //
    // Diagnostic are values and cannot change. The map below holds one
    // diagnostic for each value currently in use.  The get_diagnostic(...)
    // method returns an existing or new diagnostic with a given value.
    //
    // Diagnostics are reference-counted. References can be located in a
    // compilation unit or an analysis. When the reference count goes to zero,
    // the diagnostic destructor executes and removes itself from the map.
    //
    // Diagnostics can only be added and removed while the project is locked.
    struct DiagnosticKey
    {
      std::string message;
      Severity severity;
      Category category;
      
      bool operator<(const DiagnosticKey &other) const;
    };
    std::map<DiagnosticKey, Diagnostic*> _diagnostic_map;

    // Maximum number of diagnostics that can be reported in total to the
    // application.
    size_t diagnostic_limit[NumberOfSeverities] =
      { (size_t)-1, (size_t)-1, (size_t)-1 };

    // Maximum number of diagnostics that can still be reported to the
    // application.  These counters are decremented everytime a diagnostic is
    // reported. They are incremented everytime a diagnostic is removed.
    size_t diagnostic_budget[NumberOfSeverities] =
      { (size_t)-1, (size_t)-1, (size_t)-1 };

    // Number of hidden diagnostics.
    size_t hidden_diagnostic_count[NumberOfSeverities] = { 0, 0 };
    bool hidden_diagnostic_count_changed[NumberOfSeverities] = { false, false };

    // Chain of diagnostics
    Diagnostic diagnostic_chain[NumberOfSeverities];

    // First hidden diagnostic
    Diagnostic *first_hidden_diagnostic[NumberOfSeverities];

    // Map of occurrences - identified by their pointer - to diagnostics
    // for that occurrence, used for example for undefined and multiply defined
    // symbols.  Since these diagnostics are expected to be relatively rare, a
    // map is expected to be more efficient than adding a usually empty
    // list of diagnostics to each occurrence and/or symbol.
    //
    // This is a map, not a multimap, so at most one diagnostic per occurrence
    // is supported (asserted). Should the need arise, a multimap can be used
    // instead.
    std::map<Occurrence*, base::ptr<Diagnostic>> diagnostics;

    // Map of occurrence to user data for tracked occurrences of an entity.
    //
    // It is expected that only a limited number of occurrences will be tracked
    // at any given time.  Adding a user data field to each occurrence increases
    // memory usage for a limited potential performance gain.  A map with only
    // entries for tracked occurrences will (usually) be beneficial for memory
    // usage, and possibly indirectly also for performance.
    //
    // For performance reasons, we limit the number of tracked occurrences that
    // are reported. We keep unreported occurrences in a separate set, so that
    // they can be reported when other reported occurrences are removed or the
    // limit is increased.
    std::map<Occurrence*, void*> _reported_of_entity_map;
    std::set<Occurrence*> _unreported_of_entity_set;

    // Maximum number of occurrences of an entity to be reported.
    size_t _reported_of_entity_limit = SIZE_MAX;
    
    // Map of occurrence to user data for tracking occurrences in a file.
    //
    // It is expected that only a limited number of occurrences will be tracked
    // at any given time.  Adding a user data field to each occurrence increases
    // memory usage for a limited potential performance gain.  A map with only
    // entries for tracked occurrences will (usually) be beneficial for memory
    // usage, and possibly indirectly also for performance.
    //
    // Occurrences potentially tracked in a file include all occurrences nested
    // directly or indirectly in a definition in that file. This may include
    // occurrences in another file if there is a #include within the scope of a
    // definition.
    //
    // The value of the map is a (user_data, count) pair, where the count is one
    // for tracked occurrences plus one for each time an occurrence of a nested
    // symbol is tracked.
    //
    // For performance reasons, we limit the number of tracked occurrences that
    // are reported. We keep unreported occurrences in a separate map, so that
    // they can be reported when other reported occurrences are removed or the
    // limit is increased.
    std::map<Occurrence*, std::pair<void*, size_t>> _reported_in_file_map;
    std::map<Occurrence*, size_t> _unreported_in_file_map;
    
    // Maximum number of occurrences in a file to be reported.
    size_t _reported_in_file_limit = SIZE_MAX;
    
    std::string log_file;
    std::ofstream log_stream;
  };

  class InternalErrorAction: public base::PrefailAction
  {
  public:
    Project *const project;
    InternalErrorAction(Project *project): project(project) {}

    void fail(const char *message) override
    {
      project->fail(message);
    }

    void fail() override
    {
      project->fail("");
    }
  };
}

#endif
