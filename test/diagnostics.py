import source_analyzer as ce
from template import *
import time
import os
import shutil

eprint("Hello")
build_dir = "test/diagnostics"
source_dir = "test/diagnostics"
os.makedirs(source_dir, exist_ok=True)
try:
    shutil.rmtree(source_dir)
    shutil.rmtree(build_dir)
except:
    pass
shutil.copytree(test_dir + "/diagnostics/source", source_dir)
project = Project(build_dir, test_dir + '/diagnostics/makefile', source_dir)


eprint("\n### Test 0: check empty")
project.wait_for_analysis()
eprint(
    f"empty project status: "
    f"{ce.project_status_name(project.project_status)}"
)
eprint(
    f"empty project linker status: "
    f"{ce.linker_status_name(project.linker_status)}"
)

eprint("\n### Test 1: analyze and check")
project.add_file("main.c", ce.FileMode.AUTOMATIC)
eprint(
    f"initial project status: "
    f"{ce.project_status_name(project.project_status)}"
)
eprint(
    f"initial project linker status: "
    f"{ce.linker_status_name(project.linker_status)}"
)

project.wait_for_analysis()
eprint(f'{project.get_diagnostic_count()} diagnostics:')
eprint('\n'.join(project.get_diagnostics()))
eprint(
    'main.c inclusion status is '
    f'{ce.inclusion_status_name(project.get_file_inclusion_status("main.c"))}'
)
assert project.linker_status == ce.LinkerStatus.DONE
assert(
    project.get_file_inclusion_status("main.c") == ce.InclusionStatus.INCLUDED
)
assert project.get_diagnostic_count() == 0
eprint("Exiting")
ce.abort()
eprint("Bye")
