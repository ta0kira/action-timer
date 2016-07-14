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

  bool category_exists(const Type &category) const {
    return root && root->category_exists(category);
  }

  Size category_size(const Type &category) const {
    return root? root->category_size(category) : Size();
  }

  const Type &locate(Size size) const {
    assert(root && size >= Size() && size < this->get_total_size());
    return root->locate(size);
  }

  void update_category(const Type &category, Size new_size) {
    node_type::update_category(root, category, new_size);
  }

  void erase_category(const Type &category) {
    node_type::erase_category(root, category);
  }

  Size get_total_size() const {
    return root? root->get_total_size() : Size();
  }

private:
  typename node_type::optional_node root;

#ifdef TESTING
  FRIEND_TEST(category_tree_test, integration_test);
#endif
};

template <class Type, class Size>
class category_node {
public:
  using optional_node = std::unique_ptr <category_node>;

  category_node(const Type &new_category, Size new_size) :
  category(new_category), size(new_size), height(1), total_size() {}

  Size get_total_size() const {
    return total_size;
  }

  bool category_exists(const Type &check_category) const {
    if (check_category == category) return true;
    if (check_category < category) {
      return low_child?  low_child->category_exists(check_category)  : false;
    } else {
      return high_child? high_child->category_exists(check_category) : false;
    }
    return false;
  }

  Size category_size(const Type &check_category) const {
    if (check_category == category) return size;
    if (check_category < category) {
      return low_child?  low_child->category_size(check_category)  : Size();
    } else {
      return high_child? high_child->category_size(check_category) : Size();
    }
    return Size();
  }

  // The assumption is that 0 <= size < total_size, but it isn't enforced, due
  // to potential precision problems when combining/splitting intervals. Note
  // that the upper end is open, which allows this to work as expected with
  // integer size types.
  const Type &locate(Size check_size) const {
    // Interval is divided into three parts: low, self, high.
    if (low_child && check_size < low_child->total_size) {
      return low_child->locate(check_size);
    }
    // Not in first part => move to second.
    if (low_child) check_size -= low_child->total_size;
    // NOTE: Checking high_child prevents problems below if there is a precision
    // error that makes size >= size.
    if (!high_child || check_size < size) {
      return category;
    }
    // Not in second part => move to third.
    check_size -= size;
    return high_child->locate(check_size);
  }

  static void update_category(optional_node &current, const Type &new_category, Size new_size) {
    if (!current) {
      current.reset(new category_node(new_category, new_size));
    } else if (current->category == new_category) {
      current->size = new_size;
    } else if (new_category < current->category) {
      update_category(current->low_child, new_category, new_size);
    } else {
      update_category(current->high_child, new_category, new_size);
    }
    update_and_rebalance(current);
  }

  static void erase_category(optional_node &current, const Type &new_category) {
    optional_node discard;
    if (!current) {
      return;
    } else if (current->category == new_category) {
      remove_node(current, discard);
    } else if (new_category < current->category) {
      erase_category(current->low_child, new_category);
    } else {
      erase_category(current->high_child, new_category);
    }
    update_and_rebalance(current);
  }

private:
  void update_size() {
    total_size = size;
    if (low_child)  total_size += low_child->total_size;
    if (high_child) total_size += high_child->total_size;
  }

  void update_height() {
    const int low_height  = low_child?  low_child->height  : 0;
    const int high_height = high_child? high_child->height : 0;
    height = std::max(low_height, high_height) + 1;
  }

  int get_balance() {
    const int low_height  = low_child?  low_child->height  : 0;
    const int high_height = high_child? high_child->height : 0;
    return high_height - low_height;
  }

  static void update_and_rebalance(optional_node &current) {
    if (current) {
      current->update_size();
      current->update_height();
      if (current->get_balance() < -1) {
        pivot_high(current);
      }
      if (current->get_balance() > 1) {
        pivot_low(current);
      }
    }
  }

  static void pivot_low(optional_node &current) {
    assert(current->high_child);
    // Make sure that high_child has non-negative balance.
    if (current->high_child->get_balance() < 0) {
      pivot_high(current->high_child);
    }
    // Pivot.
    optional_node temp;
    temp.swap(current->high_child);
    current.swap(temp);
    temp->high_child.swap(current->low_child);
    temp.swap(current->low_child);
    // Update.
    if (current->low_child) {
      current->low_child->update_size();
      current->low_child->update_height();
    }
    current->update_size();
    current->update_height();
  }

  static void pivot_high(optional_node &current) {
    assert(current->low_child);
    // Make sure that low_child has non-positive balance.
    if (current->low_child->get_balance() > 0) {
      pivot_low(current->low_child);
    }
    // Pivot.
    optional_node temp;
    temp.swap(current->low_child);
    current.swap(temp);
    temp->low_child.swap(current->high_child);
    temp.swap(current->high_child);
    // Update.
    if (current->high_child) {
      current->high_child->update_size();
      current->high_child->update_height();
    }
    current->update_size();
    current->update_height();
  }

  static void remove_node(optional_node &current, optional_node &removed) {
    optional_node *swap_with = nullptr;
    if (!current->low_child) {
      swap_with = &current->high_child;
    }
    if (!current->high_child) {
      swap_with = &current->low_child;
    }
    if (swap_with) {
      optional_node discard;
      discard.swap(*swap_with);
      discard.swap(current);
      removed.swap(discard);
      assert(removed);
      removed->update_size();
      removed->update_height();
      return;
    }
    optional_node new_parent;
    if (current->get_balance() < 0) {
      remove_highest_node(current->low_child, new_parent);
      assert(new_parent);
    } else {
      remove_lowest_node(current->high_child, new_parent);
      assert(new_parent);
    }
    assert(new_parent);
    assert(!new_parent->low_child);
    assert(!new_parent->high_child);
    // Swap new_parent and current.
    current->low_child.swap(new_parent->low_child);
    current->high_child.swap(new_parent->high_child);
    current.swap(new_parent);
    // new_parent contains the removed node.
    new_parent.swap(removed);
    current->update_size();
    current->update_height();
    removed->update_size();
    removed->update_height();
  }

  static void remove_lowest_node(optional_node &current, optional_node &removed) {
    assert(current);
    if (!current->low_child) {
      optional_node discard;
      discard.swap(current->high_child);
      discard.swap(current);
      removed.swap(discard);
    } else {
      remove_lowest_node(current->low_child, removed);
      update_and_rebalance(current);
    }
  }

  static void remove_highest_node(optional_node &current, optional_node &removed) {
    assert(current);
    if (!current->high_child) {
      optional_node discard;
      discard.swap(current->low_child);
      discard.swap(current);
      removed.swap(discard);
    } else {
      remove_highest_node(current->high_child, removed);
      update_and_rebalance(current);
    }
  }

  const Type category;
  Size       size;

  int  height;
  Size total_size;

  optional_node low_child, high_child;

#ifdef TESTING
  FRIEND_TEST(category_node_test, test_exists_child);
  FRIEND_TEST(category_node_test, test_update_size);
  FRIEND_TEST(category_node_test, test_insert_no_rebalance);
  FRIEND_TEST(category_node_test, test_insert_rebalance_ordered);
  FRIEND_TEST(category_node_test, test_insert_rebalance_unordered);
  FRIEND_TEST(category_node_test, erase_category_all_unordered);
  FRIEND_TEST(category_node_test, test_remove_lowest_node_single);
  FRIEND_TEST(category_node_test, test_remove_lowest_node_no_rebalance_1_0);
  FRIEND_TEST(category_node_test, test_remove_lowest_node_no_rebalance_1_1);
  FRIEND_TEST(category_node_test, test_remove_lowest_node_no_rebalance_2_1);
  FRIEND_TEST(category_node_test, test_remove_lowest_node_rebalance_1_2);
  FRIEND_TEST(category_node_test, test_remove_highest_node_single);
  FRIEND_TEST(category_node_test, test_remove_highest_node_no_rebalance_0_1);
  FRIEND_TEST(category_node_test, test_remove_highest_node_no_rebalance_1_1);
  FRIEND_TEST(category_node_test, test_remove_highest_node_no_rebalance_1_2);
  FRIEND_TEST(category_node_test, test_remove_highest_node_rebalance_2_1);
  FRIEND_TEST(category_node_test, test_remove_node_single);
  FRIEND_TEST(category_node_test, test_remove_node_no_rebalance_low);
  FRIEND_TEST(category_node_test, test_remove_node_no_rebalance_high);
  FRIEND_TEST(category_node_test, test_remove_node_no_rebalance_low_low);
  FRIEND_TEST(category_node_test, test_remove_node_no_rebalance_low_high);
  FRIEND_TEST(category_node_test, test_remove_node_no_rebalance_high_low);
  FRIEND_TEST(category_node_test, test_remove_node_no_rebalance_high_high);
  FRIEND_TEST(category_node_test, test_pivot_low_no_recursion_1_1);
  FRIEND_TEST(category_node_test, test_pivot_low_no_recursion_1_2);
  FRIEND_TEST(category_node_test, test_pivot_low_high_recursion_1_2);
  FRIEND_TEST(category_node_test, test_pivot_high_no_recursion_1_1);
  FRIEND_TEST(category_node_test, test_pivot_high_no_recursion_2_1);
  FRIEND_TEST(category_node_test, test_pivot_high_low_recursion_2_1);
  FRIEND_TEST(category_tree_test, integration_test);

  friend class node_printer;

  bool validate_tree(std::function <bool(const category_node&)> validate) const {
    assert(validate);
    if (!validate(*this)) return false;
    if (low_child  && !low_child->validate_tree(validate))  return false;
    if (high_child && !high_child->validate_tree(validate)) return false;
    return true;
  }

  bool validate_sorted() const {
    return this->validate_tree([](const category_node &node) {
      if (node.low_child  && node.low_child->category  >= node.category) return false;
      if (node.high_child && node.high_child->category <= node.category) return false;
      return true;
    });
  }

  bool validate_balanced() const {
    return this->validate_tree([](const category_node &node) {
      const int high_height = node.high_child? node.high_child->height : 0;
      const int low_height  = node.low_child?  node.low_child->height  : 0;
      if (abs(high_height - low_height) > 1) return false;
      if (node.height != std::max(low_height, high_height) + 1) return false;
      return true;
    });
  }

  bool validate_sized() const {
    return this->validate_tree([](const category_node &node) {
      // NOTE: This must match update_size to avoid precision errors!
      Size actual_size = node.size;
      if (node.low_child)  actual_size += node.low_child->total_size;
      if (node.high_child) actual_size += node.high_child->total_size;
      return node.total_size == actual_size;
    });
  }
#endif

};

#endif //category_tree_hpp
