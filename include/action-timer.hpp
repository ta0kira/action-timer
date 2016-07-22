/* -----------------------------------------------------------------------------
Copyright (c) 2016, Google Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
----------------------------------------------------------------------------- */

// Author: Kevin P. Barry [ta0kira@gmail.com] [kevinbarry@google.com]

#ifndef action_timer_hpp
#define action_timer_hpp

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <utility>

#include <time.h>

#include "locking-container.hpp"

#include "category-tree.hpp"


struct sleep_timer {
  virtual void mark() = 0;
  virtual void sleep_for(double time, std::function <bool()> cancel = nullptr) = 0;
  virtual ~sleep_timer() = default;
};

// The antithesis of thread-safe!
class precise_timer : public sleep_timer {
public:
  // cancel_granularity dictates how often the cancel callback passed to
  // sleep_for will be called to check for cancelation. In general, you should
  // not count on cancelation being ultra-fast; it's primarily intended for use
  // with stopping threads in action_timer.
  // min_sleep_size sets a lower limit on what sleep length will be handled with
  // an actual sleep call. Below that limit, a spinlock will be used. Set this
  // value to something other than zero if you need precise timing for sleeps
  // that are shorter than your kernel's latency. (This is mostly a matter of
  // experimentation.) Higher values will cause the timer to consume more CPU;
  // therefore, set min_sleep_size to the lowest value possible. 0.0001 is a
  // good place to start.
  // min_sleep_size should be much smaller than cancel_granularity. If it isn't,
  // however, sleeps will occur in chunks of cancel_granularity size until the
  // remainder is smaller than the smaller of the two.
  explicit precise_timer(double cancel_granularity = 0.01,
                         double min_sleep_size = 0.0);

  void mark() override;
  void sleep_for(double time, std::function <bool()> cancel = nullptr) override;

private:
  void spinlock_finish() const;

  const std::chrono::duration <double> sleep_granularity, spinlock_limit;
  std::chrono::duration <double> base_time;
};

class abstract_action {
public:
  virtual void start() = 0;
  virtual bool trigger_action() = 0;
  virtual ~abstract_action() = default;
};

// Thread-safe, except for start.
class async_action : public abstract_action {
public:
  // NOTE: A callback is used rather than a virtual function to avoid a race
  // condition when destructing while trying to execute the action.

  explicit async_action(std::function <bool()> new_action = nullptr) :
  action(std::move(new_action)) {}

  void start() override;
  bool trigger_action() override;

  // NOTE: This waits for the thread to reach an exit point, which could result
  // in waiting for the current action to finish executing. The consequences
  // should be no worse than the action being executed. For this reason, the
  // action shouldn't block forever for an reason.
  ~async_action() override;

private:
  void thread_loop();

  std::atomic <bool> destructor_called, action_error;
  std::unique_ptr <std::thread> thread;

  bool action_waiting;
  const std::function <bool()> action;

  std::mutex               action_lock;
  std::condition_variable  action_wait;
};

// Thread-safe.
class sync_action : public abstract_action {
public:
  // NOTE: new_action must be thread-safe if this is used with an action_timer
  // that has more than one thread!
  explicit sync_action(std::function <bool()> new_action = nullptr) :
  action(std::move(new_action)) {}

  void start() override {}
  bool trigger_action() override;

  // NOTE: This waits for an ongoing action to complete.
  ~sync_action() override;

private:
  typedef lc::locking_container <const std::function <bool()>, lc::r_lock> locked_action;

  locked_action action;
};

template <class Category>
class action_timer {
public:
  typedef std::unique_ptr <abstract_action> generic_action;

  // The number of threads is primarily intended for making timing more accurate
  // when high lambda values are used. When n threads are used, all sleeps are
  // multiplied by n, which decreases the ratio of overhead to actual sleeping
  // time, which allows shorter sleeps to be more accurate.
  explicit action_timer(unsigned int threads = 1, int seed = time(nullptr)) :
  thread_count(threads), stop_called(true), stopped(true), generator(seed), locked_scale(1.0) {}

  explicit action_timer(unsigned int threads, std::function <sleep_timer*()> factory,
                        int seed = time(nullptr)) :
  thread_count(threads), timer_factory(std::move(factory)), stop_called(true),
  stopped(true), generator(seed), locked_scale(1.0) {}

  // NOTE: It's an error to call this when threads are running.
  void set_timer_factory(std::function <sleep_timer*()> factory);

  void set_scale(double scale);

  void set_category(const Category &category, double lambda);

  // Ideally, async_action (or similar) should be used so that the amount of
  // time spent on the action by the timer thread is extremely small, with the
  // actual execution of the action happening in a dedicated thread.
  void set_action(const Category &category, generic_action action);

  // Start the timer threads. It's an error to call this when the threads are
  // already running.
  void start();

  // Stop all threads, and wait for them to all exit.
  // NOTE: It's an error to call this from a thread that's owned by this timer,
  // e.g., calling this from sync_action will cause a crash; use async_stop
  // instead.
  void stop();
  // All threads are stopped.
  bool is_stopped() const;
  // Block until is_stopped is true. This is equivalent to join, except
  // afterward the timer can be started again with start, and this function can
  // safely be called multiple times.
  void wait_stopped();

  // Stop all threads, but don't wait. Use this if you want to stop the threads
  // from a sync_action that's owned by this timer.
  // NOTE: The threads aren't actually cleaned up until stop is called.
  void async_stop();
  // Threads should be stopping, but might not all be stopped.
  bool is_stopping() const;
  // Block until is_stopping is true.
  void wait_stopping();

  // NOTE: This is non-deterministic, since it waits for the threads to reach an
  // exit point, e.g., after the ongoing sleep. Sleeps are subdivided to allow
  // for finer-grained cancelation, however. (See precise_timer.)
  ~action_timer();

private:
  void join();

  void thread_loop(unsigned int thread_number);

  typedef lc::locking_container <category_tree <Category, double>, lc::rw_lock>
    locked_category_tree;

  // NOTE: Category is already required to be sortable by category_tree. Since
  // the log(n) price is already being paid, this is a map so that we don't have
  // to also impose hashability on Category.
  typedef std::map <Category, generic_action> action_map;
  typedef lc::locking_container <action_map, lc::rw_lock> locked_action_map;

  // NOTE: All members besides threads and timer_factory need to be thread-safe!

  const unsigned int thread_count;
  std::list <std::unique_ptr <std::thread>> threads;
  std::function <sleep_timer*()> timer_factory;

  std::mutex              state_lock;
  std::condition_variable state_wait;
  std::atomic <bool> stop_called, stopped;

  std::default_random_engine generator;
  std::uniform_real_distribution <double> uniform;
  std::exponential_distribution <double>  exponential;

  lc::locking_container <double, lc::rw_lock> locked_scale;

  locked_category_tree locked_categories;
  locked_action_map    locked_actions;
};


template <class Category>
void action_timer <Category> ::set_timer_factory(std::function <sleep_timer*()> factory) {
  assert(this->is_stopped());
  timer_factory.swap(factory);
}

template <class Category>
void action_timer <Category> ::set_scale(double scale) {
  auto scale_write = locked_scale.get_write();
  assert(scale_write);
  *scale_write = scale;
}

template <class Category>
void action_timer <Category> ::set_category(const Category &category, double lambda) {
  lc::lock_auth_base::auth_type auth(new lc::lock_auth <lc::w_lock>);
  auto category_write = locked_categories.get_write_auth(auth);
  assert(category_write);
  if (lambda > 0) {
    category_write->update_category(category, lambda);
    category_write.clear();
    // Manually perform the check that get_write_auth would perform if locking
    // state_lock was done by locking-container.
    assert(auth->guess_write_allowed(true, true));
    std::unique_lock <std::mutex> local_lock(state_lock);
    state_wait.notify_all();
  } else {
    category_write->erase_category(category);
  }
}

template <class Category>
void action_timer <Category> ::set_action(const Category &category, generic_action action) {
  if (action) {
    action->start();
  }
  auto action_write = locked_actions.get_write();
  assert(action_write);
  if (action) {
    // NOTE: swap is used here so that destruction is called after
    // locked_actions is unlocked, in case the destructor is non-trivial.
    (*action_write)[category].swap(action);
  } else {
    action_write->erase(category);
  }
}

template <class Category>
void action_timer <Category> ::start() {
  assert(this->is_stopped());
  stopped = stop_called = false;
  if (threads.empty()) {
    for (unsigned int i = 0; i < thread_count; ++i) {
      threads.emplace_back(new std::thread([this,i] { this->thread_loop(i); }));
    }
  }
}

template <class Category>
void action_timer <Category> ::stop() {
  this->async_stop();
  this->join();
}

template <class Category>
bool action_timer <Category> ::is_stopped() const {
  return stopped;
}

template <class Category>
void action_timer <Category> ::wait_stopped() {
  while (!this->is_stopped()) {
    std::unique_lock <std::mutex> local_lock(state_lock);
    state_wait.wait(local_lock);
  }
}

template <class Category>
void action_timer <Category> ::async_stop() {
  {
    std::unique_lock <std::mutex> local_lock(state_lock);
    stop_called = true;
    state_wait.notify_all();
  }
}

template <class Category>
bool action_timer <Category> ::is_stopping() const {
  return stop_called;
}

template <class Category>
void action_timer <Category> ::wait_stopping() {
  while (!this->is_stopping()) {
    std::unique_lock <std::mutex> local_lock(state_lock);
    state_wait.wait(local_lock);
  }
}

template <class Category>
action_timer <Category> ::~action_timer() {
  this->stop();
}

template <class Category>
void action_timer <Category> ::join() {
  while (!threads.empty()) {
    assert(threads.front());
    assert(std::this_thread::get_id() != threads.front()->get_id());
    threads.front()->join();
    threads.pop_front();
  }
  stopped = true;
}

template <class Category>
void action_timer <Category> ::thread_loop(unsigned int thread_number) {
  lc::lock_auth_base::auth_type auth(new lc::lock_auth <lc::rw_lock>);
  // NOTE: This *must* be unique to this thread!
  std::unique_ptr <sleep_timer> timer(timer_factory? timer_factory() : new precise_timer);

  while (!stop_called) {
    auto scale_read = locked_scale.get_read_auth(auth);
    assert(scale_read);

    const double category_uniform = uniform(generator);
    const double time_exponential = exponential(generator) / *scale_read;

    scale_read.clear();

    // NOTE: Category selection comes before sleep, so that the sleep
    // corresponds to the categories available when it starts. This makes the
    // most sense semantically (vs. selecting after the sleep), if we say that
    // any change takes effect only after the sleep. It's possible, however, for
    // the action corresponding to the category to change/disappear.

    auto category_read = locked_categories.get_read_auth(auth);
    assert(category_read);

    if (category_read->get_total_size() == 0.0) {
      // NOTE: Failing to clear category_read will cause a deadlock!
      category_read.clear();
      // Manually perform the check that get_write_auth would perform if locking
      // state_lock was done by locking-container.
      assert(auth->guess_write_allowed(true, true));
      std::unique_lock <std::mutex> local_lock(state_lock);
      if (stop_called) {
        break;
      }
      state_wait.wait(local_lock);
      // Reset the timer so that the timer doesn't correct for the waiting time.
      timer->mark();
      continue;
    }

    // NOTE: Need to copy category to avoid a race condition!
    const Category   category = category_read->locate(category_uniform * category_read->get_total_size());
    const double time     = time_exponential / category_read->get_total_size() * (double) thread_count;
    category_read.clear();
    assert(!category_read);

    timer->sleep_for(time, [this] { return (bool) stop_called; });
    if (stop_called) {
      break;
    }

    auto action_read = locked_actions.get_read_auth(auth);
    assert(action_read);
    auto existing = action_read->find(category);
    bool remove = false;
    if (existing != action_read->end() && existing->second) {
      remove = !existing->second->trigger_action();
    }
    if (remove) {
      action_read.clear();
      assert(!action_read);
      this->set_category(category, 0.0);
      this->set_action(category, nullptr);
    }
  }
}

#endif //action_timer_hpp
