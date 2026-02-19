from template import *
import time
import os
import shutil

eprint("Hello")
top_dir = "test/asm"
build_dir = top_dir + "/build"
source_dir = top_dir + "/source"
cache_dir = top_dir + "/cache"
if os.path.exists(top_dir):
    try:
        shutil.rmtree(top_dir)
    except Exception as error:
        eprint(error)
        sys.exit(1)
eprint("test dir: " + test_dir)
shutil.copytree(test_dir + "/asm", top_dir)
makefile = '../makefile'
project = Project(build_dir, makefile, source_dir)

project.add_file("main.c", ce.FileMode.AUTOMATIC)
project.add_file("nordic.S", ce.FileMode.AUTOMATIC)
#project.add_file("gcc_startup_nrf52840.S", ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
print(f"linker status: {ce.linker_status_name(project.linker_status)}")
# There will be undefined globals at this point, because we are not using an
# apropriate linker script.
#assert project.linker_status == ce.LinkerStatus.DONE

project.add_file("a.S", ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
project.print_diagnostics()
#assert project.get_diagnostic_count() # More than one, due to undefined globals
#eprint(f'First error: {project.get_diagnostics()[0]}')
#assert project.get_diagnostics()[0] == "a.S:43: error: unknown directive"

#assert project.get_diagnostic_count() == 0

#assert project.hdir_used('inc')
#assert project.file_linked("simple_main.c")
#ref = project.click("simple.c", 5)
#assert ref
#assert ref.kind == ce.occurrence_kind_include
#assert project.standard_source_path(ref.path) == "simple.h"
#assert ref.symbol.kind() == ce.entity_kind_global_function
#assert ref.symbol.name() == "simple()", ref.symbol.name()
#assert ref.size < 25
#eprint('definitions:'
#       + ''.join(['\n * ' + str(d) for d in ref.symbol.definitions])
#)
#assert len(ref.symbol.definitions) == 1
#eprint('declarations:'
#       + ''.join(['\n * ' + str(d) for d in ref.symbol.declarations])
#)
#assert len(ref.symbol.declarations) == 1
#eprint('uses:'
#       + ''.join(['\n * ' + str(d) for d in ref.symbol.uses])
#)
#assert len(ref.symbol.uses) == 1

eprint("Bye")
