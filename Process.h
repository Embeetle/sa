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

#ifndef __Process_h
#define __Process_h

#include "Task.h"
#include "Lockable.h"
#include "base/RefCounted.h"
#include "base/debug.h"
#include <mutex>
#include <set>
#include <iostream>
#include <sstream>

namespace sa {

  // Processes are used to organize source analysis.  Each process represents one
  // step of source analysis, and implements the necessary computation for that
  // step in a background task.
  //
  // A process exists as long as the source analysis step it is part of the
  // project. It is triggered whenever one of its dependencies changes.
  //
  // When a process is triggered, a background task will run it (= execute its
  // virtual run function) at some point in time after the trigger. If the
  // process is already running when triggered, the running task is cancelled
  // and a new task is started once the running task returns. There will never
  // be more than one task running the same process.
  //
  // The run function called by the background task typically executes an
  // analyzer. The analyzer produces analysis results and inserts them in a data
  // structure that represents the current analysis state. Since this is done in
  // a background thread, a mutex is usually needed to access the shared
  // analysis state.
  //
  // The process itself is lockable, and it cannot be triggered or cancelled
  // while locked. In general, all methods that lock the process will block
  // until the process is unlocked, and calling them from a thread that already
  // locks the process causes a deadlock. The process lock can also be used to
  // protect access to shared data structures if desired.
  //
  // To avoid unnecessary computations while a dependency is not up-to-date,
  // each process has a block counter. The block counter can be incremented and
  // decremented.  When the block counter is non-zero, the process will not run
  // when triggered; running it is postponed until the block count decrements to
  // zero again.  When it is incremented from zero, any running task is
  // cancelled.
  //
  // The block counter is typically incremented and decremented from
  // dependencies of the process, i.e. processes whose resuls this process
  // depends on.
  //
  class Process: public Lockable, public base::RefCounted
  {
  public:
    // Create a process.  
    Process(std::string const &name);

    // Destroy a process.
    ~Process();

    // Process name
    std::string const &process_name() const { return name; }

    // A process can be urgent or non-urgent. An urgent process executes asap,
    // and a non-urgent process executes alap. By default, a process is
    // non-urgent.  The version without underscore temporarily locks the
    // process. The version with underscore requires that the process is locked.
    void set_urgent(bool urgent);
    void _set_urgent(bool urgent);

    // Trigger this process.  It will run, unless it is blocked. If the process
    // was already running, it is cancelled and restarted, to make sure that the
    // state at the most recent trigger is taken into account. After triggering,
    // the run method is guaranteed to run at least once without being
    // cancelled.  The version without underscore temporarily locks the
    // process. The version with underscore requires that the process is locked.
    void trigger();
    void _trigger();

    // Cancel running process. Only call while process is locked. The version
    // without underscore temporarily locks the process. The version with
    // underscore requires that the process is locked.
    void cancel();
    void _cancel();

    // Increment the block count. If a task is running, incrementing from zero
    // will cancel it and retrigger the process.  The version without underscore
    // temporarily locks the process. The version with underscore requires that
    // the process is locked.
    void block();
    void _block();

    // Decrement the block count.  Decrementing to zero will start a task if the
    // process was triggered while blocked.  The version without underscore
    // temporarily locks the process. The version with underscore requires that
    // the process is locked.
    void unblock();
    void _unblock();
  
    // Method to be overridden to define process behaviour. This method will run
    // in a background task. It is not intended to be called directly. It
    // doesn't need to be reentrant; a new call will only be issued after the
    // previous call returned, even if the previous call was cancelled. The
    // process is not locked when this method is called, so that other threads
    // can block, unblock or trigger it.
    virtual void run() {}

    // Check if the current run has been cancelled, due to a new trigger or
    // because the process became blocked.
    //
    // This is a fast method with a lock-free implementation using atomics.  It
    // can only be called from the run method, and can be called often to avoid
    // doing unnecessary work. Do not call it from outside the run method!
    //
    // A cancelled process should never report analysis results.  Make sure to
    // lock the process before checking whether the process is cancelled, and
    // keep it locked until results are reported. A process cannot be cancelled
    // (by triggering or blocking it) while it is locked.
    bool cancelled() const;

    // Process statuses.
    // 
    // Note that "cancelled" is not a status.  A waiting or running process is
    // cancelled either when it becomes blocked or when it is triggered again.
    // The new status will be "blocked" or "waiting", respectively.
    enum Status {
      Ready,      // not blocked,  not triggered;  initial status.
      Blocked,    // at least one dependency is blocked or triggered
      Waiting,    // triggered,  will run when a worker thread is available
      Running,    // triggered and running in a worker thread
    };

    static const char *status_name(Status status)
    {
      static const char *names[] = {
        "ready", "blocked", "waiting", "running"
      };
      return names[status];
    }

    // Report changes in process status.  Initial status is ready. The process
    // is locked during this call.
    virtual void on_status(Status status) { (void)status; }

    // Check if this process is up-to-date, i.e. has not been triggered since it
    // was created or after completing a run that was not cancelled.  An
    // up-to-date process will not run again until triggered, even if it is
    // blocked and unblocked. A process that is not up-to-date is out-of-date. A
    // process becomes out-of-date when triggered.
    //
    // The process must be locked while calling the methods starting with
    // underscore. The methods without underscore will temporarily lock the
    // proces.
    bool is_up_to_date() const { return !is_out_of_date(); }
    bool _is_up_to_date() const { return !_is_out_of_date(); }
    bool is_out_of_date() const;
    bool _is_out_of_date() const;

    bool _is_blocked() const;
    bool is_blocked() const;
    size_t _get_block_count() const;
    size_t get_block_count() const;

    // Report that this process just became out-of-date. Called from the thread
    // that triggered the process. The process is locked during this call. While
    // the process is out-of-date, the run method can be called at any time from
    // a background thread and the process will not be deleted.
    virtual void on_out_of_date() {}

    // Report that this process just became up-to-date. Called from the
    // background thread. The process is locked during this call. While the
    // process is up-to-date, the run method will not be called and the process
    // can be deleted.
    //
    // Calls to on_out_of_date and on_up_to_date will always alternate. A new
    // process is always considered up-to-date until triggered.
    virtual void on_up_to_date() {}

    // The grab method is called just before on_out_of_date, and the drop method
    // is called just after on_up_to_date, from the same threads. The only
    // difference is that the process is not locked while calling drop. This can
    // be useful if another lock (e.g. project lock) needs to be applied first.
    virtual void grab() {}
    virtual void drop() {}

  protected:
    std::string const name;
    
    friend class Runner;

    // Execute a task.  Not re-entrant.
    void execute();

    // Cleanup after a task, whether it finishes normally or gets cancelled
    // before or after it was started.
    void epilog();
  
    class Runner: public Task
    {
    public:
      Process *const process;
    
      Runner(Process *process);

      ~Runner() override;

      void execute() override;
    };

    // Start new task. Only call while process is locked.
    void start();

    // Set process status. Only call while process is locked.
    void set_status(Status status);

  private:
    bool urgent = false;
    Runner *runner = 0;
    unsigned block_count = 0;
    bool triggered = false;
    Status status = Ready;
  };
  
  inline std::ostream &operator<<(std::ostream &out, Process::Status status)
  {
    return out << Process::status_name(status);
  }

  inline std::ostream &operator<<(std::ostream &out, const Process &process)
  {
    return out << process.process_name();
  }
}

#endif
