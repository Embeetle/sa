from template import *
import time
import os
import shutil

eprint("Hello pic32")
test_src = test_dir + "/pic32"
test_bld = "test/pic32"
try:
    shutil.rmtree(test_bld)
except:
    pass
shutil.copytree(test_src, test_bld)
project = Project(test_bld, 'makefile', test_bld, test_bld + '/cache')

tools_dir = os.path.dirname(toolchain_dir)

project.set_make_config([
    'make', '-f', 'makefile',
    f'TOOLPREFIX={tools_dir}/mips_mti_toolchain_11.2.0_64b/bin/mips-mti-elf-'
])

project.add_file("main.c",       ce.FileMode.AUTOMATIC)
project.add_file("start.S",      ce.FileMode.INCLUDE)
project.set_hdir_mode(".",       ce.HdirMode.AUTOMATIC)
project.set_hdir_mode("include", ce.HdirMode.AUTOMATIC)
project.wait_for_analysis()
project.print_diagnostics()
#assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

eprint("Exiting")
ce.abort()
eprint("Bye")
