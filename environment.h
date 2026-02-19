#ifndef __sa_environment_h
#define __sa_environment_h

namespace sa {

  // Standard environment for base::os::execute_and_capture when executing
  // commands for source analysis.  Specifically sets LANGUAGE and LC_ALL
  // to avoid localization dependencies.
  extern void *standard_environment;

}

#endif
