from template import *
import time
import os
import shutil

eprint("Hello")
test_data = test_dir + "/hello"
test_dir = "test/empty_path"
try:
    shutil.rmtree(test_dir)
except:
    pass
shutil.copytree(test_data, test_dir)
project = Project(test_dir, 'makefile', test_dir, test_dir+'/cache')

eprint("\n### Test 1: get occurrences for empty path")
project.track_occurrences(
    "",
    [ce.occurrence_kind_definition],
    [ce.entity_kind('global function')]
)
project.find_occurrence("", 3, 1)
eprint("Exiting")
ce.abort()
eprint("Bye")

