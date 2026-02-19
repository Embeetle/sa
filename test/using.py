from template import *
import time
import os
import shutil

eprint("Hello")
build_dir = "test/using"
source_dir = "test/using"
os.makedirs(source_dir, exist_ok=True)
try:
    shutil.rmtree(source_dir)
except:
    pass
shutil.copytree(test_dir + "/using/source", source_dir)
project = Project(build_dir, test_dir + '/cpp/makefile', source_dir)

eprint("\n### Test 1: try some C++")
eprint("Add files")
project.add_file("using.cpp", ce.FileMode.INCLUDE)
project.wait_for_analysis()
eprint(f'{project.get_diagnostic_count()} diagnostics:')
eprint('\n'.join(project.get_diagnostics()))
assert project.get_diagnostic_count() == 0
ref = project.click("using.cpp",11,13)
assert ref
assert ref.kind is ce.occurrence_kind_declaration

eprint("Exiting")
ce.abort()
eprint("Bye")
