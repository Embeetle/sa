from template import *
import time
import sys

eprint("Test enter")
eprint("sys.path =", sys.path)

project_path = test_dir
source_folder = project_path
build_folder = "test/emptyloop/build"
makefile = project_path + "/makefile"
project = Project(build_folder, makefile, source_folder)
file = source_folder + "/emptyloop.c"
print("@add file")
project.add_file(file, ce.FileMode.INCLUDE)
print("@add file done")
project.wait_for_analysis()
print("@analysis done")

if project.fail_count:
    eprint("analysis failed")
    exit(1)
eprint("analysis done")

range = project.find_empty_loop(file, 50)
eprint(f'Range: {range}')
range = project.find_empty_loop(file, 10)
eprint(f'Range: {range}')

ce.abort()
eprint("Bye")
