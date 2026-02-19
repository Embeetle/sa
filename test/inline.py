from template import *
import time
import os
import shutil

eprint("Hello inline")
test_src = test_dir + "/inline"
test_bld = "test/inline"
try:
    shutil.rmtree(test_bld)
except:
    pass
shutil.copytree(test_src, test_bld)
project = Project(test_bld, 'makefile', test_bld, test_bld + '/cache')

eprint("\n### Test 1: analyze as-is")
project.add_file("inline.c",      ce.FileMode.AUTOMATIC)
project.add_file("inline2.c",      ce.FileMode.AUTOMATIC)
project.add_file("tip.c",      ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
project.print_diagnostics()
assert project.get_diagnostic_count() == 4
assert project.linker_status != ce.LinkerStatus.DONE
assert not project.hdir_used('inc')
assert project.file_linked("inline.c")
# tip.c used due to linker relevant error: failed include
assert project.file_linked("tip.c")
assert not project.file_linked("inline2.c")
assert not project.file_included("inc/inline.h")

eprint("\n### Test 2: add hdir")
project.set_hdir_mode("inc", ce.hdir_mode_automatic)
project.wait_for_analysis()
project.print_diagnostics()
assert project.get_diagnostic_count() == 0
assert project.linker_status == ce.LinkerStatus.DONE
assert project.hdir_used('inc')
assert project.file_linked("inline.c")
assert not project.file_linked("inline2.c")
assert project.file_included("inc/inline.h")
assert project.file_linked("tip.c")

eprint("Exiting")
ce.abort()
eprint("Bye")
