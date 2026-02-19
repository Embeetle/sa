from template import *
import time

eprint("Test enter")
build_dir = "test/.error"
project = Project(build_dir, test_dir + '/foo.mkf')
file = project.get_file(test_dir + "/stddef.c")
project.wait_for_analysis()

if project.get_file_analysis_status(file) == ce.AnalysisStatus.FAILED:
    eprint("analysis error")
else:
    eprint("analysis done")

eprint("Bye")
