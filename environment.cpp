#include "environment.h"
#include "base/os.h"

static void *create_standard_environment()
{
  std::map<std::string, std::string> env = base::os::current_env();
  env["LANGUAGE"] = "en_US";
  env["LC_ALL"] = "en_US.UTF-8";
  return base::os::create_native_env(env);
}

void *sa::standard_environment = create_standard_environment();
