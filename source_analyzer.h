// Copyright 2018-2024 Johan Cockx
#ifndef __source_analyzer_h
#define __source_analyzer_h

// Source analyzer API, providing functions to analyze C and C++ source code.
//
// This API uses "handles" to refer to projects, source files and symbols.  A
// handle is a pointer to an abstract struct: a struct that is declared but not
// defined in this header.  The fields of an abstract struct are private to the
// implementation of the source analyzer and cannot be accessed by an application
// using the API.
//
// Once a user application has obtained a handle, the handle remains valid until
// the user application explicitly drops it. If the underlying object for a
// handle is no longer in use by the source analyzer, the source analyzer uses a
// callback to inform the user of this fact. It is then up to the user to
// release the handle and thus free the corresponding memory in the source
// analyzer.  With this approach, we hope to avoid crashes due to the use of
// handles for objects that no longer exist.
//
// Callback implementation rules
// -----------------------------
//
// Unless documented differently, the project is locked during callbacks.
// Implementation should be fast to avoid delaying background processing.
// Implementation must not call functions that lock the project to avoid
// deadlock.
//
// One exception to this rule is the update flags callback. This callback is
// expected to be slow, so the project is not locked during its execution.

#include "EntityKind.h"
#include "OccurrenceKind.h"
#include "AnalysisStatus.h"
#include "LinkerStatus.h"
#include "LinkStatus.h"
#include "InclusionStatus.h"
#include "FileMode.h"
#include "FileKind.h"
#include "Severity.h"
#include "Category.h"
#include "OccurrenceData.h"
#include <iostream>

namespace base {
  struct Checked;
}

namespace sa {

  // An abstract struct representing an entity that is a project, a file or a
  // symbol (function, variable, type, ...).
  struct Entity;

  // An abstract struct representing a project.  For the purposes of the source
  // analyzer, a project consists of a list of source files.
  struct Project;

  // An abstract struct representing a source file in the context of a project.
  // Source files are C or C++ code or header files that have been analyzed.
  // Code files are added to a project when requested by the API user.  Header
  // files are added during analysis of a code file.
  struct File;

  // An abstract struct representing a symbol (function, variable, type, ...).
  // Symbols are added to a project during analysis of a source file; they
  // cannot be added or changed by the application using the API.
  struct Symbol;

  // An abstract struct representing an occurrence of an entity in a file.
  struct Occurrence;

  // An abstract struct representing a diagnostic. Diagnostics are added to a
  // project during analysis of a source file; they cannot be added or changed
  // by the application using the API.
  struct Diagnostic;

  // Project status enum
  typedef enum {
    // Source code analysis is ready; file inclusion status, occurrences and
    // diagnostics are final, and will not change until some external event
    // occurs (file added, file mode changed, hdir added or removed, ...).
    ProjectStatus_ready,
    
    // Source code analysis is in progress; file inclusion status, occurrences
    // and diagnostics are not final yet. Split into option-extraction,
    // source-analysis and linking?
    ProjectStatus_busy,

    // Analysis failed for at least one file. File inclusion status, occurrences
    // and diagnostics are probably incomplete or incorrect. They will not
    // change until some external event occurs (file added, file mode changed,
    // hdir added or removed, ...).
    ProjectStatus_failed,
  } ProjectStatus;
  static const char * const ProjectStatus_names[] = {
    "ready", "busy", "failed"
  };
  // Return the name of a given linker status.
  extern "C" const char *ce_project_status_name(ProjectStatus value);

  // Type of callback function called when the project status changes.
  //
  // Parameters:
  // - 'project': the project handle
  // - 'status': the new project status
  //
  typedef void (*ProjectStatus_callback)(
    Project *project,
    ProjectStatus status
  );

  // Return the name of a given linker status.
  extern "C" const char *ce_linker_status_name(LinkerStatus value);

  // Type of callback function called when the linker status of a project
  // changes
  //
  // Parameters:
  // - 'project': the project handle
  // - 'status': the new link status of the project
  //
  typedef void (*LinkerStatus_callback)(
    Project *project,
    LinkerStatus status
  );

  // Return the name of a given entity kind.
  extern "C" const char *ce_entity_kind_name(EntityKind kind);

  // Type of callback function called to create user data for a symbol.
  //
  // Parameters:
  // - 'name': the symbol name
  // - 'kind': the symbol kind (an entity kind)
  // - 'symbol': the symbol handle
  //
  // Return the new user data. Returned user data will be stored on the symbol
  // and reused until the application drops it.
  //
  // Symbol user data is used:
  // - in the return value of find_occurrence calls,
  // - in add_occurrence_in_file callbacks, and
  // - in add_occurrence_of_entity callbacks, and
  // - in set_occurrence_linked callbacks
  //
  // For each such call, a counter on the symbol is incremented. The counter is
  // decremented when the application calls ce_drop_entity(symbol).  When it
  // reaches zero, the drop symbol callback notifies the application that it
  // will no longer be reused,  and that the symbol handle is no longer valid.
  //
  // This relatively complex mechanism to manage symbol handles and user data
  // avoids race conditions in a multi-threaded context.  All changes are
  // synchronized internally in the source analyzer.
  //
  typedef void *(*AddSymbol_callback)(
    const char *name, EntityKind kind, Symbol *symbol
  );

  // Type of callback function called to notify the application that user data
  // for a symbol will no longer be reused. After this call, the analyzer may
  // delete the symbol, so the corresponding symbol handle of the add symbol
  // callback is no longer guaranteed valid. See add symbol callback for
  // details.
  typedef void (*DropSymbol_callback)(void *symbol_user_data);

  // Return the name of a given diagnostic severity.
  extern "C" const char *ce_severity_name(Severity value);

  // Return the name of a given diagnostic category.
  extern "C" const char *ce_category_name(Category value);

  // Return the name of a given file mode.
  extern "C" const char *ce_file_mode_name(FileMode value);

  // Return the name of a given file kind.
  extern "C" const char *ce_file_kind_name(FileKind value);

  // Return the name of a given inclusion status.
  extern "C" const char *ce_inclusion_status_name(InclusionStatus value);

  // Return the name of a given analysis status.
  extern "C" const char *ce_analysis_status_name(AnalysisStatus value);
    
  // Return the name of a given occurrence kind.
  extern "C" const char *ce_occurrence_kind_name(OccurrenceKind value);

  // Return the name of a given link status for a symbol.
  extern "C" const char *ce_link_status_name(LinkStatus value);
    
  // Type of callback function called when a new diagnostic is found.
  //
  // Parameters:
  // - 'message': text of the diagnostic
  // - 'severity': diagnostic severity
  // - 'category': diagnostic category
  // - 'path': path of the file containing the diagnostic
  // - 'file_user_data': user data for the file containing the diagnostic
  // - 'offset': zero based byte offset of the diagnostic in the file
  // - 'after_user_data': user data of the previously added diagnostic with
  //      the same severity after which the new diagnostic should be inserted,
  //      or null if the new diagnostic should be inserted first.
  //
  // Return value: user data for the diagnostic, to be used when the remove
  // diagnostic callback is called later.
  //
  typedef void *(*AddDiagnostic_callback)(
    const char *message,
    Severity severity,
    Category category,
    const char *path,
    void *file_user_data,
    unsigned offset,
    void *after_user_data
  );

  // Type of callback function called when a diagnostic is removed.
  //
  // Parameters:
  // - 'diagnostic_user_data': the user data set by the application when the
  //                           diagnostic was added
  //
  typedef void (*RemoveDiagnostic_callback)(void *user_data);

  // Type of callback function called when the number of unreported diagnostics
  // changes.
  //
  // Parameters:
  // - 'project': the project for which the number changes
  // - 'severity': the severity for which the number changes
  // - 'number_of_unreported_diagnostics': the new number of diagnostics
  //
  typedef void (*MoreDiagnostics_callback)(
    Project *project,
    Severity severity,
    unsigned number_of_unreported_diagnostics
  );
  
  // Type of callback function called to add a tracked occurrence in a file.
  //
  // Parameters:
  // - 'path': path of the file containing the occurrence
  // - 'offset': byte offset of the first byte of the occurrence in the file
  // - 'end_offset': byte offset of the first byte beyond the occurrence
  // - 'kind': occurrence kind
  // - 'entity_user_data': user data for the occurring entity (file or symbol)
  // - 'entity': the occurring entity
  // - 'scope_user_data': user data for the (previously added) definition
  //                      containing this entity, or null if not nested
  //
  // Return: user data for the occurrence or null.
  //
  typedef void *(*AddOccurrenceInFile_callback)(
    const char *path,
    unsigned offset,
    unsigned end_offset,
    OccurrenceKind kind,
    void *entity_user_data,
    Entity *entity,
    void *scope_user_data,
    bool linked
  );

  // Type of callback function called to remove a tracked occurrence in a file
  // with non-null user data.
  //
  // Parameters:
  // - 'occurrence_user_data': the occurrence user data.
  //
  typedef void (*RemoveOccurrenceInFile_callback)(void *occurrence_user_data);

  // Type of callback function called to report the number of tracked
  // occurrences in a file.
  //
  // Parameters:
  // - 'count': the number of occurrences.
  //
  typedef void (*OccurrencesInFileCount_callback)(unsigned count);

  // Type of callback function called to set alternative content fir a binary
  // file.
  //
  // Parameters:
  // - 'path': path of the binary file
  // - 'content': path of the file containing the occurrence
  //
  typedef void (*SetAlternativeContent_callback)(
    const char *path,
    void *user_data,
    const char *content
  );

  // Type of callback function called to add a tracked occurrence of an entity.
  //
  // Parameters:
  // - 'path': path of the file containing the occurrence
  // - 'offset': byte offset of the first byte of the occurrence in the file
  // - 'end_offset': byte offset of the first byte beyond the occurrence
  // - 'kind': occurrence kind
  // - 'entity_user_data': user data for the occurring entity (file or symbol)
  // - 'entity': the occurring entity
  //
  // Return: user data for the occurrence or null.
  //
  typedef void *(*AddOccurrenceOfEntity_callback)(
    const char *path,
    unsigned offset,
    unsigned end_offset,
    OccurrenceKind kind,
    void *entity_user_data,
    Entity *entity,
    bool linked
  );

  // Type of callback function called to remove a tracked occurrence of an entity
  // with non-null user data.
  //
  // Parameters:
  // - 'occurrence_user_data': the occurrence user data.
  //
  typedef void (*RemoveOccurrenceOfEntity_callback)(void *occurrence_user_data);

  // Type of callback function called to report the number of tracked
  // occurrences of an entity.
  //
  // Parameters:
  // - 'count': the number of occurrences.
  //
  typedef void (*OccurrencesOfEntityCount_callback)(unsigned count);

  // Type of callback function called to change the linked status of a tracked
  // occurrence of an entity with non-null user data.
  //
  // Parameters:
  // - 'occurrence_user_data': the occurrence user data.
  // - 'linked': true iff the occurrence is linked
  //
  typedef void (*SetOccurrenceOfEntityLinked_callback)(
    void *occurrence_user_data,
    bool linked
  );

  // Type of callback function called when a file's inclusion status changes.
  // The inclusion status is non-zero if the file is #include'd from a linked
  // source file, or included on the link command line, or included implicitly
  // by the toolchain. The inclusion status is not affected by the file mode
  // (include/exclude/automatic); it is always determined automatically by the
  // SA.
  //
  // Parameters:
  // - 'path': the file's normalized path
  // - 'user_data': the file's user data
  // - 'status' is non-zero iff this file is included
  //
  typedef void (*InclusionStatus_callback)(
    const char *path,
    void *user_data,
    unsigned status
  );

  // Type of callback function called when a file's link status changes.  The
  // link status is non-zero if the file is either force-included or selected by
  // the SA linker to be linked into the elf file. Linked files are C/C++/asm
  // files or archive or object files that cannot be recreated by commands in
  // the makefile. It is possible though rare that a file is both included and
  // linked.
  //
  // Parameters:
  // - 'path': the file's normalized path
  // - 'user_data': the file's user data
  // - 'status' is non-zero iff this file is the main file of a compilation unit
  //            that is included in the elf file.
  //
  typedef void (*LinkStatus_callback)(
    const char *path,
    void *user_data,
    unsigned status
  );

  // Type of callback function called when the usage status of an hdir changes
  //
  // Parameters:
  // - 'path': the hdir path, ending with a forward slash '/'.
  // - 'status': the new usage status: included or excluded.
  //
  typedef void (*HdirUsage_callback)(const char *path, InclusionStatus status);

  // Type of callback function called when the analysis status of a file changes.
  //
  // Parameters:
  // - 'path': the file's normalized path
  // - 'user_data': the file's user data
  // - 'old_status': the old analysis status of the file
  // - 'new_status': the new analysis status of the file
  //
  typedef void (*AnalysisStatus_callback)(
    const char *path,
    void *user_data,
    AnalysisStatus old_status,
    AnalysisStatus new_status
  );

  // Type of callback function called when an internal error occurs in a
  // background thread.
  //
  // Parameters:
  // - 'message': a string describing the problem
  // - 'user_data': the user data for the project.
  //
  typedef void (*ReportInternalError_callback)(
    const char *message,
    void *user_data
  );

  // Type of callback function called to report memory regions found in the
  // linker script.
  //
  // Parameters:
  // - 'name': the name of the memory region
  // - 'present': true iff the memory region with that name is present
  // - 'origin': the origin of the memory region if present
  // - 'size': the size of the memory region if present
  //
  typedef void (*SetMemoryRegion_callback)(
    const char *name,
    bool present,
    unsigned origin,
    unsigned size
  );

  // Type of callback function called to report memory sections found in the
  // linker script.
  //
  // Parameters:
  // - 'name': the name of the memory section
  // - 'present': true iff the memory section with that name is present
  // - 'origin': the origin of the memory section if present
  // - 'size': the size of the memory section if present
  //
  typedef void (*SetMemorySection_callback)(
    const char *name,
    bool present,
    const char *runtime_region,
    const char *load_region
  );

  // Type of callback function called when the utf8 status of a file changes
  //
  // Parameters:
  // - 'path': the file's normalized path
  // - 'user_data': the file's user data
  // - 'is_valid_utf8': true iff the file is valid UTF-8
  //
  typedef void (*Utf8_callback)(
    const char *path,
    void *user_data,
    bool is_valid_utf8
  );

  // Create a project in the source analyzer.
  //
  // Parameters:
  //
  // - 'project_path': the absolute path of the project folder. The project
  //   folder contains all source files for the project, except for toolchain
  //   headers.  Moving the project folder to a different location will not
  //   invalidate the cache, as long as the path of source files relative to the
  //   project folder remains the same.
  //
  // - 'cache_path': the path of the cache folder used to store cached analysis
  //   results. The path must be either absolute or relative to the project
  //   folder.
  //
  // - 'resource_path': the absolute path of the folder containing platform
  //   independent resource files for the source analyzer. The
  //   <resource_path>/include subfolder contains the Clang-specific system
  //   header files. These are required and must match the Clang version in
  //   use. They will include the toolchain headers when needed.
  //
  // - 'lib_path': the absolute path of the folder containing platform dependent
  //   resource files for the source analyzer. This includes patched version of
  //   `clang` used for assembly analysis.
  //
  // - 'project_status_callback': called when the project status changes.  Also
  //   called immediately to initialize the project status. Not called if null.
  //
  // - 'add_file_callback': called to obtain a handle for the file with the
  //   given path.
  //
  //   New file handles can be created using the ce_add_file function declared
  //   further. Once a file with a given path has been added to the project,
  //   this callback *must* return the handle for that file when called for that
  //   path.  It may also return that file handle for other paths: this can be
  //   used to normalize the path before looking up its handle, so that all
  //   paths referring to the same file return the same handle.  This callback
  //   is required and cannot be null. It must always return a valid file
  //   handle, even if the given path does not refer to an existing file.
  //
  // - 'add_symbol_callback': called to create user data for a symbol before it
  //   is referenced in an occurrence passed to the application.  This allows
  //   the application to attach user data to the symbol. The user data will be
  //   used in the occurrence to refer to the symbol.
  //
  // - 'linker_status_callback': called when the link status of the project
  //   changes.  Also called immediately to initialize the project's link
  //   status. Not called if null.
  //
  // - 'hdir_usage_callback': called when the usage of an hdir (include
  //   directory) changes.
  //
  // - 'add_diagnostic_callback': called when a new diagnostic message is found
  //
  // - 'remove_diagnostic_callback': called to remove a diagnostic message
  //
  // - 'more_diagnostics_callback': called when the number of unreported
  //   diagnostics changes.
  //
  // - 'add_occurrence_in_file_callback': called when a new occurrence matching
  //   occurrence tracking criteria of its file is found.
  //
  // - 'remove_occurrence_in_file_callback': called to remove a tracked
  //   occurrence matching tracking criteria in its file.
  //
  // - 'occurrences_in_file_count_callback': called to report the total number
  //   of tracked occurrences in a file, including hidden ones.
  //
  // - 'set_occurrence_linked_callback': called when a tracked occurrence changes
  //   its 'linked' status.
  //
  // - 'set_alternative_content_callback': called to set alternative content for
  //   binary files.
  //
  // - 'add_occurrence_of_entity_callback': called when a new occurrence matching
  //   occurrence tracking criteria of its entity is found.
  //
  // - 'remove_occurrence_of_entity_callback': called to remove a tracked
  //   occurrence matching tracking criteria in its entity.
  //
  // - 'occurrences_of_entity_count_callback': called to report the total number
  //   of tracked occurrences of an entity, including hidden ones.
  //
  // - 'report_internal_error_callback': called when an internal error occurs
  //   in a background thread. Internal errors are not recoverable, so the
  //   source analyzer will stop to function.  It is advisable to save all
  //   changes and restart Embeetle after this callback.
  //
  // - 'set_memory_region_callback': called to add, change or remove a memory
  //   region.
  //
  // - 'set_memory_section_callback': called to add, change or remove a memory
  //   section.
  //
  // - 'user_data': the user data for this project.
  //
  // Returns: a handle for the new project. 
  //
  extern "C" Project *ce_create_project(
    const char *project_path,
    const char *cache_path,
    const char *resource_path,
    const char *lib_path,
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

  // - 'toolchain_prefix': the string to be preprended to a tool name like 'nm'
  //   or 'objdump'.
  //
  // Temporarily locks the project.
  extern "C" void ce_set_toolchain_prefix(Project *project, const char *prefix);

  // Set the path of the directory in which `make` will be started.
  // Temporarily locks the project.
  extern "C" void ce_set_build_path(Project *project, const char *path);

  extern "C" const char *ce_get_binary_analysis_command_arg(
    Project *project, unsigned index
  );

  // Delete a source analyzer project and release memory used by analysis data.
  // This invalidates the project handle and delete all analysis data.
  //
  // Temporarily locks the project.
  extern "C" void ce_drop_project(Project *project);

  // Inform the project that an hdir was added for flag extraction.  This may or
  // may not trigger a change in the compilation flags.  If it does, assume that
  // it adds a -I option.  This will only affect files with missing headers that
  // can be found in the new hdir; the analysis of other files remains valid.
  // Temporarily locks the project.
  extern "C" void ce_add_hdir(Project *project, const char *path);

  // Inform the project that an hdir was removed for flag extraction.  This may
  // or may not trigger a change in the compilation flags.  If it does, assume
  // that it removes a -I option.  This will only affect files including a file
  // from the removed hdir; the analysis of other files remains
  // valid. Temporarily locks the project.
  extern "C" void ce_remove_hdir(Project *project, const char *path);

  // Get a file by path. Create it - with header mode - if it doesn't exist.
  // Temporarily locks the project. The file will be kept alive at least until a
  // matching ce_drop_file call.
  extern "C" File *ce_get_file_handle(Project *project, const char *path);
  
  // Drop a file. Temporarily locks the project.
  //
  // Call this when the application no longer intends to access this file.  It
  // invalidates the file handle and allows the source analyzer to remove
  // resources associated with the file when appropriate.
  //
  // To remove a file from a project, set its mode to 'header' and drop it.  If
  // the file mode is 'header', it will be removed when it is no longer included
  // by any source file included in the project, and it will be automatically
  // re-inserted into the project when another source file includes it again.
  // If the file mode is anything other than 'header', it will remain part of
  // the project.
  extern "C" void ce_drop_file_handle( File *file );

  // Get the project of a given file.
  extern "C" Project *ce_get_file_project( File *file );

  // Get the path of a given file.
  extern "C" const char *ce_get_file_path( File *file );

  // Get the mode of a given file.
  extern "C" FileMode ce_get_file_mode( File *file );

  // Get the kind of a given file.
  extern "C" FileKind ce_get_file_kind( File *file );

  // Get the user data of a given file.
  extern "C" void *ce_get_file_user_data( File *file );

  // Notify the source analyzer of a new file, and set the file's mode and user
  // data. User data will be used in callbacks. Reloads files with missing
  // headers matching this file. Temporarily locks the project.
  //
  // It is possible that the file was already known to the source analyzer,
  // because it was included by another file or because of a previous
  // ce_add_file call. In that case, just set the file's mode and user data. To
  // only change the mode, the ce_set_file_mode function is more efficient,
  // because it doesn't need to check for changed user data or for missing
  // headers.
  //
  // Parameters:
  //
  // - 'file': the file
  //
  // - 'mode': the new mode
  //
  // - 'user_data': user data to be used in callbacks related to the file
  //
  extern "C" void ce_add_file(File *file, FileMode mode, void *user_data);

  // Set file mode. Temporarily locks the project.
  //
  // Parameters:
  //
  // - 'file': the file
  //
  // - 'mode': the new mode
  //
  extern "C" void ce_set_file_mode(File *file, FileMode mode);

  // Notify the source analyzer of a removed file.  Reload includers.
  // Temporarily locks the project.
  //
  // This does not directly remove the file object in the source analyzer, but
  // sets the user data to null and the mode to exclude. The file object will be
  // cleaned up as soon as all includers have been re-analyzed and have removed
  // the file from their lists of included files.
  //
  // Parameters:
  //
  // - 'file': the file to be removed
  //
  extern "C" void ce_remove_file(File *file);

  // Track occurrences in file. Track only occurrences for which the kind is in
  // the given set of occurrence kinds and the entity kind is in the given set
  // of entity kinds. The sets are given as bit-sets: a kind is included in the
  // set if (1<<kind & set). Temporarily locks the project.
  extern "C" void ce_track_occurrences_in_file(
    File *file, unsigned occurrence_kinds, unsigned entity_kinds
  );

  // Track occurrences of entity. Track only occurrences for which the kind is
  // in the given set of occurrence kinds. The set is given as a bit-set: a
  // value is included in the set if (1<<value & set).  Temporarily locks the
  // project.
  extern "C" void ce_track_occurrences_of_entity(
    Entity *entity, unsigned occurrence_kinds
  );

  // Set make command
  extern "C" void ce_set_make_command(
    Project *project,
    const char *command_buffer,
    unsigned command_size
  );

  // For debugging.  Return true iff a file's analysis data was extracted from
  // cache.  Return false for files that are not analyzed.
  extern "C" bool ce_analysis_data_was_read_from_cache(File *file);

  // Edit a range of characters in an in-memory representation of a file.
  //
  // Parameters:
  // - 'file': the file to be edited
  // - 'begin': zero based byte offset of the first changed character
  // - 'end': zero based byte offset of the first unchanged character
  // - 'new_content': new content of the file at [begin..end)
  //
  // Replace the content of the file at [begin..end) by new
  // content, possibly with a different length.  Characters before 'begin'
  // and characters at or after 'end' are unchanged.
  //
  // After this call, symbol occurrences at or after 'end' are patched, but the
  // new content is not analyzed.  Use analyze_file to analyze the new content.
  extern "C" void ce_edit_file(
    File *file,
    unsigned begin,
    unsigned end,
    const char *new_content
  );

  // Reload a file from disk. After loading from disk, analyze the file in the
  // background.  Call this when you know that a file has changed, for example
  // because the user saved it. Temporarily locks the project.
  extern "C" void ce_reload_file(File *file);

  // Get the occurrence overlapping a given range in a file
  // Temporarily locks the project.
  //
  // Parameters:
  //  - 'file': the file in which to look for an occurrence.
  //  - 'offset': zero-based byte index of the start of the range
  //  - 'begin_tol': the tolerance at the beginning of an occurrence; an
  //       occurrence will report a match from begin_tol characters before the
  //       range of the occurrence.
  //  - 'end_tol': the tolerance at the end of an occurrence; an
  //       occurrence will report a match up to end_tol characters after the
  //       range of the occurrence.
  //
  // Return: an occurrence struct. If there is no occurrence at the given
  //   location, return the null occurrence.
  //
  extern "C" OccurrenceData ce_find_occurrence(
    File *file,
    unsigned offset,
    unsigned begin_tol,
    unsigned end_tol
  );

  // Get included symbols with a given name.
  //
  // The callback function is called for each symbol.
  //
  // The caller needs to make sure that drop_symbol is called for each symbol
  // when it is no longer needed.
  extern "C" void ce_find_symbols(
    Project *project,
    const char *name,
    void (*find_symbol)(void *symbol_user_data, Entity *symbol, void *user_data),
    void *user_data
  );
  
  // Get suggestions for completion at given source file location.
  //
  // The callback function is called for each suggestion.
  //
  // Currently, this function also takes some context from the source file as
  // input.  In the future, file contents will be maintained in the SA, and this
  // will no longer be necessary.
  //
  // Parameters:
  //  - 'file': the file in which completion is requested
  //  - 'pos': byte offset in file at which completion is requested.
  //       Suggested completions are intended to be inserted at that position.
  //  - 'add_completion': callback, called for each completion
  //  - 'user_data': user data to be passed to callback
  //  - 'context': some context before the completion,  e.g. the line containing
  //       the completion position up to the completion position.
  //
  // Return: byte offset in file at which completions should be inserted.  This
  //   can be at or before the position at which completion is requested.  If
  //   before, the completion replaces the existing text between insertion
  //   position and completion position.
  //
  extern "C" unsigned ce_get_completions(
    File *file,
    unsigned pos,
    void (*add_completion)(const char *completion, void *user_data),
    void *user_data,
    const char *context
  );

  // Get the range of an empty loop overlapping the given offset, or else an
  // empty range.  Temporarily locks the project.
  //
  // Parameters:
  //  - 'file': the file in which to look for an empty loop.
  //  - 'offset': zero-based byte index
  //
  // Return: a range struct, with begin and end fields.
  //
  struct RangeData {
    unsigned begin;
    unsigned end;
  };
  extern "C" RangeData ce_find_empty_loop(File *file, unsigned offset);

  // Drop an entity handle.  To avoid a memory leak, every entity handle
  // returned by get_occurrence_at_location must be dropped when it will no
  // longer be used.
  extern "C" void ce_drop_entity_handle(Entity *entity);

  // Set the maximum number of diagnostics to be reported.
  extern "C" void ce_set_diagnostic_limit(
    Project *project, Severity severity, unsigned limit
  );

  // Set number of worker threads for source analysis.
  extern "C" void ce_set_number_of_workers(unsigned n);

  // Start worker threads for source analysis. No-op if already started.
  extern "C" void ce_start();

  // Stop worker threads for source analysis. No-op if already stopped.  No
  // tasks are aborted. Whether running tasks are pauzed or run to completion is
  // unspecified.
  extern "C" void ce_stop();

  // Abort all running source analysis tasks, and cancel waiting tasks. To be
  // called before program termination. Source analysis cannot be restarted.
  extern "C" void ce_abort();

  // Assert pointer validity
  extern "C" void ce_check(base::Checked *pointer);

  inline std::ostream &operator<<(std::ostream &out, sa::ProjectStatus status)
  {
    return out << sa::ce_project_status_name(status);
  }
}
    
#endif
