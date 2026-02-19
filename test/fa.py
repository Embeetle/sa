from template import *
import time
import os
import shutil

eprint("Hello")
build_dir = "test/fa"
source_dir = "test/fa"
os.makedirs(source_dir, exist_ok=True)
try:
    shutil.rmtree(source_dir)
except:
    pass
shutil.copytree(test_dir + "/fa/source", source_dir)
project = Project(build_dir, test_dir + '/fa/makefile', source_dir)

eprint("\n### Test 1: try some C++")
eprint("Add files")
project.add_file("foo.cc", ce.FileMode.INCLUDE)
project.add_file("example.cpp", ce.FileMode.INCLUDE)
project.wait_for_analysis()
project.print_diagnostics()
assert project.get_diagnostic_count() == 0

eprint("Exiting")
ce.abort()
eprint("Bye")
