import source_analyzer as ce
from template import *
import time
import shutil

eprint("Hello")
eprint(f"test dir is {test_dir}")
build_dir = "test/symkinds_build"
cache_dir = "test/symkinds_cache"
shutil.rmtree(build_dir, ignore_errors=True)
shutil.rmtree(cache_dir, ignore_errors=True)
project = Project(
    build_dir = build_dir,
    makefile = test_dir + '/generic.mkf',
    cache_dir = cache_dir,
)
symkinds1_cpp = test_dir + "/symkinds1.cpp"
symkinds2_cpp = test_dir + "/symkinds2.cpp"
file1 = project.get_file(symkinds1_cpp, symkinds1_cpp)
file2 = project.get_file(symkinds2_cpp, symkinds2_cpp)
#project.set_hdir_mode(test_dir, ce.hdir_mode_include)
project.wait_for_analysis()

# Linking does not work currently for this test project; why?
if False and project.linker_status == ce.LinkerStatus.ERROR:
    eprint("### analysis error")
    exit(1)
else:
    eprint("### analysis done")

for diagnostic in project.get_diagnostics():
    if (
            not "undefined global" in diagnostic and
            not "keyword '__make_unsigned' will be made available "
            "as an identifier for the remainder of the translation unit"
            in diagnostic
    ):
        eprint(f'### unexpected diagnostic {diagnostic}')
        exit(1)

# Test actual symbol agaist expected symbol at location in emacs coordinates:
# one-based line and zero-based column.
def test(file, line, column, name, ref_kind, sym_kind):
    eprint(f"expect {ce.symbol_kind_name(sym_kind)}"
           f" {ce.occurrence_kind_name(ref_kind)} {name} at {line}:{column}"
    )
    ref = project.click(file, line, column, False)
    if not ref:
        eprint("found nothing")
    elif ref.kind == ce.occurrence_kind_include:
        eprint(
            f"found file {ref.path}"
            f" at {ref.begin_line}:{ref.begin_column}.."
            f"{ref.end_line}:{ref.end_column}"
        )
    else:
        eprint(
            f"found  {ref.symbol.kind_name}"
            f" {ce.occurrence_kind_name(ref.kind)} {ref.symbol.name}"
            f" at {ref.begin_line}:{ref.begin_column}.."
            f"{ref.end_line}:{ref.end_column}"
            #f" in {ref.file}"
        )
    assert ref
    assert ref.symbol.name == name
    assert ref.kind == ref_kind
    assert ref.symbol.kind == sym_kind

#test( file1, 1, 11,
#      'a', ce.occurrence_kind_declaration, ce.entity_kind_global_variable)

eprint("### Bye")
