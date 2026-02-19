from template import *
import time
import os
import shutil

eprint("Hello")
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
eprint("\n### Test 1: analyze as-is, with both C files in automatic mode")
project.set_hdir_mode('inc', ce.hdir_mode_automatic)
project.add_file("simple_main.c", ce.FileMode.AUTOMATIC)
project.add_file("simple.c",      ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0
assert project.file_linked("simple_main.c")
assert project.file_linked("simple.c")
assert project.file_included("simple.h")
assert project.file_included("inc/simple2.h")
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
eprint('definitions:'
       + ''.join(['\n * ' + str(d) for d in ref.symbol.definitions])
)
assert len(ref.symbol.definitions) == 1
eprint('declarations:'
       + ''.join(['\n * ' + str(d) for d in ref.symbol.declarations])
)
assert len(ref.symbol.declarations) == 1
eprint('uses:'
       + ''.join(['\n * ' + str(d) for d in ref.symbol.uses])
)
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
eprint('definitions:'
       + ''.join(['\n * ' + str(d) for d in ref.symbol.definitions])
)
assert len(ref.symbol.definitions) == 1
eprint('declarations:'
       + ''.join(['\n * ' + str(d) for d in ref.symbol.declarations])
)
assert len(ref.symbol.declarations) == 1
eprint('uses:'
       + ''.join(['\n * ' + str(d) for d in ref.symbol.uses])
)
assert len(ref.symbol.uses) == 1

# ------------------------------------------------------------------------------
eprint("\n### Test 1b: test find_symbols")
mains = project.find_symbols("main")
eprint(f"mains: {[str(symbol) for symbol in mains]}")
assert len(mains) == 1

# ------------------------------------------------------------------------------
eprint("\n### Test 1c: check macro occurrences")
project.track_occurrences(
    "simple.h", ce.all_occurrence_kinds, [entity_kind_macro]
)
simple_defs = [ occ for occ in project.tracked
                if occ.data.entity.name == "SIMPLE"
                and occ.data.kind == ce.occurrence_kind_definition ]
for d in simple_defs:
    eprint(f'Def: {d}')
assert len(simple_defs) == 1
project.track_occurrences("simple.h", ce.all_occurrence_kinds, [])

# ------------------------------------------------------------------------------
eprint("\n### Test 1d: get completions")
line = "  return sim"
line_pos = 57
insert_pos, completions = project.get_completions(
    "simple.c", line_pos + len(line), line
)
eprint(f'Insert completion at {insert_pos}')
for c in completions:
    eprint(f'  `-> {c}')
assert insert_pos == line_pos + len(line) - 3
assert set(completions) == set(['simple', 'simple2'])

# ------------------------------------------------------------------------------
eprint("\n### Test 1e: edit without saving")

# Ensure that adding a space after a symbol does not enlarge the range in which
# the symbol is found.
assert not project.click("simple.c", 4, 3)
assert project.click("simple.c", 4, 4)
assert project.click("simple.c", 4, 10)
assert not project.click("simple.c", 4, 11)
project.edit_file("simple.c", 52, 52, " ")
ref = project.click("simple.c", 4, 11)
assert not ref

# Check that an edit before a symbol is taken into account.
project.edit_file("simple.c", 2, 2, "\n")
ref = project.click("simple.c", 4, 8)
assert ref
assert ref.kind == ce.occurrence_kind_definition
assert ref.symbol.kind == entity_kind_global_function
assert ref.symbol.name == "simple", ref.symbol.name

eprint("\n### Test 2: reload makefile")
project.reload_file(makefile)
project.wait_for_analysis()
assert project.linker_status == ce.LinkerStatus.DONE

eprint("\n### Test 3: append some code to simple.c")
eprint("src:", stdpath("simple.c", source_dir))
with open(stdpath("simple.c", source_dir), "a", newline='\n') as out:
    out.write("\nvoid foo() { simple2 = 7; }\n")
project.reload_file("simple.c")
# Try clicking during analysis.  Result is unpredictable, so ignore it.
time.sleep(0.020)
ref = project.click("simple.c", 12, 17)
time.sleep(0.020)
ref = project.click("simple.c", 12, 17)
time.sleep(0.020)
ref = project.click("simple.c", 12, 17)
project.wait_for_analysis()
# Analysis is complete, so result should be correct.
ref = project.click("simple.c", 12, 17)
assert ref
assert ref.kind == ce.occurrence_kind_use
assert ref.symbol.kind == entity_kind_static_variable
assert ref.symbol.name == "simple2", ref.symbol.name()

eprint("\n### Test 4: force-exclude simple.c")
project.set_file_mode("simple.c", ce.FileMode.EXCLUDE)
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.linker_status == ce.LinkerStatus.ERROR
project.print_diagnostics()
if project.ignore_undefined_globals:
    assert project.get_diagnostic_count() == 0
else:
    # Expect two undefined global symbols: simple() and flop()
    assert project.get_diagnostic_count() == 2
assert not project.hdir_used('inc')
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.file_linked("simple_main.c")
assert not project.file_linked("simple.c")
assert project.file_included("simple.h")
assert not project.file_included("inc/simple2.h")

eprint("\n### Test 5: force-include simple.c")
project.set_file_mode("simple.c", ce.FileMode.INCLUDE)
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0
assert project.file_linked("simple_main.c")
assert project.file_linked("simple.c")
assert project.file_included("simple.h")
assert project.file_included("inc/simple2.h")
assert project.hdir_used('inc')

eprint("\n### Test 6: force-exclude simple.c and simple_main.c")
project.set_file_mode("simple.c", ce.FileMode.EXCLUDE)
project.wait_for_analysis()
project.set_file_mode("simple_main.c", ce.FileMode.EXCLUDE)
project.wait_for_analysis()
project.set_file_mode("simple_main.c", ce.FileMode.AUTOMATIC)

eprint("\n### Test 7: automatic simple.c")
project.set_file_mode("simple.c", ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0
assert project.file_linked("simple_main.c")
assert project.file_linked("simple.c")
assert project.file_included("simple.h")
assert project.file_included("inc/simple2.h")
assert project.hdir_used('inc')

eprint("\n### Test 8: remove simple.h (rename it)")
os.rename(source_dir + "/simple.h", source_dir + "/simple.h.bak")
project.remove_file("simple.h")
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.linker_status == ce.LinkerStatus.ERROR
print("diagnostics: \n{}".format('\n'.join(project.get_diagnostics())))
assert project.get_diagnostic_count() == 3, project.get_diagnostic_count()
assert project.file_linked("simple_main.c")
assert project.file_linked("simple.c")
assert not project.file_included("simple.h")
assert project.file_included("inc/simple2.h")

eprint("\n### Test 9: restore simple.h (rename it)")
os.rename(source_dir + "/simple.h.bak", source_dir + "/simple.h")
project.add_file("simple.h")
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

eprint("\n### Test 10: force-exclude simple_main.c")
project.set_file_mode("simple_main.c", ce.FileMode.EXCLUDE)
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.linker_status == ce.LinkerStatus.ERROR
assert project.get_diagnostic_count() == 1  # Undefined global main()
assert not project.file_linked("simple_main.c")
assert not project.file_linked("simple.c")
assert not project.file_included("simple.h")
assert not project.file_included("inc/simple2.h")

eprint("\n### Test 11: force-include simple_main.c")
project.set_file_mode("simple_main.c", ce.FileMode.INCLUDE)
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0
assert project.file_linked("simple_main.c")
assert project.file_linked("simple.c")
assert project.file_included("simple.h")
assert project.file_included("inc/simple2.h")
assert project.hdir_used('inc')

eprint("\n### Test 12: add excluded file with conflicting global simple2.c")
project.add_file("simple2.c",      ce.FileMode.EXCLUDE)
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
# Force re-linking and check that the excluded file does not cause conflicts
project.reload_file("simple_main.c")
project.wait_for_analysis()
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

eprint("\n### Test 12b: set simple2.c to automatic and back to exclude")
project.set_file_mode("simple2.c", ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
assert project.linker_status == ce.LinkerStatus.ERROR
assert project.get_diagnostic_count() == 2
project.set_file_mode("simple2.c", ce.FileMode.EXCLUDE)
project.wait_for_analysis()
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

eprint("\n### Test 13: add non-existing file foofoo.c")
project.add_file("foofoo.c",      ce.FileMode.INCLUDE)
project.wait_for_analysis()
assert project.linker_status == ce.LinkerStatus.DONE
if project.get_diagnostic_count() != 1:
    eprint(f'Diagnostics:\n{project.get_diagnostics()}')
assert project.get_diagnostic_count() == 1

eprint("\n### Test 14: remove foofoo.c")
project.remove_file("foofoo.c")
project.wait_for_analysis()
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

eprint("\n### Test 15: remove simple.c")
project.remove_file("simple.c")
project.wait_for_analysis()
assert project.linker_status == ce.LinkerStatus.ERROR
project.print_diagnostics()
if project.ignore_undefined_globals:
    assert project.get_diagnostic_count() == 0
else:    
    # Expect two undefined global symbols: simple() and flop()
    assert project.get_diagnostic_count() == 2
assert not project.hdir_used('inc')
print("linked files: {}".format(' '.join(project.linked_files)))
print("included files: {}".format(' '.join(project.included_files)))
assert project.file_linked("simple_main.c")
assert not project.file_linked("simple.c")
assert project.file_included("simple.h")
assert not project.file_included("inc/simple2.h")

eprint("\n### Test 16: repair project simple.c")
project.add_file("simple.c",      ce.FileMode.AUTOMATIC)
project.remove_file("simple2.c")
project.wait_for_analysis()
diagnostics = "\n  ".join(project.get_diagnostics())
eprint(f'Diagnostics:\n  {diagnostics}')
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

eprint("\n### Test 17: test caching")
project = Project(build_dir, makefile, source_dir)
project.set_hdir_mode('inc', ce.hdir_mode_automatic)
project.add_file("simple_main.c", ce.FileMode.AUTOMATIC)
project.add_file("simple.c",      ce.FileMode.AUTOMATIC)
project.wait_for_analysis()
diagnostics = "\n  ".join(project.get_diagnostics())
eprint(f'Diagnostics:\n  {diagnostics}')
assert project.file_analysis_data_was_read_from_cache("simple_main.c")
assert project.file_analysis_data_was_read_from_cache("simple.c")
assert project.linker_status == ce.LinkerStatus.DONE
assert project.get_diagnostic_count() == 0

# TODO: try to test for a changed symbol for a released file

eprint("Exiting")
ce.abort()
eprint("Bye")
