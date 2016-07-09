/*
 * Suggested compilation command:
 *   c++ -Wall -pedantic -std=c++11 -O2 -I../include exponential.cpp -o exponential -pthread
 */

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <thread>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "locking-container.hpp"
//(necessary for non-template source)
#include "locking-container.inc"

namespace {

template <class Type>
struct exponential_categorical {
public:
  exponential_categorical() : lambda_total() {}

  bool set_category(const Type &value, double lambda) {
    assert(lambda >= 0);
    const bool existed = categories.find(value) != categories.end();
    categories.emplace(value, lambda);
    lambda_total += lambda;
    return existed;
  }

  bool clear_category(const Type &value) {
    auto existing = categories.find(value);
    if (existing == categories.end()) {
      return false;
    } else {
      lambda_total = std::max(0.0, lambda_total - existing->first);
      categories.erase(existing);
      return true;
    }
  }

  const Type &uniform_to_category(double uniform) const {
    assert(!this->empty());
    double target = uniform * lambda_total;
    for (const auto &category : categories) {
      if (target <= category.second) {
        return category.first;
      } else {
        target -= category.second;
      }
    }
    // This should only happen if uniform is extremely close to 1.0.
    return categories.begin()->first;
  }

  double uniform_to_time(double uniform) const {
    assert(!this->empty());
    return -log(uniform) / (double) lambda_total;
  }

  double empty() const {
    return categories.empty() || lambda_total == 0;
  }

private:
  double lambda_total;
  std::unordered_map <Type, double> categories;
};

class precise_timer {
public:
  precise_timer() : base_time() {
    this->mark();
  }

  void mark() {
    base_time = std::chrono::duration_cast <std::chrono::duration <double>> (
      std::chrono::high_resolution_clock::now().time_since_epoch());
  }

  void sleep_for(double time) {
    std::chrono::duration <double> target_duration(time);
    auto current_time = std::chrono::duration_cast <std::chrono::duration <double>> (
      std::chrono::high_resolution_clock::now().time_since_epoch());
    auto sleep_duration = target_duration - (current_time - base_time);
    // NOTE: If time is short enough then this will fall behind the actual time
    // until there is a sleep long enough to catch up.
    base_time += target_duration;
    std::this_thread::sleep_for(sleep_duration);
  }

private:
  std::chrono::duration <double> base_time;
};

class thread_action {
public:
  virtual void action() = 0;

  void start() {
    thread.reset(new std::thread([=] { this->thread_loop(); }));
  }

  void trigger_action() {
    action_wait.notify_all();
  }

private:
  void thread_loop() {
    while (true) {
      std::unique_lock <std::mutex> local_lock(action_lock);
      action_wait.wait(local_lock);
      this->action();
    }
  }

  std::unique_ptr <std::thread> thread;
  std::mutex               action_lock;
  std::condition_variable  action_wait;
};

class print_action : public thread_action {
public:
  print_action(const std::string &new_output) : output(new_output) {}

  void action() override {
    std::cout << output;
    std::cout.flush();
  }

private:
  const std::string output;
};

}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "%s [lamda:category...]\n", argv[0]);
    return 1;
  }

  std::default_random_engine generator(time(nullptr));
  std::uniform_real_distribution <double> uniform;

  typedef lc::locking_container <exponential_categorical <std::string>, lc::dumb_lock> locked_exponential_categorical;
  locked_exponential_categorical locked_categories;

  typedef std::unique_ptr<thread_action> generic_action;
  typedef std::unordered_map <std::string, generic_action> action_map;
  typedef lc::locking_container <action_map, lc::dumb_lock> locked_action_map;
  locked_action_map locked_actions;

  precise_timer timer;

  for (int i = 1; i < argc; ++i)
  {
    char buffer[256];
    memset(buffer, 0, sizeof buffer);
    double lambda = 0.0;
    if (sscanf(argv[i], "%lf:%256[\001-~]", &lambda, buffer) < 1) {
      fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], argv[i]);
      return 1;
    }
    {
      locked_exponential_categorical::write_proxy category_write = locked_categories.get_write();
      assert(category_write);
      category_write->set_category(buffer, lambda);
    }
    generic_action action(new print_action(buffer));
    action->start();
    {
      locked_action_map::write_proxy action_write = locked_actions.get_write();
      assert(action_write);
      (*action_write)[buffer].reset(action.release());
    }
  }

  while (true) {
    locked_exponential_categorical::read_proxy category_read = locked_categories.get_read();
    assert(category_read);
    const std::string &category = category_read->uniform_to_category(uniform(generator));
    const double       time     = category_read->uniform_to_time(uniform(generator));
    category_read.clear();

    timer.sleep_for(time);

    locked_action_map::write_proxy action_write = locked_actions.get_write();
    assert(action_write);
    (*action_write)[category]->trigger_action();
    action_write.clear();
  }
}
