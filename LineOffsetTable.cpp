// Copyright 2018-2024 Johan Cockx
#include "LineOffsetTable.h"
#include "base/debug.h"
#include <string.h>

sa::LineOffsetTable::LineOffsetTable(std::istream &in)
{
  trace_nest("LineOffsetTable");
  char buf[(
#ifdef SELFTEST
    100
#else
    1<<16
#endif
    ) + 1
  ];
  unsigned buf_offset = 0;
  unsigned end_offset = 0;
  offsets.push_back(0);
  for (;;) {
    in.read(buf, sizeof(buf)-1);
    auto count = in.gcount();
    if (!count) {
      // End of stream.
      offsets.push_back(end_offset);
      break;
    }
    trace("read " << count << " bytes");
    buf[count] = '\n'; // Sentinel for strchr
    buf_offset = end_offset;
    end_offset += count;
    char *cur = buf;
    for (;;) {
      cur = strchr(cur, '\n');
      if (cur == buf + count) {
        break;
      }
      ++cur;  // Skip end-of-line char
      offsets.push_back(buf_offset + (cur-buf));
    }
  }
  for (unsigned i = 0; i < offsets.size()-1; ++i) {
    trace(i << " offset=" << offsets[i] << " len=" << (offsets[i+1]-offsets[i]));
  }
  trace((offsets.size()-1) << " length=" << offsets.back());
}
  

// Return the number of lines in the file.  This is one more than the number
// of end-of-line characters (\n).  In other words, a file without
// end-of-line characters consists of one line, with line number zero. If
// the file contents end with an end-of-line character, then the last line
// is empty.
unsigned sa::LineOffsetTable::line_count() const
{
  assert(offsets.size());
  return offsets.size()-1;
}

// Return the length of a line. The line number must be less than the line
// count.
unsigned sa::LineOffsetTable::line_length(unsigned line) const
{
  assert(line < line_count());
  return offsets[line+1] - offsets[line];
}

// Return true iff the given (line,column) position corresponds to a
// position in the file. Line and column are both zero-based.
bool sa::LineOffsetTable::in_file(unsigned line, unsigned column) const
{
  return line < line_count() && column < line_length(line);
}

unsigned sa::LineOffsetTable::offset(unsigned line, unsigned column) const
{
  assert(in_file(line,column));
  return offsets[line] + column;
}

unsigned sa::LineOffsetTable::offset_or_zero(unsigned line, unsigned column)
  const
{
  return in_file(line,column) ? offset(line, column) : 0;
}

#ifdef SELFTEST

#include <sstream>

int main()
{
  const char data[] =
    "    .syntax unified\n"
    "    .arch armv7e-m\n"
    "\n"
    "//#include \"empty.h\"\n"
    "\n"
    "//#include <empty.h>\n"
    "\n"
    "#ifdef __STARTUP_CONFIG\n"
    "#include \"startup_config.h\"\n"
    "#ifndef __STARTUP_CONFIG_STACK_ALIGNEMENT\n"
    "#define __STARTUP_CONFIG_STACK_ALIGNEMENT 3\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "    .section .bss\n"
    "johan:\n"
    "    .long 0\n"
    "    \n"
    "#if 0   \n"
    "    .globl johan\n"
    "    .type johan, %function\n"
    "#endif\n"
    "\n"
    "    \n"
    "    .section .stack\n"
    "#if defined(__STARTUP_CONFIG)\n"
    "    .align __STARTUP_CONFIG_STACK_ALIGNEMENT\n"
    "    .equ    Stack_Size, __STARTUP_CONFIG_STACK_SIZE\n"
    "#elif defined(__STACK_SIZE)\n"
    "    .align 3\n"
    "    .equ    Stack_Size, __STACK_SIZE\n"
    "#else\n"
    "    .align 3\n"
    "    .equ    Stack_Size, 8192\n"
    "#endif\n"
    ;
  std::cout << "Hello " << (sizeof(data)-1) << "\n";
  std::istringstream in(data);
  sa::LineOffsetTable table(in);
  test_out(table.line_count() << " lines");
  for (unsigned i = 0; i < table.line_count(); ++i) {
    test_out("line " << i << " length is " << table.line_length(i));
  }
  test_out("Offset of 20.10 is " << table.offset(20,10));
  assert(table.line_count() == 36);
  assert(table.line_length(20) == 27);
  assert(table.line_length(35) == 0);
  assert(table.line_length(18) == 9);
  assert(table.in_file(18,0));
  assert(table.in_file(18,8));
  assert(!table.in_file(18,9));
  assert(!table.in_file(36,0));
  assert(table.offset(20,10) == 315);
  test_out("Selftest succeeded");
  return 0;
}

#endif
