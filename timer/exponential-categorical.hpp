#ifndef exponential_categorical_hpp
#define exponential_categorical_hpp

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <thread>

#include "category-tree.hpp"

// Not thread-safe!
template <class Type>
struct exponential_categorical {
public:
  bool set_category(const Type &category, double lambda);
  bool clear_category(const Type &category);

  const Type &uniform_to_category(double uniform) const;
  double uniform_to_time(double uniform) const;
  bool empty() const;

private:
  category_tree <Type, double> categories;
};


template <class Type>
bool exponential_categorical <Type> ::set_category(const Type &category, double lambda) {
  assert(lambda >= 0);
  const bool existed = categories.category_exists(category);
  categories.update_or_add(category, lambda);
  return existed;
}

template <class Type>
bool exponential_categorical <Type> ::clear_category(const Type &category) {
  const bool existed = categories.category_exists(category);
  if (existed) {
    categories.erase(category);
  }
  return existed;
}

template <class Type>
const Type &exponential_categorical <Type> ::uniform_to_category(double uniform) const {
  return categories.locate(uniform * categories.get_total_size());
}

template <class Type>
double exponential_categorical <Type> ::uniform_to_time(double uniform) const {
  return -log(uniform) / categories.get_total_size();
}

template <class Type>
bool exponential_categorical <Type> ::empty() const {
  return categories.get_total_size() == 0.0;
}

#endif //exponential_categorical_hpp
