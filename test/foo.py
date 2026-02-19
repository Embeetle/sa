from template import *
import time

eprint("Hello")
build_dir = "test/.foo"
project = Project(build_dir, test_dir + '/foo.mkf')
foo_c = project.get_file(test_dir + "/foo.c")
while project.get_file_analysis_status(foo_c) == ce.AnalysisStatus.BUSY:
    time.sleep(2)
bar_c = project.get_file(test_dir + "/bar.c")
project.wait_for_analysis()
    
if project.linker_status == ce.LinkerStatus.ERROR:
    eprint("analysis error as expected")
else:
    eprint(
        "analysis done, but should have detected undefined globals"
        " __bss_end__ __bss_start__ _exit"
    )
    exit(1)
    
eprint("Bye")
