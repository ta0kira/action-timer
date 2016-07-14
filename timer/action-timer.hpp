#ifndef action_timer_hpp
#define action_timer_hpp

#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>

#include <time.h>

#include "locking-container.hpp"

#include "category-tree.hpp"


// The antithesis of thread-safe!
class precise_timer {
public:
  precise_timer(double granularity = 0.01);

  void mark();
  void sleep_for(double time, std::function <bool()> cancel = nullptr);


private:
  // The cancel callback is checked at this granularity during sleep_for, i.e.,
  // this is approximately the max latency for cancelation, with the expectation
  // being about half of this.
  const std::chrono::duration <double> sleep_granularity;
  std::chrono::duration <double> base_time;
};

class abstract_action {
public:
  virtual void start() = 0;
  virtual void trigger_action() = 0;
  virtual ~abstract_action() = default;
};

// Thread-safe, except for start.
class thread_action : public abstract_action {
public:
  // NOTE: A callback is used rather than a virtual function to avoid a race
  // condition when destructing while trying to execute the action.

  thread_action(std::function <void()> new_action = nullptr) :
  destructor_called(), action_waiting(), action(new_action) {}

  void set_action(std::function <void()> new_action);
  void start();
  void trigger_action();

  // NOTE: This waits for the thread to reach an exit point, which could result
  // in waiting for the current action to finish executing. The consequences
  // should be no worse than the action being executed. For this reason, the
  // action shouldn't block forever for an reason.
  ~thread_action();

private:
  void thread_loop();

  std::atomic <bool> destructor_called;
  std::unique_ptr <std::thread> thread;

  bool action_waiting;
  std::function <void()> action;

  std::mutex               action_lock;
  std::condition_variable  action_wait;
};

template <class Type>
class action_timer {
public:
  typedef std::unique_ptr <abstract_action> generic_action;

  // The number of threads is primarily intended for making timing more accurate
  // when high lambda values are used. When n threads are used, all sleeps are
  // multiplied by n, which decreases the ratio of overhead to actual sleeping
  // time, which allows shorter sleeps to be more accurate.
  explicit action_timer(unsigned int threads = 1, int seed = time(nullptr)) :
  destructor_called(false), generator(seed), thread_count(threads) {}

  void set_category(const Type &category, double lambda);

  // Ideally, thread_action (or similar) should be used so that the amount of
  // time spent on the action by the timer thread is extremely small, with the
  // actual execution of the action happening in a dedicated thread.
  void set_action(const Type &category, generic_action action);

  void start();
  void join();

  // NOTE: This is non-deterministic, since it waits for the threads to reach an
  // exit point, e.g., after the ongoing sleep. Sleeps are subdivided to allow
  // for finer-grained cancelation, however. (See precise_timer.)
  ~action_timer();

private:
  void thread_loop(unsigned int thread_number);

  typedef lc::locking_container <category_tree <Type, double>, lc::rw_lock>
    locked_category_tree;

  typedef std::unordered_map <Type, generic_action> action_map;
  typedef lc::locking_container <action_map, lc::w_lock> locked_action_map;

  // NOTE: All members besides threads need to be thread-safe!

  std::atomic <bool> destructor_called;
  std::list <std::unique_ptr <std::thread>> threads;

  std::default_random_engine generator;
  std::uniform_real_distribution <double> uniform;
  std::exponential_distribution <double>  exponential;

  std::mutex               empty_lock;
  std::condition_variable  empty_wait;

  const unsigned int thread_count;
  locked_category_tree locked_categories;
  locked_action_map    locked_actions;
};


template <class Type>
void action_timer <Type> ::set_category(const Type &category, double lambda) {
  auto category_write = locked_categories.get_write();
  assert(category_write);
  if (lambda > 0) {
    category_write->update_category(category, lambda);
    category_write.clear();
    std::unique_lock <std::mutex> local_lock(empty_lock);
    empty_wait.notify_all();
  } else {
    category_write->erase_category(category);
  }
}

template <class Type>
void action_timer <Type> ::set_action(const Type &category, generic_action action) {
  if (action) {
    action->start();
  }
  auto action_write = locked_actions.get_write();
  assert(action_write);
  if (action) {
    (*action_write)[category].reset(action.release());
  } else {
    action_write->erase(category);
  }
}

template <class Type>
void action_timer <Type> ::start() {
  if (threads.empty()) {
    for (unsigned int i = 0; i < thread_count; ++i) {
      threads.emplace_back(new std::thread([this,i] { this->thread_loop(i); }));
    }
  }
}

template <class Type>
void action_timer <Type> ::join() {
  for (auto &thread : threads) {
    assert(thread);
    thread->join();
  }
}

template <class Type>
action_timer <Type> ::~action_timer() {
  {
    std::unique_lock <std::mutex> local_lock(empty_lock);
    destructor_called = true;
    empty_wait.notify_all();
  }
  for (auto &thread : threads) {
    assert(thread);
    thread->join();
  }
}

template <class Type>
void action_timer <Type> ::thread_loop(unsigned int thread_number) {
  lc::lock_auth_base::auth_type auth(new lc::lock_auth <lc::rw_lock>);
  precise_timer timer;

  while (!destructor_called) {
    const double category_uniform = uniform(generator);
    const double time_exponential = exponential(generator);

    // NOTE: Category selection comes before sleep, so that the sleep
    // corresponds to the categories available when it starts. This makes the
    // most sense semantically (vs. selecting after the sleep), if we say that
    // any change takes effect only after the sleep. It's possible, however, for
    // the action corresponding to the category to change/disappear.

    auto category_read = locked_categories.get_read_auth(auth);
    assert(category_read);

    if (category_read->get_total_size() == 0) {
      // NOTE: Failing to clear category_read will cause a deadlock!
      category_read.clear();
      assert(!category_read);
      std::unique_lock <std::mutex> local_lock(empty_lock);
      if (destructor_called) {
        break;
      }
      empty_wait.wait(local_lock);
      continue;
    }

    // NOTE: Need to copy category to avoid a race condition!
    const Type   category = category_read->locate(category_uniform * category_read->get_total_size());
    const double time     = time_exponential / category_read->get_total_size() * (double) thread_count;
    category_read.clear();
    assert(!category_read);

    timer.sleep_for(time, [this] { return (bool) destructor_called; });
    if (destructor_called) {
      break;
    }

    auto action_write = locked_actions.get_write_auth(auth);
    assert(action_write);
    auto existing = action_write->find(category);
    if (existing != action_write->end() && existing->second) {
      existing->second->trigger_action();
    }
  }
}

#endif //action_timer_hpp
