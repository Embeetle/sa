// Copyright 2018-2024 Johan Cockx
#include "EmptyLoop.h"
#include "File.h"
#include "Project.h"
#include "base/debug.h"

sa::EmptyLoop::EmptyLoop( base::ptr<File> file, const Range &range)
  : file(file)
  , range(range)
{
  assert(is_valid(file));
  trace("create " << this << " " << *this);
}

sa::EmptyLoop::~EmptyLoop()
{
  prefail_dump("~EmptyLoop", EmptyLoop&, *this);
  trace_nest("delete " << this << " " << *this);
  assert_(!instance_count, *this);
  assert(file->project->is_locked());
  file->drop_empty_loop(this);
  assert(base::is_valid(file));
}

std::ostream &sa::operator<<(std::ostream &out, const EmptyLoop &empty_loop)
{
  assert(base::is_valid(&empty_loop));
  empty_loop.write(out);
  return out;
}

void sa::EmptyLoop::write(std::ostream &out) const
{
  assert(base::is_valid(this));
  assert(base::is_valid(file));
  out << "empty loop " << file->get_name() << "@" << range;
}

void sa::EmptyLoop::insert_instance()
{
  trace_nest("insert empty loop instance#" << (instance_count+1)
    << " @" << *this
  );
  if (!instance_count) {
    file->insert_empty_loop_in_file(this);
  }
  instance_count++;
  assert(instance_count);
}

void sa::EmptyLoop::remove_instance()
{
  trace_nest("remove empty_loop instance#" << instance_count
    << " @" << *this
  );
  assert(instance_count);
  instance_count--;
  if (!instance_count) {
    file->remove_empty_loop_in_file(this);
  }
}

#ifndef NDEBUG
#include <sstream>

std::string sa::EmptyLoop::get_debug_name() const
{
  std::stringstream buf;
  buf << *this;
  return buf.str();
}
#endif
