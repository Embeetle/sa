from template import *
import time
import os
import shutil

eprint("Hello")
test_data = test_dir + "/hello"
test_dir = "test/hello"
try:
    shutil.rmtree(test_dir)
except:
    pass
shutil.copytree(test_data, test_dir)
project = Project(test_dir, 'makefile', test_dir, test_dir+'/cache')

eprint("\n### Test 1: analyze as-is")
project.add_file("hello.c", ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

eprint("Exiting")
ce.abort()
eprint("Bye")

