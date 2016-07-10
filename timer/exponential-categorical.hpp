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


// Not thread-safe!
template <class Type>
class category_indexer {
public:
  template <class Container>
  void regenerate(Container container);
  const Type &get_category(double uniform) const;
  double get_total() const;

private:
  int binary_search(double target, int index_low, int index_high) const;

  std::vector <std::pair<Type, double>> index;
};

// Not thread-safe!
template <class Type>
struct exponential_categorical {
public:
  // NOTE: Updates are linear time so that lookup can be logarithmic, since it's
  // assumed that updates will be comparatively rare. Updates also aren't done
  // lazily so that lookup can be done with const.

  bool set_category(const Type &category, double lambda);
  bool clear_category(const Type &category);

  const Type &uniform_to_category(double uniform) const;
  double uniform_to_time(double uniform) const;
  bool empty() const;

private:
  category_indexer <Type>           category_index;
  std::unordered_map <Type, double> categories;
};


template <class Type> template <class Container>
void category_indexer <Type> ::regenerate(Container container) {
  index.clear();
  index.reserve(container.size());
  double total = 0.0;
  for (const auto &item : container) {
    index.push_back(item);
    assert(index.rbegin()->second >= 0.0);
    total = index.rbegin()->second += total;
  }
}

template <class Type>
const Type &category_indexer <Type> ::get_category(double uniform) const {
  assert(index.size());
  assert(uniform >= 0.0 && uniform <= 1.0);
  const double target = uniform * index.rbegin()->second;
  const int location = this->binary_search(target, 0, (signed) index.size() - 1);
  assert(location >= 0 && location < (signed) index.size());
  return index[location].first;
}

template <class Type>
double category_indexer <Type> ::get_total() const {
  return index.empty()? 0.0 : index.rbegin()->second;
}

template <class Type>
int category_indexer <Type> ::binary_search(double target, int index_low, int index_high) const {
  assert(index_low <= index_high);
  if (index_low == index_high) {
    // Only one element left.
    return index_high;
  }
  if (target < index[index_low].second) {
    // Inside the interval ending at the low index, assuming the value at
    // low-1 is lower than the target.
    return index_low;
  }
  const int index_mid = (index_high + index_low) / 2;
  if (target < index[index_mid].second) {
    // Target is in the lower half.
    return this->binary_search(target, index_low, index_mid);
  } else {
    // Target is in the upper half. Note that the midpoint must be excluded
    // for logic above to work. Note that mid+1 <= high because the average of
    // low and high must be < high.
    return this->binary_search(target, index_mid + 1, index_high);
  }
}


template <class Type>
bool exponential_categorical <Type> ::set_category(const Type &category, double lambda) {
  assert(lambda >= 0);
  const bool existed = categories.find(category) != categories.end();
  categories.emplace(category, lambda);
  category_index.regenerate(categories);
  return existed;
}

template <class Type>
bool exponential_categorical <Type> ::clear_category(const Type &category) {
  auto existing = categories.find(category);
  if (existing == categories.end()) {
    return false;
  } else {
    categories.erase(existing);
    category_index.regenerate(categories);
    return true;
  }
}

template <class Type>
const Type &exponential_categorical <Type> ::uniform_to_category(double uniform) const {
  return category_index.get_category(uniform);
}

template <class Type>
double exponential_categorical <Type> ::uniform_to_time(double uniform) const {
  return -log(uniform) / category_index.get_total();
}

template <class Type>
bool exponential_categorical <Type> ::empty() const {
  return categories.empty() || category_index.get_total();
}

#endif //exponential_categorical_hpp
