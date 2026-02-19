// Copyright 2018-2023 Johan Cockx
// Debugging utilities.

#ifndef __base_debug_h
#define __base_debug_h

#include "time_util.h"
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <typeinfo>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>

//------------------------------------------------------------------------------
// Assertions
//------------------------------------------------------------------------------
//
// If NATIVE_ASSERT is defined, use the built-in assert functionality;
// otherwise, use our own version.  Our own version of assert writes an
// end-of-line before reporting a failed assertion, thus making sure that the
// report always starts at the beginning of a line and is therefor easier to
// locate, both visually and for emacs.
//
// The assert_ macro is equivalent to the standard assert, except that it takes
// an additional argument providing details of the failed assertion in the form
// of an expression that can be shifted onto an ostream.
//

//#define assert_static(condition)


#ifdef NATIVE_ASSERT

// Use built-in assert
#include <cassert>

// Define assert_ for compatibility with code relying on our own definition.
#ifdef NDEBUG
#define assert_(condition,detail)
#else
#define assert_(condition,detail) if (!(condition)) {                   \
    debug_stream << "\nAssertion details: " << detail << "\n";         \
    assert(condition))                                                  \
  }
#endif

#else
// Use our own assert.
#ifdef assert
#error debug.h is not compatible with assert.h
#endif
#undef assert
#undef assert_

#define assert(condition) debug_assert(condition,"")
#define assert_(condition,detail) \
  debug_assert(condition,"Assertion details: " << detail << "\n")

// "Check" assertions are expensive assertions, only asserted when CHECK is
// defined.
#define check_assert(condition) check_code(assert(condition))
#define check_assert_(condition, detail) check_code(assert_(condition, detail))

#ifdef NDEBUG

// Ignore assertions
#define debug_assert(condition,detail) {}

#else

#define debug_assert(condition,detail) if (!(condition)) {                 \
  check_atomic_writeln(detail << ASSERTION_FAILED_MESSAGE(condition));     \
  base::assertion_failed(ASSERTION_FAILED_MESSAGE(condition));             \
  }

#define ASSERTION_FAILED_MESSAGE(condition) \
  CONCATENATE(ASSERTION_PREFIX, ": Assertion `" #condition "' failed.")

#ifdef __GNUC__

// GNU compilers define __PRETTY_FUNCTION__,  so use it.
#define ASSERTION_PREFIX CONCATENATE( \
    __FILE__ ":" EXPAND_AND_STRINGIFY(__LINE__) ": ", \
    __PRETTY_FUNCTION__ \
  )
#define CONCATENATE(s1, s2) (std::string(s1) + s2).data()

#else

// Non-GNU compilers don't define __PRETTY_FUNCTION__,  so don't use it.
#define ASSERTION_PREFIX \
  __FILE__ ":" EXPAND_AND_STRINGIFY(__LINE__)

#endif // __GNUC__

#define STRINGIFY(x) #x
#define EXPAND_AND_STRINGIFY(x) STRINGIFY(x)
#endif // NDEBUG
#endif // NATIVE_ASSERT

//------------------------------------------------------------------------------
// Execute check code atomically.
#define check_atomic_code(code) debug_code(                         \
  {                                                                 \
    const std::lock_guard<std::mutex> lock(base::check_mutex);      \
    code;                                                           \
  }                                                                 \
)

//------------------------------------------------------------------------------
// Atomically write check output line with prefix
#define check_atomic_writeln(desc) check_atomic_code(debug_writeln(desc))


//------------------------------------------------------------------------------
// Expensive checks
//------------------------------------------------------------------------------
//
// Code surrounded by a check_code macro is only compiled when explicitly
// requested.

#ifdef CHECK
#define check_code(x) x
#else
#define check_code(x)
#endif

//------------------------------------------------------------------------------
// Safe array access
//------------------------------------------------------------------------------

#define array_size(array) (sizeof(array)/sizeof(*array))

//------------------------------------------------------------------------------
// Debugging code
//------------------------------------------------------------------------------

// Mark code as debug code: execute it unless NDEBUG is defined.
#ifdef NDEBUG
#define debug_code(code)
#else
#define debug_code(code) code
#endif

// Lock debug output in current scope.
#define debug_lock_output \
  const std::lock_guard<std::mutex> lock(base::debug_mutex)

// Execute debug code atomically.
#define debug_atomic_code(code) debug_code(                         \
  {                                                                 \
    debug_lock_output;                                              \
    code;                                                           \
  }                                                                 \
)

namespace base {
  debug_code(extern const char *SA_DEBUG;)
}

// Execute code when SA_DEBUG is set unless NDEBUG is defined.
#define debug_exec(code) debug_code(if (base::SA_DEBUG) { code; })
#define debug_atomic_exec(code) debug_atomic_code(if (base::SA_DEBUG) { code; })

// Code to execute when SA_DEBUG is *not* set, e.g. cleanup code
#ifdef NDEBUG
#define no_debug_code(code) code
#else
#define no_debug_code(code) if (!base::SA_DEBUG) { code; }
#endif
  
//------------------------------------------------------------------------------
// Debug output
//------------------------------------------------------------------------------

// Write to debug output stream.
//
// Debug output is prefixed by a '#', timestamp and thread id.
// It can be indented using the dout_nest() macro.  This macro opens a
// new scope by writing '{' and increases the current indentation level. While
// the indentation level is greater than zero, dout() will generate extra
// indentation after the prefix. The scope will automatically close - and the
// indentation level will decrease - when the macro that started it goes out of
// scope.
//
// In a multi-threaded context, indentation is per thread.  To make sure that
// writing happens atomically, use the debug_atomic_code macro.

// Write debug output prefix. Allows appending more output.
#define debug_write_prefix() ::base::DebugContext::get()->prefixed_stream()

// Write debug output line with prefix.
#define debug_writeln(desc) \
  debug_exec(debug_write_prefix() << desc << std::endl << std::flush;)

// Atomically write debug output line with prefix
#define debug_atomic_writeln(desc) debug_atomic_exec(debug_writeln(desc))

// Output stream for error messages such as failed assertions and tracing output
#define debug_stream (*debug_stream_pointer)
debug_code(extern std::ostream *debug_stream_pointer;)

// Start a new scope for debug output and increase indentation.  The scope is
// opened on the current line.  This is a statement, not an expression.
//
// Note: gcc 4.5.1 has a bug when the default value of the constructor argument
// for the Nest class is used: the constructor is not actually called.
// Work-around: specify it explicitly.
#define dout_nest() debug_code(::base::Nest _nest_(true))

//------------------------------------------------------------------------------
// Tracing
//------------------------------------------------------------------------------

// Tracing output is debugging output that can easily and flexibly be enabled or
// disabled. In a multi-threaded context, output is always atomic per trace
// statement.
//
// The trace(desc) and trace_nest(desc) macros take an argument 'desc' that
// describes the information to be traced.  This can be any printable expression
// or a list of such expressions separated by '<<' operators. The macros
// themselves are statements, not expressions.
//
// Trace output is only generated in marked regions of code or time spans in the
// execution of the code. To activate tracing in an arbitrary region of code,
// put '#define TRACE 1' at the start of the region, and '#undef TRACE' at the
// end. To activate it within the scope of a block of code, use 'enum{TRACE=1};'
// in that block.
//
// To control tracing with a runtime boolean expression 'b', use '#define TRACE
// b'.
//
// To activate dynamic tracing across compilation units, use '#define TRACE
// DYNAMIC' and trace_dynamic(b).
//

// Write a line of tracing output when tracing is enabled.
#define trace(desc) trace_code(debug_atomic_writeln(desc))

// Enclose tracing output of the current scope in a labeled block and increase
// indentation.
//
// Note: the nest variable used to implement this cannot be surrounded by the
// trace_code macro, because that would place it in a conditional block so that
// it would not be in the current scope but in a nested scope.
#define trace_nest(desc) trace_nest_core(desc << " {")

// Nest without description.
#define trace_nest_here() trace_nest_core("{")

// Aux macro
#define trace_nest_core(fulldesc) \
 trace(fulldesc); \
 debug_code(::base::Nest _nest_(TRACE))

// Execute code (a statement) when tracing is active.
#define trace_code(code) debug_code(                                \
    if (TRACE) {                                                    \
      code;                                                         \
    }                                                               \
  )

#define test_out(x) debug_atomic_writeln(x)
#define test_out_nest(desc) test_out_nest_core(desc << " {")
#define test_out_nest_here() out_nest_core("{")
#define test_out_nest_core(fulldesc) \
 test_out(fulldesc); \
 debug_code(::base::Nest _nest_(1))

// Execute code (a statement) atomically when tracing is active.
#define trace_atomic_code(code) debug_code(                         \
    if (TRACE) {                                                    \
      debug_atomic_code(code);                                      \
    }                                                               \
  )

// Evaluate an expression when tracing is active.
#define trace_expr(expr) (debug_code((TRACE)?((void)(expr)):)(void)0)

// Fallback value when TRACE is not #defined;
// - must be compile time constant,  so tracing code gets optimized away by any
//   reasonable optimizing compiler;
// - must be declared *before* #define TRACE to avoid syntax errors,  so
//   this header file is best included first.
//
debug_code(enum { TRACE = 0 };)

#define trace_dynamic(b) \
  debug_code(::base::DynamicTrace _dynamic_trace_(b))

#define DYNAMIC ::base::DynamicTrasa::active()

namespace std {

  //----------------------------------------------------------------------------
  // Tracing support
  //----------------------------------------------------------------------------

  //----------------------------------------------------------------------------
  // Write a vector of printable objects to an output stream
  template <typename T>
  ostream& operator<<(ostream& out, const vector<T>& v) {
    out << "{";
    for (typename vector<T>::size_type i = 0; i < v.size(); ++i) {
      if (i != 0) out << ",";
      out << v[i];
    }
    out << "}";
    return out;
  }
}

namespace base {

  //--------------------------------------------------------------------------
  // Debugging support
  //--------------------------------------------------------------------------

  // Mutex for debug output
  extern std::mutex debug_mutex;

  // Separate mutex for output for assertions, pointer checking and aborts, as
  // these may occur while the normal debug output mutex is locked.
  extern std::mutex check_mutex;

  class PrefailAction;

  //--------------------------------------------------------------------------
  // In a multi-threaded environment, debugging context (such as nesting level)
  // needs to be stored in thread-specific data. The debug context struct
  // collects all thread-specific debug data.
  struct DebugContext {
    // Get the debug context for the current thread.
    static DebugContext *get();
    
    // Destructor;  called automatically when thread exits.
    ~DebugContext();

    // Write the debug prefix for this thread to the debug output stream and
    // return it.
    std::ostream &prefixed_stream();

    // Return a string representing the current indentation.
    const char *indent_string();

    // Open a debug output scope and increase indentation.
    void open_scope();

    // Close a debug output scope and decrease indentation.
    void close_scope();
    
    bool dynamic_trace_active = false;
    
    PrefailAction *prefail_top = 0;
    
  private:
    DebugContext();
    unsigned indent_level = 0;
  };

  //--------------------------------------------------------------------------
  // C++ with templates is sometimes hard to debug.  It is not always possible
  // to place breakpoints at the desired statement.  As an alternative,
  // insert a call to the simple (and empty) breakpoint function at the
  // desired location and set a breakpoint in the breakpoint function.
  extern "C" void breakpoint();
  
  //--------------------------------------------------------------------------
  // When an assertion fails, it can be useful to take some action before
  // aborting. This is the function that does it.
  extern "C" void assertion_failed(const char *message);
  
  //--------------------------------------------------------------------------
  // When an assertion fails, it can be useful to not immediately abort, so
  // that a debugger can be attached while the program is still running.
  // This function waits forever.
  extern "C" void wait_forever();

  //--------------------------------------------------------------------------
  // Get process and thread ID.
  unsigned get_pid();
  unsigned get_tid();
  bool in_main_thread();
  
  //--------------------------------------------------------------------------
  // Get a unique integer to be used as ID. Values start from 1.
  // TODO: use atomics
  extern std::atomic_size_t next_debug_uid;
  inline size_t get_debug_uid() {
    return next_debug_uid.fetch_add(1, std::memory_order_relaxed);
  }
  
  //--------------------------------------------------------------------------
  // Return class name for a given class pointer.
  // To be used for debugging only.
  template <class T> const char* class_name(T* t) {
    if (t) {
      const char* name = "<class>"; // typeid(*t).name()
      while (isdigit(*name)) ++name;
      return name;
    } else {
      return "(null pointer - unknown class)";
    }
  }

  //--------------------------------------------------------------------------
  // Protection against unintended copying.
  //--------------------------------------------------------------------------
  
  //--------------------------------------------------------------------------
  // Class detecting unintentional copying of derived classes.
  // Declares a private copy constructor and assignment operator to suppress
  // the compiler generated copy constructor and assignment operator.  To
  // allow copying,  derived classes will have to explicitly define their
  // own copy constructor and assignment operator.
  //
  // Implementation note concerning gcc-3.4.0 and later:
  // since gcc-3.4.0,  it is no longer possible to pass a temporary object
  // as a const reference argument unless the copy constructor is
  // accessible,  even if it is not actually called.  I suspect this is a
  // consequence of stricter ANSI standard adherence.  To allow this use
  // while still conforming to gcc-3.4.0 requirements (and probably the ANSI
  // standard),  it is no longer possible to declare the copy constructor
  // private and detect problems at compile time.  The alternative is to
  // make the copy constructor public but provide no implementation,  so
  // that a link time error occurs.  The problem however with that approach
  // is that the linker provides no hint as to what caused the problem.
  // Currently,  the situation causing problems no longer occurs in this
  // code,  so the copy constructor is declared private again.
  // A possible workaround may be to declare a public copy constructor
  // (without implementation) in the derived class causing a problem.
  //
  class NoCopy {
      
  public:
    NoCopy() {}
    // When a copy constructor is present,  the compiler no longer generates
    // a default constructor,  so an explicit default constructor is provided
    // here.

  private:
    NoCopy(const NoCopy&);
    // Implicit copy construction is forbidden.  This constructor is
    // intentionally not defined.

    const NoCopy& operator=(const NoCopy&);
    // Implicit assignment is forbidden.  This constructor is intentionally not
    // defined.
  };

  //--------------------------------------------------------------------------
  // Auxiliary class for debug output
  //--------------------------------------------------------------------------

  //--------------------------------------------------------------------------
  // Aux class to indent debug output.  Constructor opens scope and increases
  // indentation level.  Destructor decreases indentation level and closes
  // scope. Opening and closing parentheses are written to the debug_stream.
  //
  class Nest: public NoCopy {
  public:

    // Open a new nesting level for debug output.  Increment indentation level.
    // Implemented inline because it makes use of the debug_stream macro which
    // may be redefined.
    //
    // The flag argument determines whether a scope is actually opened or not.
    // This may depend on whether tracing is active or not.  The value is stored
    // and used again at destruction time, so that closing of scopes is always
    // consistent with opening of scopes. Usually, the value of the flag will be
    // a compile time constant, so the compiler should be able to optimize it
    // out.
    //
    Nest( bool flag = true );

    // Close a nesting level for debug output.  Decrement the indentation level.
    // Implemented inline because it makes use of the debug_stream macro which
    // may be redefined.
    ~Nest();

  protected:
    bool flag;
  };

  //--------------------------------------------------------------------------
  // Auxiliary class for dynamic tracing
  //--------------------------------------------------------------------------

  //--------------------------------------------------------------------------
  class DynamicTrace {
    // Aux class to control dynamic tracing across compilation units.
    // Constructor sets the 'active' flag to a given value.  Destructor
    // restores the previous value.
    //
  public:
    //
    static bool active() { return DebugContext::get()->dynamic_trace_active; }
    //
    DynamicTrace( bool flag = true ) : _old_active(active()) {
      DebugContext::get()->dynamic_trace_active = flag;
    }
    //
    ~DynamicTrace() {
      DebugContext::get()->dynamic_trace_active = _old_active;
    }
    //
  protected:
    //
    bool _old_active;
  };

  //----------------------------------------------------------------------------
  // Prefail actions
  //----------------------------------------------------------------------------

  //----------------------------------------------------------------------------
  class PrefailAction {
    // Define an action to be executed just before the program aborts due to a
    // failing assertion or other failure (such as a signal).  A failure causes
    // PrefailAction::execute() to be called, which will in turn call the
    // virtual fail() method of all actions on the prefail action stack.
    //
    // The constructor puts the prefail action on top of the stack, and the
    // destructor removes it from the stack.  Prefail actions on the stack are
    // executed in top-down order.  If a prefail action fails, e.g. due to a
    // failing assertion, prefail actions below it on the stack will still be
    // executed.
    //
    // See base::ObjectContext/debug_context and
    // base::SourceContext/debug_source_reference for usage examples.
    // 
    
  public:
    PrefailAction();
    PrefailAction(PrefailAction &other);
    virtual ~PrefailAction();
    virtual void fail(const char *message) { (void)message; fail(); }
    virtual void fail();
    static void execute(const char *message);
  protected:
    static PrefailAction *&top();
    unsigned _tid;
    PrefailAction *_next;
  };
  
  //----------------------------------------------------------------------------
  // A class that dumps any printable object to standard error just before the
  // program aborts.  The idea is to provide context for the problem that causes
  // the abort by instantiating this class in a scope in which the object is
  // processed. All live instances are placed on a stack and printed in reverse
  // chronological order, preceded by a label.  The label is intended to
  // describe the processing performed on the object.
  //
  // To allow any type of object to be dumped, the type must be specified
  // explicitly as a template argument.  Macros for specific object types
  // including void are provided below.
  //
  // A copy of the specified object is kept inside PrefailDump.  To avoid
  // copying, specify a reference type as template argument.
  //
  template <typename Object> class PrefailDump: public PrefailAction {
    
  public:
    PrefailDump(Object object, const char* label):
      _object(object),
      _label(label)
    {
      assert(label);
    }
    
    ~PrefailDump() {}
    
    void fail() {
      debug_writeln("while " << _label << " " << _object);
    }
      
  protected:
    Object _object;
    const char* _label;
  };

  template <> class PrefailDump<void>: public PrefailAction {
    
  public:
    PrefailDump(const char* label):
      _label(label)
    {
      assert(label);
    }
    
    ~PrefailDump() {}
    
    void fail() {
      debug_writeln("while " << _label);
    }
      
  protected:
    const char* _label;
  };

  //----------------------------------------------------------------------------
  // Dump a labeled object if execution is aborted in the scope of this macro.
  // This can be very helpful for debugging, as it can provide some context
  // information for the cause of the termination.  Usable for any type of
  // object (except void).
#define prefail_dump(label,type,object)           \
  debug_code(base::PrefailDump<type> _d_(object,label))

  //----------------------------------------------------------------------------
  // Dump a label if execution is aborted in the scope of this macro.
  // This can be very helpful for debugging, as it can provide some context
  // information for the cause of the termination.
#define debug_context(label) \
  debug_code(base::PrefailDump<void> _d_(label))

  //----------------------------------------------------------------------------
  // Dump a label if execution is aborted in the scope of this macro.
  // Takes a std::string as argument.
#define debug_string_context(label, data)                        \
  debug_code(base::PrefailDump<std::string> _d_(data, label))

  //--------------------------------------------------------------------------
  // Pointer tracing and validity checking
  //--------------------------------------------------------------------------
  
  //--------------------------------------------------------------------------
  // Functions to check pointers for validity.  The idea is to call the
  // appropriate function when a pointer is allocated or deleted.  When this is
  // done, it becomes possible to check whether such a pointer is valid.
  // Pointers for which the add-function is never called wil be reported as
  // invalid.
  //
  // When an invalid pointer is detected, one would often like to know if the
  // target object was never allocated (uninitialized or overwritten pointer),
  // or was allocated and later deallocated (dangling pointer).  One way to
  // achieve this is to write out the invalid pointer value when it is detected,
  // and then re-run the program while checking for this pointer.  However, this
  // only works if allocation is reproducible, i.e. allocation returns the same
  // addresses in each run (with identical input data).  Unfortunately,
  // allocation is not always reproducible on all platforms.
  //
  // As an alternative, the allocation sequence number of an invalid pointer is
  // reported in addition to the pointer value itself. In debug.C, a specific
  // pointer allocation sequence number ('tnum') can be "traced",
  // i.e. allocation and freeing of this pointer can be reported, and the
  // "breakpoint" function (see above) is called when this happens.
  //
  namespace checked_pointer {
    void add(const void *ptr);
    void copy(const void *ptr, const void *src);
    void remove(const void *ptr);
    bool valid(const void *ptr);
    bool invalid(const void *ptr);
    bool istraced(const void *ptr);
    size_t find(const void *ptr);

#ifndef CHECK
    // Dummy inline implementation of checked_pointer functions when CHECK is
    // not defined.
    inline void add(const void* ptr) { (void)ptr; }
    inline void copy(const void* ptr, const void* src) { (void)ptr; (void)src; }
    inline void remove(const void* ptr) { (void)ptr; }
    inline bool valid(const void* ptr) { return ptr; }
    inline bool invalid(const void* ptr) { return !ptr; }
    inline bool traced(const void* ptr) { (void)ptr; return false; }
    inline size_t find(const void *ptr) { (void)ptr; return 0; }
#endif
  };
  
  // --------------------------------------------------------------------------
  // Base class allowing pointers to be checked for validity.  Maintains a set
  // with pointers to all current instances.  A pointer is valid iff it is in
  // the set.
  //
  class Checked {
  public:
    Checked() { checked_pointer::add(this); }
    Checked(const Checked& other) { checked_pointer::copy(this,&other); }
    ~Checked() { checked_pointer::remove(this); }
    size_t object_number() const { return checked_pointer::find(this); }
  };

  inline bool is_valid(const Checked *ptr)
  {
    return checked_pointer::valid(ptr);
  }
  
  inline bool is_valid_or_null(const Checked *ptr)
  {
    return !ptr || is_valid(ptr);
  }

  inline bool is_valid(const void *ptr) { return ptr; }
  inline bool is_valid_or_null(const void *) { return true; }

  //--------------------------------------------------------------------------
  // Reason counter
  //--------------------------------------------------------------------------
  
  // In the SA, there are several counters that are incremented when a certain
  // situation occurs and decremented when it no longer occurs;
  // e.g. File::inclusion_count which is incremented when an #include is linked,
  // and decremented when it is no longer linked. When the counter changes
  // to/from zero, a callback is triggered to report a status change.
  //
  // One debugging challenge is to ensure that reasons can only be removed when
  // they have first been added.  Another debugging challenge is to figure out
  // why a counter remains non-zero when it is expected to return to zero, in
  // other words to list the reasons why it is non-zero.
  //
  // This auxiliary object helps to do exactly that by keeping track of the
  // reasons why a counter is incremented.
  //
  // Note that the same reason can occur more than once; the reason tracker
  // keeps track of all of them.
  //
  template <typename Reason> class ReasonTracker
  {
  public:
    void insert_reason(Reason const &reason)
    {
      ++map[reason];
    }
    
    void remove_reason(Reason const &reason)
    {
      size_t &count = map[reason];
      assert_(count, reason);
      --count;
      if (!count) map.erase(reason);
    }

    //for (auto const &[reason, count]: map) { ... }
    std::map<Reason, size_t> map;
  };

  template <typename Reason> 
  std::ostream operator<<(std::ostream &out,
    const ReasonTracker<Reason> &tracker
  )
  {
    for (auto const &[reason, count]: tracker.map) {
      out << count << "x " << reason << "\n";
    }
    return out;
  }
}

#endif
