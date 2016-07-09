#ifndef action_timer_hpp
#define action_timer_hpp

#include <memory>
#include <random>
#include <string>
#include <thread>

#include <time.h>

#include "locking-container.hpp"
#include "exponential-timer.hpp"

class action_timer {
public:
  action_timer(int seed = time(nullptr)) : generator(seed) {}

  typedef std::unique_ptr <abstract_action> generic_action;

  void set_category(const std::string &category, double lambda);
  void set_action(const std::string &category, generic_action action);

  void start();
  void join();

  ~action_timer();

private:
  void thread_loop();

  std::default_random_engine generator;
  std::uniform_real_distribution <double> uniform;

  typedef lc::locking_container <exponential_categorical <std::string>, lc::dumb_lock> locked_exponential_categorical;

  typedef std::unordered_map <std::string, generic_action> action_map;
  typedef lc::locking_container <action_map, lc::dumb_lock> locked_action_map;

  std::unique_ptr <std::thread> thread;
  locked_exponential_categorical locked_categories;
  locked_action_map              locked_actions;
  precise_timer                  timer;

};

#endif //action_timer_hpp
