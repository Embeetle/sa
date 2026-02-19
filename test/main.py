from template import *
import time
import sys

eprint("Test enter")
eprint("sys.path =", sys.path)

project_path = test_dir + "/STM32F303VC" 
source_folder = project_path + "/source/"
build_folder = "test/main/build"
makefile = project_path + "/config/buildConfig/makefile"
project = Project(build_folder, makefile, source_folder)
file = source_folder + "user_code/main.c"
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
    print(f"@add hdir {hdir}")
    project.set_hdir_mode(hdir, ce.hdir_mode_automatic)

print("@add file")
project.add_file(file, ce.FileMode.INCLUDE)
print("@add file done")
project.wait_for_analysis()
print("@analysis done")

if project.fail_count:
    eprint("analysis failed")
    exit(1)
eprint("analysis done")

# Errors are expected: there will be undefined globals
#if project.error_count:
#    eprint("analysis errors")
#    exit(1)

project.click(file, 140, 34)
project.reload_file(file)
project.click(file, 140, 34)
project.wait_for_analysis()
ref = project.click(file, 140, 34)
assert ref
assert ref.kind is not ce.occurrence_kind_include
assert ce.entity_kind_name(ref.symbol.kind) == 'global function'
ref = project.click(file, 51, 22)
assert ref
assert ref.kind is ce.occurrence_kind_include
ref = project.click(file, 37, 3)
assert ref is None

ce.abort()
eprint("Bye")
