from template import *
import time
import os
import shutil

eprint("Hello inline_c99")
test_src = test_dir + "/inline_c99"
test_bld = "test/inline_c99"
try:
    shutil.rmtree(test_bld)
except:
    pass
shutil.copytree(test_src, test_bld)
project = Project(test_bld, 'makefile', test_bld, test_bld + '/cache')

eprint("\n### Test 1: analyze as-is")
project.add_file("inline_c99.c",      ce.FileMode.AUTOMATIC)
project.add_file("tap.c",             ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
project.print_diagnostics()
#assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 1
diagnostics = project.get_diagnostics()
assert diagnostics[0] == "inline_c99.c:349: error: undefined global symbol tip"
eprint("Got one diagnostic as expected")

#assert project.file_linked("inline_cpp.cpp")

eprint("Exiting")
ce.abort()
eprint("Bye")
