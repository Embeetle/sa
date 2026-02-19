from template import *
import template
import source_analyzer
import time
import os
import shutil

fprint = source_analyzer._eprint

fprint("Hello")
#template.silent = 1

build_dir = "test/simple"
source_dir = "test/simple"
os.makedirs(source_dir, exist_ok=True)
try:
    shutil.rmtree(source_dir)
    shutil.rmtree("test/cache/simple.c")
except:
    pass
shutil.copytree(test_dir + "/simple/source", source_dir)

makefile = test_dir + '/simple/makefile'
project = Project(build_dir, makefile, source_dir)

# ------------------------------------------------------------------------------
fprint("\n### Test 0: analyze excluded file")
# This simulates the situation where a source file is open when you open the
# project, before analysis is complete.
project.click("simple.c", 4, 5)

# ------------------------------------------------------------------------------
fprint("\n### Test 1: analyze as-is, with both C files in automatic mode")
project.set_hdir_mode('inc', ce.hdir_mode_automatic)
project.add_file("simple_main.c", ce.FileMode.AUTOMATIC)
project.add_file("simple.c",      ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
#eprint("used files: {}".format(' '.join(project.used_files)))
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0
assert project.file_linked("simple_main.c")
assert project.file_linked("simple.c")
assert project.file_included("simple.h")
assert project.file_included("inc/simple2.h")
assert project.file_included(makefile)
assert project.hdir_used('inc')
ref = project.click("simple.c", 1, 10)
assert ref
assert ref.kind == ce.occurrence_kind_include
assert project.standard_source_path(ref.path) == "simple.h"
ref = project.click("simple.c", 4, 8)
assert ref
assert ref.kind == ce.occurrence_kind_definition
assert ref.symbol.kind == entity_kind_global_function
assert ref.symbol.name == "simple", ref.symbol.name
assert ref.end_offset - ref.begin_offset < 25
#eprint('definitions:'
#       + ''.join(['\n * ' + str(d) for d in ref.symbol.definitions])
#)
assert len(ref.symbol.definitions) == 1
#eprint('declarations:'
#       + ''.join(['\n * ' + str(d) for d in ref.symbol.declarations])
#)
assert len(ref.symbol.declarations) == 1
#eprint('uses:'
#       + ''.join(['\n * ' + str(d) for d in ref.symbol.uses])
#)
assert len(ref.symbol.uses) == 1
ref = project.click("simple.c", 6, 13)
assert ref
assert ref.kind == ce.occurrence_kind_use
assert ref.symbol.kind == entity_kind_static_variable
assert ref.symbol.name == "simple2", ref.symbol.name()
ref = project.click("simple.c", 8, 0)
assert not ref
ref = project.click("simple_main.c", 24, 9)
assert ref
assert ref.kind == ce.occurrence_kind_use
# This location contains both a global function call and a macro
# expansion. Which one is found first is unpredictable.
if ref.symbol.kind == entity_kind_macro:
    assert ref.symbol.kind == entity_kind_macro
    assert ref.symbol.name == "SIMPLE", ref.symbol.name
    assert ref.end_offset - ref.begin_offset == 6
else:
    assert ref.symbol.kind == entity_kind_global_function
    assert ref.symbol.name == "simple", ref.symbol.name
    assert ref.end_offset - ref.begin_offset == 6
ref = project.click("simple_main.c", 11, 13)
assert ref
assert ref.kind == ce.occurrence_kind_declaration
assert ref.symbol.kind == entity_kind_local_function
assert ref.symbol.name == "baz", ref.symbol.name()
#eprint('definitions:'
#       + ''.join(['\n * ' + str(d) for d in ref.symbol.definitions])
#)
assert len(ref.symbol.definitions) == 1
#eprint('declarations:'
#       + ''.join(['\n * ' + str(d) for d in ref.symbol.declarations])
#)
assert len(ref.symbol.declarations) == 1
#eprint('uses:'
#       + ''.join(['\n * ' + str(d) for d in ref.symbol.uses])
#)
assert len(ref.symbol.uses) == 1

fprint("\n### Test 2: touch simple.c")
project.reload_file("simple.c")
project.wait_for_analysis()
project.click("simple.c",4,5)

fprint("Exiting")
ce.abort()
fprint("Bye")
