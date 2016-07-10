#ifndef action_timer_hpp
#define action_timer_hpp

#include <list>
#include <memory>
#include <random>
#include <thread>

#include <time.h>

#include "locking-container.hpp"
#include "exponential-timer.hpp"

template <class Type>
class action_timer {
public:
  explicit action_timer(unsigned int threads = 1, int seed = time(nullptr)) :
  generator(seed), thread_count(threads) {}

  typedef std::unique_ptr <abstract_action> generic_action;

  void set_category(const Type &category, double lambda);
  void set_action(const Type &category, generic_action action);

  void start();
  void join();

  ~action_timer();

private:
  void thread_loop(unsigned int thread_number);

  typedef lc::locking_container <exponential_categorical <Type>, lc::rw_lock>
    locked_exponential_categorical;

  typedef std::unordered_map <Type, generic_action> action_map;
  typedef lc::locking_container <action_map, lc::w_lock> locked_action_map;

  // NOTE: All members besides threads need to be thread-safe!

  std::default_random_engine generator;
  std::uniform_real_distribution <double> uniform;

  const unsigned int thread_count;
  std::list <std::unique_ptr <std::thread>> threads;
  locked_exponential_categorical locked_categories;
  locked_action_map              locked_actions;
};


template <class Type>
void action_timer <Type> ::set_category(const Type &category, double lambda) {
  auto category_write = locked_categories.get_write();
  assert(category_write);
  if (lambda > 0) {
    category_write->set_category(category, lambda);
  } else {
    category_write->clear_category(category);
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
  threads.clear();
  for (unsigned int i = 0; i < thread_count; ++i) {
    threads.emplace_back(new std::thread([=] { this->thread_loop(i); }));
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
  for (auto &thread : threads) {
    assert(thread);
    thread->detach();
  }
}

template <class Type>
void action_timer <Type> ::thread_loop(unsigned int thread_number) {
  precise_timer timer;

  while (true) {
    const double category_uniform = uniform(generator);
    const double time_uniform     = uniform(generator);

    // NOTE: Category selection comes before sleep, so that the sleep
    // corresponds to the categories available when it starts. This makes the
    // most sense semantically (vs. selecting after the sleep), if we say that
    // any change takes effect only after the sleep. It's possible, however, for
    // the action corresponding to the category to change/disappear.

    auto category_read = locked_categories.get_read();
    assert(category_read);
    // NOTE: Need to copy category to avoid a race condition!
    const Type   category = category_read->uniform_to_category(category_uniform);
    const double time     = category_read->uniform_to_time(time_uniform) * (double) thread_count;
    category_read.clear();
    assert(!category_read);

    timer.sleep_for(time);

    auto action_write = locked_actions.get_write();
    assert(action_write);
    auto existing = action_write->find(category);
    if (existing != action_write->end() && existing->second) {
      if (thread_number) existing->second->trigger_action();
    }
  }
}

#endif //action_timer_hpp
