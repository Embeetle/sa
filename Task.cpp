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

#include "Task.h"
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "base/debug.h"

static const char *state_name[] = {
  "created", "scheduled", "running", "cancelled"
};

// Define SA_NO_THREADS to execute tasks in the foreground thread calling
// start.  This can be useful for debugging because it makes task scheduling
// predictable.
static bool use_threads = !getenv("SA_NO_THREADS");
static const char *max_threads_string = getenv("SA_MAX_THREADS");
static unsigned max_threads = max_threads_string ? atol(max_threads_string) : 0;

// Singly linked list of waiting tasks, protected by a mutex. The extra pointer
// to the last task allows constant time insertion of non-urgent tasks.
static pthread_mutex_t mutex;
static Task *_next_task = 0;
static Task *_last_task = 0;

// Semaphore counting waiting tasks. Starts at zero. Scheduling a task posts
// to this semaphore. Worker threads wait for it before popping a task from the
// queue. Always zero when task execution is paused.
static sem_t task_semaphore;

// Semaphore counting threads to be killed. Threads will try to wait for this
// semaphore before popping a task from the queue. If waiting succeeds, the
// thread posts to the task semaphore and exits. Used to reduce the number of
// workers without locking. To reduce the number of workers by N, post N times
// to this semaphore.
static sem_t kill_semaphore;

// Semaphore that is non-zero when no threads are waiting or running.
static sem_t ready_semaphore;

// Number of waiting plus running tasks, plus access semaphore, for debugging
static unsigned task_count = 0;
static pthread_mutex_t task_count_mutex;

// Number of Worker threads.
static unsigned worker_count = 0;

// True iff running.
static bool running = false;

Task::Task()
{
  trace("task constructor " << this);
  initialize();
  pthread_mutex_lock(&task_count_mutex);
  if (!task_count) {
    sem_wait(&ready_semaphore);
  }
  ++task_count;
  pthread_mutex_unlock(&task_count_mutex);
}

Task::~Task()
{
  trace("task destructor " << this);
  assert(is_valid(this));
  pthread_mutex_lock(&task_count_mutex);
  --task_count;
  if (!task_count) {
    sem_post(&ready_semaphore);
  }
  trace("task destructor " << this << " done");
  pthread_mutex_unlock(&task_count_mutex);
}

void Task::schedule(bool urgent)
{
  trace("Task::schedule urgent=" << urgent << " " << this);
  if (!use_threads) debug_atomic_writeln("SA_NO_THREADS set");
  assert(is_valid(this));
  pthread_mutex_lock(&mutex);
  assert(state == Created);
  if (urgent) {
    _insert_at_head();
  } else {
    _insert_at_tail();
  }
  state = Scheduled;
  if (running) {
    sem_post(&task_semaphore);
  }
  pthread_mutex_unlock(&mutex);
}

void Task::cancel()
{
  trace("Task::cancel " << this);
  assert(is_valid(this));
  state = Cancelled;
}
  
void Task::_insert_at_head()
{
  assert(is_valid(this));
  _next = _next_task;
  _next_task = this;
  if (!_last_task) {
    _last_task = this;
  }
}

void Task::_insert_at_tail()
{
  assert(is_valid(this));
  if (_last_task) {
    _last_task->_next = this;
  } else {
    _next_task = this;
  }
  _last_task = this;
}

Task *Task::_pop()
{
  Task *task = _next_task;
  assert_(is_valid(task), task);
  _next_task = task->_next;
  if (task == _last_task) {
    _last_task = 0;
  }
  return task;
}

int Task::init_once()
{
  debug_writeln("Process ID: " << base::get_pid());
  sem_init(&task_semaphore, 0, 0);
  sem_init(&kill_semaphore, 0, 0);
  sem_init(&ready_semaphore, 0, 0);
  sem_post(&ready_semaphore);
  pthread_mutex_init(&mutex, 0);
  pthread_mutex_init(&task_count_mutex, 0);
  //
  // Add one worker to get started. Do not use set_number_of_workers(1) here
  // as that would deadlock.
  if (use_threads) add_worker();
  return 1;
}

void Task::set_number_of_workers(unsigned n)
{
  trace_nest("Task set number of workers to " << n);
  initialize();
  if (use_threads) {
    pthread_mutex_lock(&mutex);
    if (max_threads && n > max_threads) {
      debug_atomic_writeln(
        "SA_MAX_THREADS set: limit #workers " << n << " -> "
        << max_threads
      );
      n = max_threads;
    }
    while (worker_count > n) {
      remove_worker();
    }
    while (worker_count < n) {
      add_worker();
    }
    pthread_mutex_unlock(&mutex);
  } else {
    debug_atomic_writeln(
      "SA_NO_THREADS set: ignore set_number_of_workers=" << n
    );
  }
}
  
void Task::start()
{
  trace_nest("Task start");
  initialize();
  pthread_mutex_lock(&mutex);
  if (!running) {
    running = true;
    for (Task *task = _next_task; task; task = task->_next) {
      trace("Post for task " << task);
      sem_post(&task_semaphore);
    }
  }
  pthread_mutex_unlock(&mutex);
  if (!use_threads) work(0);
}

void Task::stop()
{
  trace_nest("Task stop");
  initialize();
  pthread_mutex_lock(&mutex);
  if (running) {
    running = false;
    for (Task *task = _next_task; task; task = task->_next) {
      int rc;
      // On Windows, sem_trywait sometimes keeps returning EAGAIN,
      // so limit the number of tries. Not clear why this happens.
      int tries = 100;
      do {
        rc = sem_trywait(&task_semaphore);
      } while (rc && errno == EAGAIN && --tries);
      assert_(rc == 0 || tries == 0, errno << " " << strerror(errno));
    }
  }
  pthread_mutex_unlock(&mutex);
}

void Task::abort()
{
  trace_nest("Task abort");
  stop();
  set_number_of_workers(0);
  pthread_mutex_lock(&mutex);
  for (Task *task = _next_task; task; task = task->_next) {
    task->cancel();
  }
  pthread_mutex_unlock(&mutex);
  //
  // Allow running tasks some time to stop cleanly.
}

void Task::wait()
{
  sem_wait(&ready_semaphore);
  sem_post(&ready_semaphore);
}

// Entry point for workers.
void *Task::work(void *handle)
{
  (void)handle;
  trace("Enter worker pid=" << base::get_pid()
    << " tid=" << base::get_tid() << " handle=" << handle
  );
  for (;;) {
    if (use_threads) {
      trace("Waiting for new task (" << task_count << " queued) ...");
      sem_wait(&task_semaphore);
      trace("Found task (" << task_count << " queued) ...");
      int kill_rc = sem_trywait(&kill_semaphore);
      if (!kill_rc) {
        trace("Stop worker");
        break;
      }
      assert(errno == EAGAIN);
    } else {
      trace("Task worker SA_NO_THREADS");
      int task_rc = sem_trywait(&task_semaphore);
      if (task_rc != 0 && errno == EAGAIN) {
        // No more tasks
        break;
      }
    }
    trace("Waiting for task list mutex ...");
    pthread_mutex_lock(&mutex);
    trace("Task pop");
    Task *task = _pop();
    trace(" `--> " << task << " " << state_name[task->state]);
    assert(is_valid(task));
    pthread_mutex_unlock(&mutex);
    //
    // Below, it is essential to use atomic compare_exchange_strong instead of a
    // test followed by an assignment.  Otherwise, if the task is cancelled
    // between the test and the assignment, the cancellation would be
    // overwritten and thus ignored.
    State state = Scheduled;
    if (task->state.compare_exchange_strong(state, Running)) {
      trace_nest("Execute task");
      assert(base::is_valid(task));
      task->execute();
    }
    trace("Task done");
    assert(base::is_valid(task));
    trace("Deleting task");
    delete task;
    trace("Task deleted");
  }
  trace("Leave worker");
  return 0;
}

void Task::add_worker()
{
  pthread_t thread;
  int rc = pthread_create(&thread, 0, work, (void*)1);
  if (rc != 0) {
    trace("cannot create worker thread for source analysis");
    return;
  }
  pthread_detach(thread);
  ++worker_count;
}

void Task::remove_worker()
{
  sem_post(&kill_semaphore);
  sem_post(&task_semaphore);
  --worker_count;
}
