from template import *
import time

eprint("Hello")
project = Project('test', test_dir + '/basic.mkf')
source = project.get_file(test_dir + "/function_template.cpp")
project.wait_for_analysis()
project.print_diagnostics()
assert project.get_diagnostic_count() == 0
eprint("Bye")
