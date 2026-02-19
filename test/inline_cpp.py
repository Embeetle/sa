from template import *
import time
import os
import shutil

eprint("Hello inline_cpp")
test_src = test_dir + "/inline_cpp"
test_bld = "test/inline_cpp"
try:
    shutil.rmtree(test_bld)
except:
    pass
shutil.copytree(test_src, test_bld)
project = Project(test_bld, 'makefile', test_bld, test_bld + '/cache')

eprint("\n### Test 1: analyze as-is")
project.add_file("inline_cpp.cpp",      ce.FileMode.AUTOMATIC)
project.add_file("tip.cpp",             ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
project.print_diagnostics()
assert project.get_diagnostic_count() == 0
assert project.linker_status == ce.LinkerStatus.DONE
assert project.file_linked("tip.cpp")
assert project.file_linked("inline_cpp.cpp")

eprint("Exiting")
ce.abort()
eprint("Bye")
