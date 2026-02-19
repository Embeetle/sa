from template import *
import time
import os
import shutil

eprint("Hello")
top_dir = "test/ar"
build_dir = top_dir + "/build"
source_dir = top_dir + "/source"
try:
    shutil.rmtree(top_dir)
except:
    pass
print("test dir: " + test_dir)
shutil.copytree(test_dir + "/ar", top_dir)
makefile = '../config/makefile'
project = Project(build_dir, makefile, source_dir)
files = [
    "user_code/arduino_due_serial.ino.cpp",
    "libraries/HID/src/HID.cpp",
    "libraries/SPI/src/SPI.cpp",
    "libraries/Wire/src/Wire.cpp",
    "variants/arduino_due_x/variant.cpp",
    "variants/libsam_sam3x8e_gcc_rel.a",
    "core/arduino/wiring_pulse.cpp",
    "core/arduino/UARTClass.cpp",
    "core/arduino/avr/dtostrf.c",
    "core/arduino/hooks.c",
    "core/arduino/USARTClass.cpp",
    "core/arduino/WString.cpp",
    "core/arduino/wiring_analog.c",
    "core/arduino/iar_calls_sam3.c",
    "core/arduino/watchdog.cpp",
    "core/arduino/Print.cpp",
    "core/arduino/main.cpp",
    #"core/arduino/wiring_pulse_asm.S",
    "core/arduino/new.cpp",
    "core/arduino/IPAddress.cpp",
    "core/arduino/Stream.cpp",
    "core/arduino/itoa.c",
    "core/arduino/RingBuffer.cpp",
    "core/arduino/WMath.cpp",
    "core/arduino/wiring_shift.c",
    "core/arduino/cortex_handlers.c",
    "core/arduino/Reset.cpp",
    "core/arduino/USB/CDC.cpp",
    "core/arduino/USB/USBCore.cpp",
    "core/arduino/USB/PluggableUSB.cpp",
    "core/arduino/syscalls_sam3.c",
    "core/arduino/wiring.c",
    "core/arduino/abi.cpp",
    "core/arduino/wiring_digital.c",
    "core/arduino/WInterrupts.c",
]
#files = [ "variants/libsam_sam3x8e_gcc_rel.a" ]
#files = [ "libraries/Wire/src/Wire.cpp" ]
hdirs = [
    "core/arduino",
    "other/packages/arduino/hardware/sam/1.6.12/system/CMSIS/CMSIS/Include",
    "other/packages/arduino/hardware/sam/1.6.12/system/CMSIS/Device/ATMEL",
    "other/packages/arduino/hardware/sam/1.6.12/system/libsam",
    "variants/arduino_due_x",
]
for file in files:
    project.add_file(file, ce.FileMode.AUTOMATIC)
for hdir in hdirs:
    project.set_hdir_mode(hdir, ce.hdir_mode_automatic)
project.wait_for_analysis()
#text = project.get_text_for_binary("variants/libsam_sam3x8e_gcc_rel.a")
#print(f'text for binary:\n{text}')
project.print_diagnostics()
print(f"linker status: {ce.linker_status_name(project.linker_status)}")
#assert project.linker_status == ce.LinkerStatus.DONE
#assert project.get_diagnostic_count() == 0

#assert project.hdir_used('inc')
#assert project.file_linked("simple_main.c")
#ref = project.click("simple.c", 5)
#assert ref
#assert ref.kind == ce.occurrence_kind_include

eprint("Bye")
