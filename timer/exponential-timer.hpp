#ifndef exponential_timer_hpp
#define exponential_timer_hpp

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <thread>


template <class Type>
struct exponential_categorical {
public:
  exponential_categorical() : lambda_total() {}

  bool set_category(const Type &category, double lambda);
  bool clear_category(const Type &category);
  const Type &uniform_to_category(double uniform) const;
  double uniform_to_time(double uniform) const;
  bool empty() const;

private:
  double lambda_total;
  std::unordered_map <Type, double> categories;
};

class precise_timer {
public:
  precise_timer();

  void mark();
  void sleep_for(double time);

private:
  std::chrono::duration <double> base_time;
};

class thread_action {
public:
  virtual void action() = 0;

  virtual void start();
  virtual void trigger_action();

  virtual ~thread_action();

private:
  void thread_loop();

  std::unique_ptr <std::thread> thread;
  std::mutex               action_lock;
  std::condition_variable  action_wait;
};


template <class Type>
bool exponential_categorical <Type> ::set_category(const Type &category, double lambda) {
  assert(lambda >= 0);
  const bool existed = this->clear_category(category);
  categories.emplace(category, lambda);
  lambda_total += lambda;
  return existed;
}

template <class Type>
bool exponential_categorical <Type> ::clear_category(const Type &category) {
  auto existing = categories.find(category);
  if (existing == categories.end()) {
    return false;
  } else {
    lambda_total = std::max(0.0, lambda_total - existing->second);
    categories.erase(existing);
    return true;
  }
}

template <class Type>
const Type &exponential_categorical <Type> ::uniform_to_category(double uniform) const {
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

template <class Type>
double exponential_categorical <Type> ::uniform_to_time(double uniform) const {
  assert(!this->empty());
  return -log(uniform) / (double) lambda_total;
}

template <class Type>
bool exponential_categorical <Type> ::empty() const {
  return categories.empty() || lambda_total == 0;
}

#endif //exponential_timer_hpp
