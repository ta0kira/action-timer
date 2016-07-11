#include <cmath>
#include <memory>
#include <stack>

template <class Type, class Size>
struct category {
  category(const Type &new_value, Size new_size) :
  value(new_value), size(new_size) {}

  Type value;
  Size size;
};

template <class Type, class Size>
class category_node {
public:
  using optional_node = std::unique_ptr <category_node>;

  category_node(const Type &new_value, Size new_size) :
  balance(0), total_size(), data(new_value, new_size) {}

  using category_type = category <Type, Size>;

  bool operator < (const category_node &other) const {
    return data->value < other.data->value;
  }

  bool operator == (const category_node &other) const {
    return data->value == other.data->value;
  }

  bool exists(const Type &value) const {
    if (data->value == value) return true;
    if (value < data->value) {
      return low_child?  low_child->exists(value)  : false;
    } else {
      return high_child? high_child->exists(value) : false;
    }
    return false;
  }

  const Type &find(Size size) const {
    // Interval is divided into three parts: self, low, high.
    if (size <= data.size) {
      return data.value;
    }
    // Not in first part => move to second.
    size -= data.size;
    if (low_child && size <= low_child->data.size) {
      return low_child->find(size);
    }
    // Not in second part => move to third.
    if (low_child) size -= low_child->data.size;
    if (high_child && size <= high_child->data.size) {
      return high_child->find(size);
    }
    // Return self by default, since numerical error might prevent size from
    // falling into the third part.
    return data.value;
  }

  Size get_total_size() {
    return total_size;
  }

  // NOTE: current is a unique_ptr passed by reference because balancing
  // operations sometimes require the node being operated on to be swapped out.
  static int update_or_add(optional_node &current, const Type &new_value, Size new_size) {
    assert(current);
    int height_change = 0;
    if (current->data->value == value) {
      current->data->size = new_size;
    } else if (value < current->data->value) {
      if (!current->low_child) {
        // Add low.
        current->low_child.reset(new category_node(new_value, new_size));
        if (balance == 0) {
          // NOTE: If balance == -1, rebalancing will happen.
          height_change = 1;
        }
        balance -= 1;
      } else {
        // Recusive call low.
        balance -= update_or_add(current->low_child, value, size);
      }
      if (balance < -1) {
        height_change = pivot_high(current);
      }
    } else {
      if (!high_child) {
        // Add high.
        current->high_child.reset(new category_node(new_value, new_size));
        if (balance == 0) {
          // NOTE: If balance == 1, rebalancing will happen.
          height_change = 1;
        }
        balance += 1;
      } else {
        // Recusive call high.
        balance += update_or_add(current->high_child, value, size);
      }
      if (balance > 1) {
        height_change = pivot_low(current);
      }
    }
    update_size(current.get());
    return height_change;
  }

  // NOTE: current is a unique_ptr passed by reference because balancing
  // operations sometimes require the node being operated on to be swapped out.
  static int erase(optional_node &current) {
    assert(false);
    // TODO: Implement!
  }

private:
  static void update_size(category_node *current) {
    if (current) {
      current->total_size = current->data.size;
      if (current->low_child)  current->total_size += current->low_child->data.size;
      if (current->high_child) current->total_size += current->high_child->data.size;
    }
  }

  static int pivot_high(optional_node &current) {
    assert(current->low_child);
    // Make sure that low_child has non-positive balance.
    if (current->low_child->balance > 0) {
      // NOTE: Assuming low_child was already balanced, its height cannot
      // decrease when pivoting.
      current->balance -= pivot_low(current->low_child);
    }
    // 1. Correct balance values.
    current->->balance += 1 - current->low_child->balance;
    current->low_child->balance += 1;
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
    current->->balance -= 1 - current->high_child->balance;
    current->high_child->balance -= 1;
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
  }

  int           balance;
  Size          total_size;
  category_type data;
  optional_node low_child, high_child;
};

template <class Type, class Size = double>
class category_tree {
public:
  using node_type = category_node <Type, Size>;

  bool exists(const Type &value) const {
    return head && head->exists(value);
  }

  const Type &find(Size size) {
    assert(head && size >= Type() && size <= this->get_total_size());
    return head->find(size);
  }

  bool update_or_add(const Type &value, Size new_size) {
    return node_type::optional_node::update_or_add(head, value, new_size);
  }

  bool erase(const Type &value) {
    return node_type::optional_node::erase(head);
  }

  Size get_total_size() const {
    return head? head->get_total_size() : Size();
  }

private:
  typename node_type::optional_node head;
};
