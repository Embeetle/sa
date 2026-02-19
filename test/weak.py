from template import *
import time
import os
import shutil

eprint("Hello")
project_dir = 'test/weak'
build_dir = project_dir + '/build'
cache_dir = project_dir + '/cache'
makefile = '../config/makefile'
os.makedirs(project_dir, exist_ok=True)
shutil.rmtree(project_dir)
shutil.copytree(test_dir + "/weak", project_dir)

project = Project(build_dir, makefile, project_dir, cache_dir)

project.add_file("main.c", ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

ref = project.click("main.c", 2, 6)
assert ref
assert ref.kind == ce.occurrence_kind_weak_declaration
assert ref.symbol.kind == entity_kind_global_function
assert ref.symbol.name == "foo"

ref = project.click("main.c", 6, 7)
assert ref
assert ref.kind == ce.occurrence_kind_weak_use
assert ref.symbol.kind == entity_kind_global_function
assert ref.symbol.name == "foo"

ref = project.click("main.c", 6, 12)
assert ref
assert ref.kind == ce.occurrence_kind_weak_use
assert ref.symbol.kind == entity_kind_global_function
assert ref.symbol.name == "foo"

eprint("Exiting")
ce.abort()
eprint("Bye")
