import source_analyzer as ce
from template import *
import time
import shutil

eprint("Hello")
build_dir = "test/symbol_kinds_build"
cache_dir = "test/symbol_kinds_cache"
shutil.rmtree(build_dir, ignore_errors=True)
shutil.rmtree(cache_dir, ignore_errors=True)
project = Project(
    build_dir = build_dir,
    makefile = test_dir + '/generic.mkf',
    cache_dir = cache_dir,
)
symbol_kinds_c = test_dir + "/symbol_kinds.c"
file = project.get_file(symbol_kinds_c, "the symbols C file")
project.wait_for_analysis()
    
# Linking does not work currently for this test project; why?
if False and project.linker_status == ce.LinkerStatus.ERROR:
    eprint("### analysis error")
    exit(1)
else:
    eprint("### analysis done")

# Test actual symbol agaist expected symbol at location in emacs coordinates:
# one-based line and zero-based column.
def test(line, column, name, ref_kind, sym_kind):
    eprint(f"expect {ce.symbol_kind_name(sym_kind)}"
           f" {ce.occurrence_kind_name(ref_kind)} {name} at {line}:{column}"
    )
    ref = project.click(file, line, column, False)
    if not ref:
        eprint("found nothing")
    elif ref.kind == ce.occurrence_kind_include:
        eprint(
            f"found file {ref.path}"
            f" at {ref.begin_offset}..{ref.end_offset}"
        )
    else:
        eprint(
            f"found  {ref.symbol.kind_name}"
            f" {ce.occurrence_kind_name(ref.kind)} {ref.symbol.name}"
            f" at {ref.begin_offset}..{ref.end_offset}"
            #f" in {ref.file}"
        )
    assert ref
    assert ref.symbol.name == name
    assert ref.kind == ref_kind
    assert ref.symbol.kind == sym_kind

test( 1, 11, 'a', ce.occurrence_kind_declaration, entity_kind_global_variable)
test( 2,  4, 'b', ce.occurrence_kind_definition,  entity_kind_global_variable)
test( 3,  4, 'c', ce.occurrence_kind_tentative_definition,
                                                  entity_kind_global_variable)
test( 4, 11, 'd', ce.occurrence_kind_definition,  entity_kind_static_variable)
test( 5, 11, 'e', ce.occurrence_kind_tentative_definition, entity_kind_static_variable)
test( 6, 11, 'e', ce.occurrence_kind_definition,  entity_kind_static_variable)
test( 8, 12, 'ff',
     ce.occurrence_kind_declaration,              entity_kind_global_function)
test( 9,  4, 'main',
     ce.occurrence_kind_definition,               entity_kind_global_function)
test( 9, 13, 'argc', ce.occurrence_kind_definition, entity_kind_parameter)
test( 9, 25, 'argv', ce.occurrence_kind_definition, entity_kind_parameter)
test(11,  8, 'argv', ce.occurrence_kind_use,      entity_kind_parameter)
test(12,  6, 'f', ce.occurrence_kind_definition,  entity_kind_automatic_variable)
test(12, 10, 'argc', ce.occurrence_kind_use,      entity_kind_parameter)
test(13, 13, 'g',  ce.occurrence_kind_definition, entity_kind_local_static_variable)
test(14, 13, 'g2', ce.occurrence_kind_definition, entity_kind_local_static_variable)
test(15, 13, 'h', ce.occurrence_kind_declaration, entity_kind_global_variable)
test(16,  8, 'h', ce.occurrence_kind_use,  entity_kind_global_variable)
test(17, 15, 'i',
     ce.occurrence_kind_definition,  entity_kind_automatic_variable)
test(18,  2, 'ff', ce.occurrence_kind_use,  entity_kind_global_function)
test(18,  5, 'i', ce.occurrence_kind_use,  entity_kind_automatic_variable)
test(19,  9, 'a', ce.occurrence_kind_use,  entity_kind_global_variable)
test(19, 13, 'b', ce.occurrence_kind_use,  entity_kind_global_variable)
test(19, 17, 'c', ce.occurrence_kind_use,  entity_kind_global_variable)
test(19, 21, 'd', ce.occurrence_kind_use,  entity_kind_static_variable)
test(19, 25, 'e', ce.occurrence_kind_use,  entity_kind_static_variable)
test(19, 29, 'f', ce.occurrence_kind_use,  entity_kind_automatic_variable)
test(19, 33, 'g', ce.occurrence_kind_use,  entity_kind_local_static_variable)
test(19, 37, 'g2', ce.occurrence_kind_use,  entity_kind_local_static_variable)
test(19, 42, 'h', ce.occurrence_kind_use,  entity_kind_global_variable)
test(19, 46, 'i', ce.occurrence_kind_use,  entity_kind_automatic_variable)
test(22, 12, 'f',
     ce.occurrence_kind_declaration, entity_kind_local_function)
test(23, 12, 'f', ce.occurrence_kind_definition, entity_kind_local_function)
test(25, 12, 'T', ce.occurrence_kind_definition,  entity_kind_type)
test(26,  2, 'z', ce.occurrence_kind_tentative_definition,  entity_kind_global_variable)
test(26,  0, 'T', ce.occurrence_kind_use,  entity_kind_type)
test(28,  4, 'a', ce.occurrence_kind_definition,  entity_kind_global_variable)
test(29,  5, 'ff',
     ce.occurrence_kind_definition,  entity_kind_global_function)
test(29, 12, 'x', ce.occurrence_kind_definition,  entity_kind_parameter)
test(29, 23, 'x', ce.occurrence_kind_use,  entity_kind_parameter)
test(30,  4, 'h', ce.occurrence_kind_definition,  entity_kind_global_variable)

eprint("### Track global function definitions and weak definitions")
project.track_occurrences(
    symbol_kinds_c,
    [ce.occurrence_kind_definition, ce.occurrence_kind_weak_definition],
    [entity_kind_global_function]
)
eprint("### Also track uses and stack variables, but not weak definitions")
project.track_occurrences(
    symbol_kinds_c,
    [ce.occurrence_kind_definition, ce.occurrence_kind_use],
    [entity_kind_global_function, entity_kind_automatic_variable]
)
eprint("### Untrack uses")
project.track_occurrences(
    symbol_kinds_c,
    [ce.occurrence_kind_definition],
    [entity_kind_global_function, entity_kind_automatic_variable]
)
eprint("### Track nothing")
project.track_occurrences(symbol_kinds_c, [], [])

eprint("### Bye")
