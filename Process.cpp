// Copyright 2018-2024 Johan Cockx
#include "Process.h"

sa::Process::Process(std::string const &name)
  : name(name)
{
  trace("create process");
}

sa::Process::~Process()
{
  Lock lock(this);
  trace_nest("destroy process");
  // A process cannot be deleted while it is running (in another thread).  It is
  // the application's responsibility to ensure this.
  assert(_is_up_to_date());
  assert(!runner);
}

void sa::Process::set_urgent(bool urgent)
{
  Lock lock(this);
  _set_urgent(urgent);
}

void sa::Process::_set_urgent(bool urgent)
{
  assert(is_locked());
  this->urgent = urgent;
  // todo: also reschedule created-but-not-running runner
}

void sa::Process::trigger()
{
  Lock lock(this);
  _trigger();
}

void sa::Process::_trigger()
{
  assert(is_locked());
  trace_nest("trigger '" << process_name() << "' blocked=" << block_count);
  if (runner) {
    trace("cancel existing");
    _cancel();
    assert(triggered);
    // Process will run again when the current runner returns.
  } else if (!triggered) {
    trace("trigger");
    triggered = true;
    grab();
    increment_ref_count();
    on_out_of_date();
    if (!block_count) {
      trace("start");
      start();
    }
  }
}

void sa::Process::block()
{
  Lock lock(this);
  _block();
}

void sa::Process::_block()
{
  assert(is_locked());
  trace_nest("inc block count '" << process_name() << "' from " << block_count);
  if (!block_count) {
    if (runner) {
      trace("cancel " << process_name());
      _cancel();
    }
    trace("block " << process_name());
    set_status(Blocked);
  }
  ++block_count;
  assert(block_count);
}

void sa::Process::unblock()
{
  Lock lock(this);
  _unblock();
}

void sa::Process::_unblock()
{
  assert(is_locked());
  assert(block_count);
  --block_count;
  trace_nest("dec block count '" << process_name() << "' to " << block_count);
  if (!block_count) {
    if (!triggered) {
      trace(process_name() << " is ready");
      set_status(Ready);
    } else if (runner) {
      trace(process_name() << " is cancelled but still running, so wait");
      assert(runner->cancelled());
    } else {
      trace("start " << process_name());
      start();
    }
  }
}

void sa::Process::execute()
{
  trace_nest("execute '" << process_name() << "'");
  // To make sure that 'runner' is up-to-date, always lock+unlock the process.
  // This allows the background thread to check for cancellation without
  // locking. Since we are locking anyway, we can also guarantee that the
  // process is always locked during set_status(...) calls.  Don't keep it
  // locked while running, because then, it cannot be cancelled.
  {
    Lock lock(this);
    // The block count might have been incremented between the moment the runner
    // was started and the moment it locked the process here. In that case, we
    // should not set the status, and not execute the run function.Status should
    // already be Blocked.
    if (block_count) {
      trace("blocked before execution");
      assert(status == Blocked);
      return;
    }
    set_status(Running);
  }
  run();
  {
    Lock lock(this);
    trace("executed '" << process_name() << "'");
  }
}

void sa::Process::epilog()
{
  bool drop_required = false;
  {
    Lock lock(this);
    trace_nest("epilog '" << process_name() << "'");
    assert(triggered);
    assert(runner);
    if (block_count) {
      trace(process_name() << " got blocked while waiting or running");
      assert(runner->cancelled());
      set_status(Blocked);
      runner = 0;
    } else if (runner->cancelled()) {
      trace(process_name() << " got cancelled while waiting or running");
      start();
    } else {
      trace(process_name() << " finished normally");
      set_status(Ready);
      triggered = false;
      drop_required = true;
      runner = 0;
      on_up_to_date();
    }
  }
  if (drop_required) {
    decrement_ref_count();
    drop();
  }
}

void sa::Process::start()
{
  assert(is_locked());
  assert(!block_count);
  assert(triggered);
  set_status(Waiting);
  runner = new Runner(this);
}

void sa::Process::cancel()
{
  Lock lock(this);
  _cancel();
}

void sa::Process::_cancel()
{
  assert(is_locked());
  assert(runner);
  assert(triggered);
  runner->cancel();
}

bool sa::Process::cancelled() const
{
  // assert(runner); // Runner is null for direct call
  return runner ? runner->cancelled() : false;
}

void sa::Process::set_status(Status status)
{
  assert(is_locked());
  this->status = status;
  on_status(status);
}

bool sa::Process::is_out_of_date() const
{
  Lock lock(this);
  return _is_out_of_date();
}

bool sa::Process::_is_out_of_date() const
{
  assert(is_locked());
  return triggered;
}

bool sa::Process::is_blocked() const
{
  Lock lock(this);
  return _is_blocked();
}

bool sa::Process::_is_blocked() const
{
  assert(is_locked());
  return block_count;
}

size_t sa::Process::get_block_count() const
{
  Lock lock(this);
  return _get_block_count();
}

size_t sa::Process::_get_block_count() const
{
  assert(is_locked());
  return block_count;
}

sa::Process::Runner::Runner(sa::Process *process): process(process)
{
  trace("create runner " << this << " " << process->process_name());
  schedule(process->urgent);
}

sa::Process::Runner::~Runner()
{
  trace_nest("delete runner " << this << " cancelled=" << cancelled());
  assert(base::is_valid(this));
  assert(base::is_valid(process));
  process->epilog();
}

void sa::Process::Runner::execute()
{
  trace_nest("Execute runner");
  assert(base::is_valid(process));
  process->execute();
}

//==============================================================================
#ifdef SELFTEST

#include "base/debug.h"
#include "base/time_util.h"
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <set>

static void do_something()
{
  unsigned long long msec = (unsigned long long)rand() * 95 / RAND_MAX + 5;
  test_out("do something for " << msec << " ms");
  usleep(msec*1000);
  test_out("did something for " << msec << " ms");
}

//------------------------------------------------------------------------------
class Linker: public sa::Process {
public:
  unsigned count = 0;
  
  Linker(): Process("linker")
  {
  }

  void run() override
  {
    test_out("Link");
    do_something();
    Lock lock(this);
    if (cancelled()) {
      test_out("Link cancelled");
    } else {
      test_out("Link done");
      ++count;
    }
  }

  void on_status(Status status) override
  {
    test_out(process_name() << " status: " << status);
  }

};

//------------------------------------------------------------------------------
class Unit: public sa::Process {
public:
  std::string const path;
  Linker *const linker;

  std::string flags;
  
  sem_t running;
  unsigned start_count = 0;
  unsigned finish_count = 0;
  
  Unit(std::string path, Linker *linker)
    : Process(path + "-analyzer"), path(path), linker(linker)
  {
    test_out_nest("Create " << process_name());
    sem_init(&running, 0, 0);
    if (!is_up_to_date()) {
      linker->block();
    }
  }

  ~Unit()
  {
    if (!is_up_to_date()) {
      linker->unblock();
    }
  }

  void set_flags(std::string &&flags)
  {
    assert(is_locked());
    if (flags != this->flags) {
      this->flags.swap(flags);
      _trigger();
    }
  }

  void run() override
  {
    {
      Lock lock(this);
      ++start_count;
      test_out("Analyze " << path << " start-count=" << start_count);
    }
    sem_post(&running);
    do_something();
    Lock lock(this);
    if (cancelled()) {
      test_out("Analyze " << path << " cancelled");
    } else {
      test_out("Analyze " << path << " done");
      ++finish_count;
      linker->trigger();
    }
  }

  void on_status(Status status) override
  {
    test_out(process_name() << " status: " << status);
  }

  void on_up_to_date() override
  {
    test_out_nest(process_name() << " report up-to-date");
    linker->unblock();
  }
  
  void on_out_of_date() override
  {
    test_out_nest(process_name() << " report out-of-date");
    linker->block();
  }

};

//------------------------------------------------------------------------------
class FlagExtractor: public sa::Process {
  Linker *const linker;
  std::vector<Unit*> old_targets;
  std::vector<Unit*> new_targets;
  double last_update = 0;
  

public:
  sem_t running;
  unsigned start_count = 0;
  unsigned finish_count = 0;
  std::string flag = "-f";
  
  FlagExtractor(Linker *linker)
    : Process("make-command-analyzer")
    , linker(linker)
  {
    sem_init(&running, 0, 0);
  }

  // Add target;  target should not be locked.
  Unit *add_target(const std::string &path)
  {
    test_out_nest("add target to makefile analyzer: " << path);
    Unit *target = new Unit(path, linker);
    target->block();
    target->trigger();
    Lock lock(this);
    new_targets.push_back(target);
    last_update = base::get_time();
    test_out("A1");
    _trigger();
    test_out("A2");
    return target;
  }
    
  void touch()
  {
    Lock lock(this);
    for (auto target: old_targets) {
      target->block();
      new_targets.push_back(target);
    }
    old_targets.clear();
    _trigger();
  }

protected:
  void on_status(Status status) override
  {
    test_out(process_name() << " status: " << status);
  }
  
  void run() override
  {
    test_out("Analyze make command: wait for more targets");
    ++start_count;
    sem_post(&running);
    base::sleep_until(last_update + 0.100);
    if (cancelled()) {
      test_out("Analyze make command: aborted due to new target");
      return;
    }
    test_out("Analyze make command: doit");
    do_something(); // compute flags for new targets
    Lock lock(this);
    if (cancelled()) {
      test_out("Analyze make command: cancelled");
    } else {
      test_out("Analyze make command: done");
      for (auto target: new_targets) {
        old_targets.push_back(target);
        Lock lock(target);
        target->set_flags(flag + " " + target->path);
        target->_unblock();
      }
      new_targets.clear();
      ++finish_count;
    }
  }
};


//------------------------------------------------------------------------------
int main()
{
  auto now = time(0); // now = 1645797380;
  std::cout << "Hello " << now << "\n";
  srand(now);

  Linker linker;
  FlagExtractor make(&linker);
  Unit *foo = make.add_target("foo.c");
  Unit *bar = make.add_target("bar.c");

  Task::set_number_of_workers(4);
  Task::start();

  sem_wait(&make.running);
  test_out("Add source file while make command analysis is running");
  base::sleep(0.050);
  Unit *fup = make.add_target("fup.c");

  sem_wait(&foo->running);
  test_out("Trigger foo while foo analysis is running");
  foo->trigger();
  
  test_out("Also block and unblock foo just for fun");
  foo->block();
  foo->unblock();
  Task::wait();

  test_out("Simulate changed makefile");
  make.flag = "-g";
  make.touch();
  Task::wait();

  test_out("Check results");
  {
    sa::Process::Lock lock(&make);
    assert_(make.start_count == 3, make.start_count);
    assert_(make.finish_count == 2, make.finish_count);
    assert(make._is_up_to_date());
  }
  {
    sa::Process::Lock lock(foo);
    assert_(foo->start_count == 3, foo->start_count);
    assert_(foo->finish_count == 2, foo->finish_count);
    assert_(foo->flags == "-g foo.c", "flags found: " << foo->flags);
    assert(foo->_is_up_to_date());
  }
  {
    sa::Process::Lock lock(bar);
    assert_(bar->start_count == 2, bar->start_count);
    assert_(bar->finish_count == 2, bar->finish_count);
    assert_(bar->flags == "-g bar.c", "flags found: " << bar->flags);
    assert(bar->_is_up_to_date());
  }
  {
    sa::Process::Lock lock(fup);
    assert_(fup->start_count == 2, fup->start_count);
    assert_(fup->finish_count == 2, fup->finish_count);
    assert_(fup->flags == "-g fup.c", "flags found: " << fup->flags);
    assert(fup->_is_up_to_date());
  }
  {
    sa::Process::Lock lock(&linker);
    assert_(linker.count==2, linker.count);
    assert(linker._is_up_to_date());
  }
  std::cout << "Bye\n";
}

#endif
