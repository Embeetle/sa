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

#ifndef __Task_h
#define __Task_h

#include "base/debug.h"
#include <atomic>

// A task executes code - defined by overloading the execute() method in a
// derived class - in a background worker thread, while the foreground thread
// that starts it is free to perform other work.
//
// Tasks are executed by a finite number of worker threads. If there are more
// tasks than worker threads, new tasks are placed in a queue until a worker
// thread becomes available.  There is an option to schedule tasks either ASAP -
// at the head of the queue - or ALAP - at the tail of the queue.
//
// A task can be cancelled at any point while on the queue or while executing.
// If not cancelled, it will run to completion, report its results and
// self-destroy.
// 
// There is at least one foreground thread and zero or more worker threads
// involved in task execution. A foreground thread can start and cancel tasks
// and observe their results.
//
// Task results are reported directly to the foreground task when the background
// task is ready. The foreground task does not need to wait for the results.
// Since task results are shared between a foreground thread (that wants to
// observe the results and may try to cancel the task) and a worker thread (that
// will report results). They must therefore be protected by a mutex.
//
// Before reporting results, a task must be sure that it has not been cancelled.
// To do this, it must first lock the result mutex and then check the task
// state. It is allowed to also check the state earlier and abort if the task
// was cancelled, even without locking the mutex. It is however not safe to
// first check the state and then lock the mutex and unconditionally report
// results, because the state might change just before the mutex is locked.  In
// other words, a task will always need to do a final check for cancellation
// while the result mutex is locked.
//
// Checking whether a task has been cancelled is very fast - implementation is
// based on atomics.  This means that a task can check for cancellation very
// often without introducing much overhead.
// 
// If you cancel a task, you want to be sure that it will not report any results
// later on, so that you are free to examine the current results or start a new
// task storing results in the same location. This means that the result mutex
// must be locked while cancelling the task. Whether the task completes or gets
// cancelled is then unambiguous and will depend on who locks the mutex first:
// the worker thread executing the task or the user interface thread cancelling
// the task.
//
// After reporting its results, a task will self-destroy.  To avoid cancelling a
// self-destroyed task, the task results must include a way to check they have
// been reported.  If so, the task will soon self-destroy or has already
// self-destroyed and should not be cancelled anymore.
//
// If a task detects that is has been cancelled, it can choose to skip any
// remaining computations and immediately return from the execute() method. It
// should not delete itself.  Task deletion happens automatically when the
// execute() method returns.  The destructor will run.
//
// Here are some templates for correct usage.  We assume that at most one task
// should run at any time for a given result, which is the most common and
// simplest case. We set the task pointer to null to indicate that no task is
// running.
//
// Start a task:
//   result->lock();
//   if (task) task->cancel();
//   task = new Task(); // Instantiate a derived class instead!
//   task->schedule(hint);
//   result->unlock();
//   ...task can run at any time, do not delete result
// 
// Cancel a task:
//   result->lock();
//   if (task) task->cancel();
//   result->unlock();
//   ...task will not access result anymore, feel free to delete it
//
// Report results from a task:
//   result->lock();
//   if (!cancelled()) {
//     result->set(...);
//   }
//   result->unlock()
//   ...task auto-deletes without further access to result
// 
class Task: public base::Checked
{
public:
  // Create a task.
  //
  // Do not schedule or start the task yet. It is not safe to immediately
  // schedule the task and possibly start execution from this constructor,
  // because the derived class might not be completely constructed yet, causing
  // task execution to fail.
  Task();

  // Schedule this task. A scheduled task will start when a worker thread is
  // available; otherwise, it is temporarily queued. A task can be scheduled at
  // most once; this is asserted. An urgent task is started as soon as a worker
  // thread is available, before any already queued tasks (LIFO = Last In First
  // Out).  A non-urgent task is added at the end of the queue and is started
  // after all previously queued tasks (FIFO = First In First Out).
  void schedule(bool urgent = false);

  // Cancel this task. If this task is not running yet, it will never start. If
  // it is already running, this will set a flag to be checked by the execute()
  // function.
  //
  // To use this method safely, make sure the task notifies you before
  // terminating.  Otherwise, you might be cancelling a non-existing task.
  void cancel();

  // Return true iff this task has been cancelled.
  bool cancelled() const { return state == Cancelled; }

  // Set the number of workers. Initial number is one.
  static void set_number_of_workers(unsigned n);

  // Start background threads. No-op if already started.
  static void start();

  // Stop background threads. No-op if already stopped.  No tasks are
  // aborted. Whether running tasks are pauzed or run to completion is
  // unspecified.
  static void stop();

  // Abort all running tasks.  To be called before program termination.
  // Tasks cannot be resumed.
  static void abort();

  // Wait until all tasks are complete or cancelled.
  static void wait();

protected:
  // Execute this task.
  virtual void execute() {};

  // Destroy this task. Do not call directly: tasks will self-destroy after
  // completion or cancellation.
  virtual ~Task();

private:
  // Possible state values
  enum State { Created, Scheduled, Running, Cancelled };

  // Current state of this task
  std::atomic<State> state = Created;

  // If queued, the next task in the queue.
  Task *_next = 0;

  // Queue manipulation.
  void _insert_at_head();
  void _insert_at_tail();
  static Task *_pop();

  // Initialization. Call this at least once before any action that requires the
  // queue, mutexes and semaphores to be initialized. Additional calls are
  // ignored in a thread-safe way.
  static void initialize()
  {
    // Use initialization of static local variable to initialize exactly once.
    // C++ guarantees that this is thread-safe.
    static int initialized = Task::init_once();
    (void)initialized;
  }

  // Call this exactly once before any action that requires the queue, mutexes
  // and semaphores to be initialized. Not thread-safe. Return an int so that it
  // can be used in an initialization expression.
  static int init_once();

  // Worker thread function.
  static void *work(void *handle);

  // Add a new worker.
  static void add_worker();

  // Remove an existing worker. If all workers are busy, this will return
  // immediately, and the first works that completes will exit.
  static void remove_worker();
};

#endif
