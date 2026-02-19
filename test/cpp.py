from template import *
import time
import os
import shutil

eprint("Hello")
build_dir = "test/cpp"
source_dir = "test/cpp"
os.makedirs(source_dir, exist_ok=True)
try:
    shutil.rmtree(source_dir)
except:
    pass
shutil.copytree(test_dir + "/cpp/source", source_dir)
project = Project(build_dir, test_dir + '/cpp/makefile', source_dir)

eprint("\n### Test 1: try some C++")
eprint("Add files")
project.add_file("foo.cpp", ce.FileMode.INCLUDE)
project.add_file("num.cpp", ce.FileMode.INCLUDE)
project.add_file("A.cpp", ce.FileMode.AUTOMATIC)
project.add_file("A2.cpp", ce.FileMode.AUTOMATIC)
project.add_file("template.cpp", ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
project.print_diagnostics()
assert project.get_diagnostic_count() == 0
assert project.file_linked("A.cpp")
assert not project.file_linked("A2.cpp")

# TODO: try to test for a changed symbol for a released file

eprint("Exiting")
ce.abort()
eprint("Bye")
