from template import *
import time

eprint("Hello")
build_dir = "test/.location"
project = Project(build_dir, test_dir + '/foo.mkf')
location_c = project.get_file(test_dir + "/location.c")
project.wait_for_analysis()
eprint(f'Project status: {ce.project_status_name(project.project_status)}')
eprint(f'Linker status: {ce.linker_status_name(project.linker_status)}')
assert project.project_status == ce.ProjectStatus.READY
# Expect undefined globals _exit __bss_end__ __bss_start__
#assert project.linker_status == ce.LinkerStatus.DONE
eprint("Bye")
