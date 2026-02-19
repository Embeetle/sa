from template import *
import time

eprint("Hello")
project = Project('test/multi', test_dir + '/basic.mkf')
multi_c = project.get_file(test_dir + "/multi.c")
multi2_c = project.get_file(test_dir + "/multi2.c")
project.wait_for_analysis()
assert project.get_file_inclusion_status(multi_c) == ce.InclusionStatus.INCLUDED
assert project.get_file_inclusion_status(multi2_c)== ce.InclusionStatus.INCLUDED

eprint("\ntest 1")
project.set_file_mode(multi2_c, ce.FileMode.EXCLUDE)
project.wait_for_analysis()
assert project.get_file_inclusion_status(multi_c) == ce.InclusionStatus.INCLUDED
assert project.get_file_inclusion_status(multi2_c)== ce.InclusionStatus.EXCLUDED

eprint("\ntest 2")
project.set_file_mode(multi2_c, ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
assert project.get_file_inclusion_status(multi_c) == ce.InclusionStatus.INCLUDED
assert project.get_file_inclusion_status(multi2_c)== ce.InclusionStatus.INCLUDED

eprint("\ntest 3")
project.set_file_mode(multi_c, ce.FileMode.EXCLUDE)
project.wait_for_analysis()
assert project.get_file_inclusion_status(multi_c) == ce.InclusionStatus.EXCLUDED
assert project.get_file_inclusion_status(multi2_c)== ce.InclusionStatus.EXCLUDED

eprint("Bye")

