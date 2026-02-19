from template import *
import time
import os
import shutil

eprint("Hello")
project_dir = 'test/v6'
build_dir = project_dir + '/build'
cache_dir = project_dir + '/cache'
makefile = '../config/makefile'
os.makedirs(project_dir, exist_ok=True)
shutil.rmtree(project_dir)
shutil.copytree(test_dir + "/v6", project_dir)

project = Project(build_dir, makefile, project_dir, cache_dir)

libfoo = os.path.abspath(os.path.join(project_dir, "libfoo"))

project.set_hdir_mode(libfoo, ce.hdir_mode_automatic)
project.add_file("main.c", ce.FileMode.AUTOMATIC)
project.add_file("foo.c",  ce.FileMode.AUTOMATIC)
project.add_file(libfoo+'/bar.c',  ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

eprint("Exiting")
ce.abort()
eprint("Bye")
