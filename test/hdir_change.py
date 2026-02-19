from template import *
import time
import sys
import shutil

eprint("@Test enter")

project_path = test_dir + "/STM32F303VC" 
source_folder = project_path + "/source/"
build_folder = "test/hdir_change/build"
shutil.rmtree(build_folder, ignore_errors=True)
shutil.rmtree("test/hdir_change/cache", ignore_errors=True)
makefile = project_path + "/config/buildConfig/makefile"
project = Project(build_folder, makefile, source_folder)
file = source_folder + "user_code/main.c"
eprint("@add file")
project.add_file(file, ce.FileMode.INCLUDE)
for hdir in [
        "Drivers/CMSIS/Include",
        "Drivers/CMSIS/Device/ST/STM32F3xx/Include",
        "Drivers/CMSIS/NN/Include",
        "Drivers/CMSIS/DSP/Include",
        "Drivers/STM32F3xx_HAL_Driver/Inc",
        "Inc",
        "Middlewares/ST/STM32_USB_Host_Library/Core/Inc",
        "Middlewares/ST/STM32_USB_Host_Library/Class/CDC/Inc",
        "Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS",
]:
    eprint(f"@add hdir {hdir}")
    project.set_hdir_mode(hdir, ce.hdir_mode_automatic)
    time.sleep(0.01)

project.add_file(file, ce.FileMode.INCLUDE) #remove later
project.wait_for_analysis()
eprint("@analysis done")

if project.fail_count:
    eprint("analysis failed")
    exit(1)
eprint("@analysis worked")

# Errors are expected: there will be undefined globals
project.print_diagnostics()
if project.ignore_undefined_globals:
    assert project.error_count == 1, project.error_count
else:    
    assert project.error_count == 30, project.error_count
    project.set_diagnostic_limit(ce.Severity.ERROR, 10)
    assert project.error_count == 10, project.error_count
    project.set_diagnostic_limit(ce.Severity.ERROR, 40)
    assert project.error_count == 30, project.error_count
    project.set_diagnostic_limit(ce.Severity.ERROR, 30)
    assert project.error_count == 30, project.error_count

ref = project.click(file, 140, 34)
assert ref
assert ref.kind is not ce.occurrence_kind_include
assert ce.symbol_kind_name(ref.symbol.kind) == "global function"
eprint("symbol name: " + ref.symbol.name)
assert ref.symbol.name == "osThreadCreate"
ref = project.click(file, 51, 22)
assert ref
assert ref.kind is ce.occurrence_kind_include
ref = project.click(file, 37, 3)
assert ref is None

ce.abort()
eprint("Bye")
