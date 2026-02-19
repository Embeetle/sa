from template import *
import time

eprint("Hello")
project = Project('test', test_dir + '/basic.mkf')
decltype_cpp = project.get_file(test_dir + "/decltype.cpp")
project.wait_for_analysis()
project.print_diagnostics()
assert project.get_diagnostic_count() == 1
eprint("Bye")

