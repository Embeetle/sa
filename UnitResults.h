// Copyright 2018-2024 Johan Cockx
#ifndef __UnitResults_h
#define __UnitResults_h

namespace sa {

  struct UnitResults {
    bool success;
    
    std::vector<base::ptr<Occurrence>> occurrences;

    std::vector<base::ptr<Diagnostic>> diagnostics;

    // A linker relevant error is an error that affects the occurrence of global
    // symbols in the compilation unit.  This flag is set if there is at least
    // one linker relevant error.
    bool has_linker_relevant_error = false;

    std::map<Occurrence*, sa::Scope> scope_data;

    std::vector<std::string> missing_headers;

    std::vector<base::ptr<Section>> sections;

    std::vector<base::ptr<EmptyLoop>> empty_loops;

    // Files used in this unit that contain at least one non UTF8 character.
    std::set<base::ptr<File>> non_utf8_files;

    bool from_cache;

    bool has_alternative_content;

    std::string alternative_content;
  };

}

#endif
