#ifndef category_tree_hpp
#define category_tree_hpp

#include <cassert>
#include <cmath>
#include <memory>
#include <stack>

template <class Type, class Size>
class category_node;

template <class Type, class Size = double>
class category_tree {
public:
  using node_type = category_node <Type, Size>;

  bool category_exists(const Type &value) const {
    return head && head->category_exists(value);
  }

  const Type &locate(Size size) {
    assert(head && size >= Type() && size <= this->get_total_size());
    return head->locate(size);
  }

  bool update_or_add(const Type &value, Size new_size) {
    return node_type::optional_node::update_or_add(head, value, new_size);
  }

  bool erase(const Type &value) {
    return node_type::optional_node::erase(head, value);
  }

  Size get_total_size() const {
    return head? head->get_total_size() : Size();
  }

private:
  typename node_type::optional_node head;
};

template <class Type, class Size>
struct category {
  category(const Type &new_value, Size new_size) :
  value(new_value), size(new_size) {}

  const Type value;
  Size       size;
};

template <class Type, class Size>
class category_node {
public:
  using optional_node = std::unique_ptr <category_node>;

  category_node(const Type &new_value, Size new_size) :
  balance(0), total_size(), data(new_value, new_size) {}

  bool category_exists(const Type &value) const {
    if (data.value == value) return true;
    if (value < data.value) {
      return low_child?  low_child->category_exists(value)  : false;
    } else {
      return high_child? high_child->category_exists(value) : false;
    }
    return false;
  }

  const Type &locate(Size size) const {
    // (Hopefully this doesn't cause problems in recursive calls after doing
    // subtractions below.)
    assert(size >= Size() && size < total_size);
    // Interval is divided into three parts: low, self, high.
    if (low_child && size < low_child->total_size) {
      return low_child->locate(size);
    }
    if (low_child) size -= low_child->total_size;
    // Not in first part => move to second.
    if (size < data.size) {
      return data.value;
    }
    size -= data.size;
    // Not in second part => move to third.
    high_child->locate(size);
  }

  Size get_total_size() {
    return total_size;
  }

  // NOTE: current is a unique_ptr passed by reference because balancing
  // operations sometimes require the node being operated on to be swapped out.
  static int update_or_add(optional_node &current, const Type &new_value, Size new_size) {
    if (!current) {
      current.reset(new category_node(new_value, new_size));
      current->total_size = new_size;
      return 1;
    }
    int height_change = 0;
    if (current->data.value == new_value) {
      current->data.size = new_size;
    } else if (new_value < current->data.value) {
      current->balance -= update_or_add(current->low_child, new_value, new_size);
      if (current->balance == -1) {
        height_change = 1;
      }
      if (current->balance < -1) {
        height_change = pivot_high(current);
      }
    } else {
      current->balance += update_or_add(current->high_child, new_value, new_size);
      if (current->balance == 1) {
        height_change = 1;
      }
      if (current->balance > 1) {
        height_change = pivot_low(current);
      }
    }
    update_size(current.get());
    return height_change;
  }

  // NOTE: current is a unique_ptr passed by reference because balancing
  // operations sometimes require the node being operated on to be swapped out.
  static int erase(optional_node &current, const Type &new_value) {
    if (!current) {
      return 0;
    }
    int height_change = 0;
    optional_node discard;
    if (current->data.value == new_value) {
      return remove_node(current, discard);
    } else if (new_value < current->data.value) {
      if (!current->low_child) {
        return 0;
      } else {
        // Recusive call low.
        current->balance -= erase(current->low_child, new_value);
        if (current->balance < -1) {
          height_change = pivot_high(current);
        }
      }
    } else {
      if (!current->high_child) {
        return 0;
      } else {
        // Recusive call high.
        current->balance -= erase(current->high_child, new_value);
        if (current->balance < 1) {
          height_change = pivot_low(current);
        }
      }
    }
    update_size(current.get());
    return height_change;
  }

private:
#ifdef TESTING
  friend class category_node_test;
#endif

  static void update_size(category_node *current) {
    if (current) {
      current->total_size = current->data.size;
      if (current->low_child)  current->total_size += current->low_child->total_size;
      if (current->high_child) current->total_size += current->high_child->total_size;
    }
  }

  static int pivot_low(optional_node &current) {
    assert(current->high_child);
    // Make sure that high_child has non-negative balance.
    if (current->high_child->balance < 0) {
      // NOTE: Assuming low_child was already balanced, its height cannot
      // decrease when pivoting.
      current->balance += pivot_high(current->high_child);
    }
    assert(current->high_child->balance >= 0);
    // 1. Correct balance values.
    int height_a = 0, height_b = 0, height_c = 0;
    if (current->balance < 0) {
      height_a -= current->balance;
    } else {
      height_b += current->balance;
      height_c += current->balance;
    }
    if (current->high_child->balance < 0) {
      height_c += current->high_child->balance;
    } else {
      height_b -= current->high_child->balance;
    }
    const int old_height = std::max(height_a, std::max(height_b, height_c));
    height_a += 1;
    height_c -= 1;
    const int new_height = std::max(height_a, std::max(height_b, height_c));
    const int height_change = new_height - old_height;
    current->balance = height_b - height_a;
    current->high_child->balance = height_c - std::max(height_a, height_b);
    // 2. Pivot.
    optional_node temp;
    temp.swap(current->high_child);
    current.swap(temp);
    temp->high_child.swap(current->low_child);
    temp.swap(current->low_child);
    // 3. Correct total_size values. (Order here matters!)
    update_size(current->low_child.get());
    // NOTE: This is redundant when called from update_or_add.
    update_size(current.get());
    return height_change;
  }

  static int pivot_high(optional_node &current) {
    assert(current->low_child);
    // Make sure that low_child has non-positive balance.
    if (current->low_child->balance > 0) {
      // NOTE: Assuming low_child was already balanced, its height cannot
      // decrease when pivoting.
      current->balance -= pivot_low(current->low_child);
    }
    assert(current->low_child->balance <= 0);
    // 1. Correct balance values.
    // TODO: Update the stuff below!
int height_change = 0;
//     int height_a = 0, height_b = 0, height_c = 0;
//     if (current->balance < 0) {
//       height_a -= current->balance;
//     } else {
//       height_b += current->balance;
//       height_c += current->balance;
//     }
//     if (current->high_child->balance < 0) {
//       height_c += current->high_child->balance;
//     } else {
//       height_b -= current->high_child->balance;
//     }
//     const int old_height = std::max(height_a, std::max(height_b, height_c));
//     height_a += 1;
//     height_c -= 1;
//     const int new_height = std::max(height_a, std::max(height_b, height_c));
//     const int height_change = new_height - old_height;
//     current->balance = height_b - height_a;
//     current->high_child->balance = height_c - std::max(height_a, height_b);
    // 2. Pivot.
    optional_node temp;
    temp.swap(current->low_child);
    current.swap(temp);
    temp->low_child.swap(current->high_child);
    temp.swap(current->high_child);
    // 3. Correct total_size values. (Order here matters!)
    update_size(current->high_child.get());
    // NOTE: This is redundant when called from update_or_add.
    update_size(current.get());
    return height_change;
  }

  static int remove_node(optional_node &current, optional_node &removed) {
    if (!current->low_child) {
      optional_node discard;
      discard.swap(current->high_child);
      discard.swap(current);
      removed.swap(discard);
      return -1;
    }
    if (!current->high_child) {
      optional_node discard;
      discard.swap(current->low_child);
      discard.swap(current);
      removed.swap(discard);
      return -1;
    }
    optional_node new_parent;
    int height_change = 0;
    if (current->balance < 0) {
      height_change = remove_lowest_node(current, new_parent);
    } else {
      height_change = remove_highest_node(current, new_parent);
    }
    assert(new_parent);
    assert(!new_parent->low_child);
    assert(!new_parent->high_child);
    // Swap new_parent and current.
    current->low_child.swap(new_parent->low_child);
    current->high_child.swap(new_parent->high_child);
    current.swap(new_parent);
    update_size(current.get());
    update_size(new_parent.get());
    current->balance = new_parent->balance;
    // new_parent contains the removed node.
    new_parent.swap(removed);
    return height_change;
  }

  static int remove_lowest_node(optional_node &current, optional_node &removed) {
    assert(current && current->low_child);
    int height_change = 0;
    if (!current->low_child->low_child) {
      optional_node discard;
      discard.swap(current->low_child->high_child);
      discard.swap(current->low_child);
      removed.swap(discard);
      height_change = -1;
    } else {
      current->balance -= remove_lowest_node(current->low_child, removed);
      // Change will only be positive here!
      if (current->balance == 1) {
        height_change = -1;
      } else if (current->balance > 1) {
        height_change = pivot_low(current);
      }
    }
    return height_change;
  }

  static int remove_highest_node(optional_node &current, optional_node &removed) {
    assert(current && current->high_child);
    int height_change = 0;
    if (!current->high_child->high_child) {
      optional_node discard;
      discard.swap(current->high_child->low_child);
      discard.swap(current->high_child);
      removed.swap(discard);
      height_change = -1;
    } else {
      current->balance += remove_highest_node(current->high_child, removed);
      // Change will only be negative here!
      if (current->balance == -1) {
        height_change = -1;
      } else if (current->balance < -1) {
        height_change = pivot_high(current);
      }
    }
    return height_change;
  }

  int  balance;
  Size total_size;

  category <Type, Size> data;

  optional_node low_child, high_child;
};

#endif //category_tree_hpp
