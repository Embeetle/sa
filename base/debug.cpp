// Copyright © 2018-2026 Johan Cockx
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// SPDX-License-Identifier: GPL-3.0-or-later

#include <csignal>
#include "debug.h"
#include "print.h"
#include <set>
#include <pthread.h>
#include <atomic>
#include <unistd.h>
#include <stdlib.h>
#include <fstream>
#include "os.h"

using namespace std;
using base::Nest;
using base::PrefailAction;

debug_code(const char *base::SA_DEBUG = getenv("SA_DEBUG");)
debug_code(std::ostream *debug_stream_pointer = &std::cerr;)

//------------------------------------------------------------------------------
// Separate mutex for output for assertions, pointer checking and aborts, as
// these may occur while the normal debug output mutex is locked.
std::mutex base::check_mutex;

//------------------------------------------------------------------------------
std::mutex base::debug_mutex;

//------------------------------------------------------------------------------
unsigned base::get_pid()
{
  return ::getpid();
}

//------------------------------------------------------------------------------
static pthread_t main_thread = pthread_self();

//------------------------------------------------------------------------------
unsigned base::get_tid()
{
  return os::get_tid();
}

//------------------------------------------------------------------------------
bool base::in_main_thread()
{
  return pthread_self() == main_thread;
}

//------------------------------------------------------------------------------
static void delete_thread_specific_data(void *context)
{
  delete static_cast<base::DebugContext*>(context);
}

//------------------------------------------------------------------------------
static pthread_key_t create_thread_specific_data_key()
{
  pthread_key_t key;
  pthread_key_create(&key, delete_thread_specific_data);
  return key;
}

//------------------------------------------------------------------------------
base::DebugContext *base::DebugContext::get()
{
  static pthread_key_t key = create_thread_specific_data_key();
  DebugContext *context =
    static_cast<base::DebugContext*>(pthread_getspecific(key));
  if (!context) {
    context = new DebugContext();
    pthread_setspecific(key, context);
  }
  return context;
}

//------------------------------------------------------------------------------
base::DebugContext::DebugContext()
{
  // Postpone initialization of output file until first debug output.
}

//------------------------------------------------------------------------------
base::DebugContext::~DebugContext()
{
}

//------------------------------------------------------------------------------
const char *base::DebugContext::indent_string()
{
  enum {
    indent_step = 2,
    indent_max = 40
  };

  // The indent buffer contains spaces, or whatever characters are to be printed
  // for indentation. Its size is at least equal to the maximum indent supported:
  // deeper indents should "wrap" using a modulo approach. _indent_end points to
  // the buffer's closing zero byte.
  static char trace_indent_buffer[] =
    "                                                                       ";
  static char *trace_indent_end =
    trace_indent_buffer + sizeof(trace_indent_buffer) - 1;
  assert(sizeof(trace_indent_buffer) > indent_max);
  return trace_indent_end - (indent_level * indent_step) % indent_max;
}

//------------------------------------------------------------------------------
std::ostream &base::DebugContext::prefixed_stream()
{
  std::cout << std::flush;
  static bool process_id_reported = false;
  if (!process_id_reported) {
    process_id_reported = true;
    debug_stream << "# process ID: " << get_pid() << std::endl;
  }
  return debug_stream
    << "#" << std::fixed << std::setw(10) << std::setprecision(6)
    << base::get_time() << " " << get_tid() << " " << indent_string();
}

//------------------------------------------------------------------------------
void base::DebugContext::open_scope()
{
  ++indent_level;
}
  
//------------------------------------------------------------------------------
void base::DebugContext::close_scope()
{
  assert(indent_level != 0);
  --indent_level;
}

//------------------------------------------------------------------------------
base::Nest::Nest( bool flag ): flag(flag)
{
  if (flag) {
    DebugContext::get()->open_scope();
  }
}

//------------------------------------------------------------------------------
base::Nest::~Nest()
{
  if (flag) {
    DebugContext::get()->close_scope();
    debug_atomic_writeln("}");
  }
}

//------------------------------------------------------------------------------
namespace {
  class InitAndExitActions {
  public:
    InitAndExitActions()
    {
      warn_for_debug();
    }
    
    ~InitAndExitActions()
    {
      warn_for_debug();
    }

    void warn_for_debug()
    {
#ifndef NDEBUG
      if (base::SA_DEBUG) {
        debug_writeln("Process ID " << getpid());
#ifdef CHECK
        debug_writeln("");
        debug_writeln("#######################################"
          "##############################"
        );
        debug_writeln("Compiled with expensive checks enabled"
          " - recompile before release"
        );
        debug_writeln("#######################################"
          "##############################"
        );
#endif
      }
#endif
    }
  };
  static InitAndExitActions init_and_exit_actions;
}

//------------------------------------------------------------------------------
int breakpoint_count = 0;

//------------------------------------------------------------------------------
extern "C" void base::breakpoint()
{
  if (!breakpoint_count) {
    const char *SA_ATTACH = getenv("SA_ATTACH");
    if (SA_ATTACH) {
      std::cerr << "in gdb, try: attach " << get_tid() << "\n";
      std::cerr << "then go up to 'breakpoint', set break breakpoint, go=1\n";
      static volatile int go = 0;
      while (!go) {
        usleep(1000000);
      }
    }
  }
  ++breakpoint_count;
}
  
//------------------------------------------------------------------------------
extern "C" void base::assertion_failed(const char *message)
{
  debug_writeln("assertion failed in process " << get_pid()
    << " thread " << get_tid()
  );
  PrefailAction::execute(message);
  base::breakpoint();
  debug_atomic_writeln(
    "wait forever called by process " << get_pid() << " thread " << get_tid()
  );
  wait_forever();
  abort();
}
  
//------------------------------------------------------------------------------
extern "C" void base::wait_forever()
{
  // Don't attempt to lock the debug mutex here; there could also be an
  // exception while the debug mutex is already locked, for example while
  // writing output for a failed assertion.
  for (;;) {
    usleep(10000000);
  }
}

//------------------------------------------------------------------------------
std::atomic_size_t base::next_debug_uid = 1;

//------------------------------------------------------------------------------
static void handle_failure(int sig)
{
  // Before proceeding, set the signal behavior back to its default value to
  // avoid an infinite loop.
  signal(sig,SIG_DFL);
  
  std::stringstream message;
  message << "Exception " << sig << ": " << (
    sig == SIGABRT ? "abort called" :
    sig == SIGFPE ? "floating point exception" :
    sig == SIGILL ? "illegal instruction" :
    sig == SIGINT ? "keyboard interrupt (^C)" :
    sig == SIGSEGV ? "segmentation fault (illegal memory reference)" :
    sig == SIGTERM ? "terminate (default kill signal)" :
    ""
  );
  check_atomic_writeln(message.str());
  
  debug_writeln("enter prefail action");
  PrefailAction::execute(message.str().data());
  debug_writeln("leave prefail action");

  // Allow the debugger a chance to gain control
  base::breakpoint();

  switch (sig) {
    case SIGFPE:
    case SIGILL:
    case SIGSEGV:
    case SIGABRT:
      debug_writeln("calling wait forever");
      // Don't attempt to lock the debug mutex here; there could also be an
      // exception while the debug mutex is already locked, for example while
      // writing output for a failed assertion.
      debug_writeln(
        "wait forever called by process " << base::get_pid()
        << " thread " << base::get_tid()
      );
      debug_exec(base::wait_forever();)
      break;
    default:
      break;
  }
  // Return to retry the failed instruction.
}

//------------------------------------------------------------------------------
static int set_handlers()
{
  trace("set handlers");
  static int done = 0;
  if (!done) {
    signal(SIGABRT,handle_failure);
    signal(SIGFPE,handle_failure);
    signal(SIGILL,handle_failure);
    signal(SIGINT,handle_failure);
    signal(SIGSEGV,handle_failure);
    signal(SIGTERM,handle_failure);
    done = 1;
  }
  trace("SA_DEBUG set");
  return done;
}
static int handlers_set = set_handlers();

//------------------------------------------------------------------------------
static pthread_mutex_t prefail_mutex = PTHREAD_MUTEX_INITIALIZER;

//------------------------------------------------------------------------------
PrefailAction::PrefailAction()
{
  pthread_mutex_lock(&prefail_mutex);
  trace("### new PrefailAction " << this << " replaces " << top());
  (void)set_handlers;
  _next = top(); // init while mutex locked!
  _tid = get_tid();
  top() = this;
  pthread_mutex_unlock(&prefail_mutex);
}


//------------------------------------------------------------------------------
PrefailAction *&PrefailAction::top()
{
#if 0
  // Only works for single-threaded code
  static PrefailAction *top = 0;
  return top;
#else
  return DebugContext::get()->prefail_top;
#endif
}

//------------------------------------------------------------------------------
PrefailAction::PrefailAction(PrefailAction &other)
{
  pthread_mutex_lock(&prefail_mutex);
  trace("### copied PrefailAction " << this << " replaces " << top());
  (void)other;
  static bool handler_set = false;
  assert(handler_set);
  _next = top(); // init while mutex locked!
  _tid = get_tid();
  top() = this;
  pthread_mutex_unlock(&prefail_mutex);
}

//------------------------------------------------------------------------------
PrefailAction::~PrefailAction()
{
  pthread_mutex_lock(&prefail_mutex);
  trace("### delete PrefailAction " << this << " top=" << top());
  PrefailAction** p = &top();
  for (;;) {
    assert(*p);
    if (*p == this) {
      *p = _next;
      pthread_mutex_unlock(&prefail_mutex);
      return;
    }
    p = &((*p)->_next);
  }
}

//------------------------------------------------------------------------------
void PrefailAction::fail() {}

//------------------------------------------------------------------------------
void PrefailAction::execute(const char *message) {
  pthread_mutex_lock(&prefail_mutex);
  trace_nest("### execute prefail actions");
  //
  // Make sure that internal errors during execution of a prefail action
  // do not cause an infinite loop: use _top to iterate,  so that only actions
  // that have not been executed yet will execute on an internal error during
  // or after a prefail action.
  PrefailAction *&_top = top();
  while (_top) {
    PrefailAction* action = _top;
    _top = _top->_next;
    if (action->_tid == get_tid()) {
      trace_nest("### exec PrefailAction " << action);
      action->fail(message);
    }
  }
  pthread_mutex_unlock(&prefail_mutex);
}

//------------------------------------------------------------------------------
// Pointer validity checking
//------------------------------------------------------------------------------

// When debugging hard pointer-related problem, it is very useful to be able to
// check if a given pointer is a valid pointer (allocated and not freed) at a
// given point in the program.
//
// Pointer validity checking is implemented below using a static set of void*
// pointers that are currently allocated and not freed. The set is only
// maintained when the CHECK preprocessor macro is defined, and only for
// pointers for which the checked_pointer function are called (see debug.h).
// This includes classes derived from the "Checked" class (see debug.h).
//
// A set of valid pointers does not provide pointer allocation numbers. For this
// purpose, a large array of all allocation is maintained as well; pointers are
// never removed from this array, only added.

#ifdef CHECK
//------------------------------------------------------------------------------
static pthread_mutex_t ptr_mutex = PTHREAD_MUTEX_INITIALIZER;

//------------------------------------------------------------------------------
static void lock_ptr_mutex()
{
  pthread_mutex_lock(&ptr_mutex);
}

//------------------------------------------------------------------------------
static void unlock_ptr_mutex()
{
  pthread_mutex_unlock(&ptr_mutex);
}

//------------------------------------------------------------------------------
// Tnum is the allocation entry number to be traced.  Zero is the default value
// and causes no tracing.  It can be changed in the debugger or by recompilation
// with another initial value.
//
static const unsigned tnum = 0;

//------------------------------------------------------------------------------
// Tptr is the pointer corresponding to tnum, or null if that pointer has not
// been allocated yet.
//
static const void* tptr = 0;

//------------------------------------------------------------------------------
// A buffer with all pointers to checked objects in order of allocation.
static const void* buf[100000000];
static unsigned cur = 1;

//------------------------------------------------------------------------------
// The set of pointers known to be valid.  Do not rely on static construction,
// because pointers may be added before the static constructors have completed!
static std::set<const void*> *pset = 0;

//------------------------------------------------------------------------------
static void add_ptr( const void* ptr )
{
  lock_ptr_mutex();
  assert(ptr);

  // Valid pointer set
  assert(pset);
  pset->insert(ptr);

  // Pointers by allocation number
  assert(cur < sizeof(buf)/sizeof(buf[0]));
  if (cur == tnum) {
    tptr = ptr;
  }
  buf[cur] = ptr;
  ++cur;
  if (ptr == tptr) {
    debug_atomic_writeln("created: [#" << tnum << "]=" << ptr);
    base::breakpoint();
  }
  unlock_ptr_mutex();
}

//------------------------------------------------------------------------------
void base::checked_pointer::add( const void* ptr )
{
  trace_nest("chk::add " << ptr);
  lock_ptr_mutex();
  if (!pset) {
    pset = new std::set<const void*>;
    if (tnum) {
      debug_atomic_writeln(
        "Tracing pointer [#" << tnum << "] (not allocated yet)"
      );
    }
  }
  unlock_ptr_mutex();
  add_ptr(ptr);
}

//------------------------------------------------------------------------------
void base::checked_pointer::copy( const void* ptr, const void* src )
{
  trace("chk::copy " << ptr << " from " << src);
  assert(valid(src));
  add_ptr(ptr);
}

//------------------------------------------------------------------------------
void base::checked_pointer::remove( const void* ptr )
{
  trace("chk::remove " << ptr);

  // Pointers by allocation number
  if (ptr == tptr) {
    debug_atomic_writeln("destroyed: [#" << tnum << "]=" << ptr);
    base::breakpoint();
  }

  // Valid pointer set
  assert_(valid(ptr),"deleting invalid pointer " << ptr);
  assert(pset);
  lock_ptr_mutex();
  pset->erase(ptr);
  unlock_ptr_mutex();
}

//------------------------------------------------------------------------------
bool base::checked_pointer::istraced( const void* ptr )
{
  lock_ptr_mutex();
  bool result = ptr == tptr;
  unlock_ptr_mutex();
  return result;
}

//------------------------------------------------------------------------------
bool base::checked_pointer::valid( const void* ptr )
{
  trace_nest("chk::valid " << ptr);
  lock_ptr_mutex();

  // Valid pointer set
  bool valid = pset && pset->find(ptr) != pset->end();

  // Pointers by allocation number
  if (!valid) {
    size_t i = cur;
    size_t pdist = (size_t)-1;
    size_t ipdist = 0;
    size_t ndist = (size_t)-1;
    size_t indist = 0;
    do {
      --i;
      if (buf[i] == ptr) break;
      if ((size_t)ptr - (size_t)buf[i] < pdist) {
        pdist = (size_t)ptr - (size_t)buf[i];
        ipdist = i;
      }
      if ((size_t)buf[i] - (size_t)ptr < ndist) {
        ndist = (size_t)buf[i] - (size_t)ptr;
        indist = i;
      }
    } while (i);

    if (i) {
      check_atomic_writeln("invalid pointer [#" << i << "]=" << ptr << " "
        << (size_t)ptr << ": address is no longer allocated"
      );
    } else if (!ptr) {
      check_atomic_writeln("unexpected null pointer");
    } else {
      check_atomic_writeln("invalid pointer " << ptr << " " << (size_t)ptr
                   << ": address was never allocated"
      );
      if (indist) {
        check_atomic_writeln("ptr is " << ndist << " bytes below ptr#"
          << indist << " " << buf[indist] << " " << (size_t)buf[indist]
        );
      }
      if (ipdist) {
        check_atomic_writeln("ptr is " << pdist << " bytes above ptr#"
          << ipdist << " " << buf[ipdist] << " " << (size_t)buf[ipdist]
        );
      }
    }
    base::breakpoint();
  }
 
  unlock_ptr_mutex();
  return valid;
}

//------------------------------------------------------------------------------
bool base::checked_pointer::invalid( const void* ptr )
{
  trace_nest("chk::invalid " << ptr);
  lock_ptr_mutex();
  bool valid = pset && pset->find(ptr) != pset->end();
  if (valid) {
    base::breakpoint();
  }
  unlock_ptr_mutex();
  return !valid;
}

//------------------------------------------------------------------------------
size_t base::checked_pointer::find( const void* ptr )
{
  trace_nest("chk::find " << ptr);
  lock_ptr_mutex();

  for (size_t i = cur; i--; ) {
    if (buf[i] == ptr) {
      unlock_ptr_mutex();
      return i;
    }
  }
  unlock_ptr_mutex();
  return 0;
}

#endif
