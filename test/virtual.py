from template import *
import time
import os
import shutil

eprint("Hello")
build_dir = "test/virtual"
source_dir = "test/virtual"
os.makedirs(source_dir, exist_ok=True)
try:
    shutil.rmtree(source_dir)
except:
    pass
shutil.copytree(test_dir + "/virtual/source", source_dir)
project = Project(build_dir, test_dir + '/virtual/makefile', source_dir)

eprint("\n### Test 1: try some C++")
eprint("Add files")
project.add_file("A.cpp", ce.FileMode.AUTOMATIC)
project.add_file("main.cpp", ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
eprint(f'{project.get_diagnostic_count()} diagnostics:')
eprint('\n'.join(project.get_diagnostics()))
assert project.get_diagnostic_count() == 0
#assert project.file_linked("A.cpp")

eprint("Exiting")
ce.abort()
eprint("Bye")
