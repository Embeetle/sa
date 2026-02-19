from template import *

source_path = "/home/johan/bld/ce/test/project name with space/source/test/project name with space/source/Drivers/CMSIS/DSP_Lib/Source/BasicMathFunctions/arm_scale_q7.c"
source_dir  = "/home/johan/bld/ce/test/project name with space/source"
build_dir   = "/home/johan/bld/ce/test/project name with space/build"
#print("std path: " + standard_path(source_path, build_dir, source_dir))

sources = [
#    "Drivers/CMSIS/DSP_Lib/Source/BasicMathFunctions/arm_abs_f32.c",
#    "Drivers/CMSIS/DSP_Lib/Source/FilteringFunctions/arm_fir_f32.c",
#    "Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal.c",
    "user_code/main.c",
#    "user_code/stm32f3xx_hal_msp.c",
#    "user_code/stm32f3xx_it.c",
#    "user_code/system_stm32f3xx.c",
]

eprint("Test enter")

test_data = "test/project name with space"
try:
    shutil.rmtree(test_data)
except:
    pass
os.makedirs("test", exist_ok=True)
shutil.copytree(dev_dir + "/" + test_data, test_data)

source_folder = test_data + "/source/"
build_folder = test_data + "/build/"
makefile = "../config/buildConfig/makefile"
project = Project(build_folder, makefile, source_folder)
print("source folder: " + source_folder)

start = time.time()
files = []
for source in sources:
    file = project.get_file(source)
    files.append(file)
    if source == "user_code/main.c":
        file1 = file
t1 = time.time() - start
eprint("{} files added in {:.3f} seconds - {:.3f} ms per file".format(
    len(files), t1, 1000*t1/len(files)
))

project.wait_for_analysis()
t1 = time.time() - start
eprint("{} files analyzed in {:.3f} seconds - {:.3f} ms per file".format(
    len(files), t1, 1000*t1/len(files)
))
if project.get_file_analysis_status(file1) == ce.AnalysisStatus.FAILED:
    eprint("analysis error")
    print("diagnostics: \n{}".format('\n'.join(project.get_diagnostics())))
    exit(1)
eprint("analysis done")
project.click(file1, 153, 36)
eprint("clicked")
import gc
gc.collect()
eprint("collected")
project.click(file1, 153, 36)
eprint("clicked again")
project.reload_file(file1)
eprint("file reloaded")
project.click(file1, 153, 36)
eprint("quick click")
project.wait_for_analysis()
project.click(file1, 153, 36)
eprint("and clicked again")

eprint("Bye")
