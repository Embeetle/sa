// Copyright 2018-2024 Johan Cockx
#include "source_analyzer.h"
#include <stdio.h>

// The Py_DecRef function will be defined in the python main code that will load
// the Clang engine shared object. It will not be found while linking the shared
// object.  To avoid linking errors when testing for undefined globals, define
// it here.
class PyObject;
extern "C" void Py_DecRef(PyObject*)
{
}

int main()
{
  printf("%p\n",sa::ProjectStatus_names);
  printf("%p\n",sa::ce_project_status_name);
  printf("%p\n",sa::LinkerStatus_names);
  printf("%p\n",sa::ce_linker_status_name);
  printf("%p\n",sa::FileMode_names);
  printf("%p\n",sa::ce_file_mode_name);
  printf("%p\n",sa::ce_file_kind_name);
  printf("%p\n",sa::InclusionStatus_names);
  printf("%p\n",sa::ce_inclusion_status_name);
  printf("%p\n",sa::AnalysisStatus_names);
  printf("%p\n",sa::ce_analysis_status_name);
  printf("%p\n",sa::EntityKind_names);
  printf("%p\n",sa::ce_entity_kind_name);
  printf("%p\n",sa::OccurrenceKind_names);
  printf("%p\n",sa::ce_occurrence_kind_name);
  printf("%p\n",sa::LinkStatus_names);
  printf("%p\n",sa::ce_link_status_name);
  printf("%p\n",sa::ce_create_project);
  printf("%p\n",sa::ce_drop_project);
  printf("%p\n",sa::ce_set_toolchain_prefix);
  printf("%p\n",sa::ce_get_file_handle);
  printf("%p\n",sa::ce_drop_file_handle);
  printf("%p\n",sa::ce_get_file_project);
  printf("%p\n",sa::ce_get_file_path);
  printf("%p\n",sa::ce_get_file_mode);
  printf("%p\n",sa::ce_get_file_kind);
  printf("%p\n",sa::ce_get_file_user_data);
  printf("%p\n",sa::ce_add_file);
  printf("%p\n",sa::ce_set_file_mode);
  printf("%p\n",sa::ce_remove_file);
  printf("%p\n",sa::ce_track_occurrences_in_file);
  printf("%p\n",sa::ce_track_occurrences_of_entity);
  printf("%p\n",sa::ce_edit_file);
  printf("%p\n",sa::ce_reload_file);
  printf("%p\n",sa::ce_find_occurrence);
  printf("%p\n",sa::ce_set_diagnostic_limit);
  printf("%p\n",sa::ce_set_number_of_workers);
  printf("%p\n",sa::ce_start);
  printf("%p\n",sa::ce_stop);
  printf("%p\n",sa::ce_abort);
}
