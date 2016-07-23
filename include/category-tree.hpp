/* -----------------------------------------------------------------------------
Copyright (c) 2016, Google Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
----------------------------------------------------------------------------- */

// Author: Kevin P. Barry [ta0kira@gmail.com] [kevinbarry@google.com]

#ifndef category_tree_hpp
#define category_tree_hpp

#include <cassert>
#include <cmath>
#include <functional>
#include <memory>
#include <stack>

template <class Category, class Size>
class category_node;

template <class Category, class Size = double>
class category_tree {
public:
  using node_type = category_node <Category, Size>;

  bool category_exists(const Category &category) const {
    return root && root->category_exists(category);
  }

  Size category_size(const Category &category) const {
    return root? root->category_size(category) : Size();
  }

  const Category &locate(Size size) const {
    assert(root && size >= Size() && size < this->get_total_size());
    return root->locate(size);
  }

  void update_category(const Category &category, Size new_size) {
    node_type::update_category(root, category, new_size);
  }

  void update_category(const Category &category,
                       const std::function <Size(Size)> &update) {
    node_type::update_category(root, category, update);
  }

  void erase_category(const Category &category) {
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

template <class Category, class Size>
class category_node {
public:
  using optional_node = std::unique_ptr <category_node>;

  category_node(const Category &new_category, Size new_size) :
  category(new_category), size(new_size), height(1), total_size() {}

  Size get_total_size() const {
    return total_size;
  }

  bool category_exists(const Category &check_category) const {
    if (check_category == category) return true;
    if (check_category < category) {
      return low_child?  low_child->category_exists(check_category)  : false;
    } else {
      return high_child? high_child->category_exists(check_category) : false;
    }
    return false;
  }

  Size category_size(const Category &check_category) const {
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
  const Category &locate(Size check_size) const {
    // Interval is divided into three parts: low, self, high.
    if (low_child && check_size < low_child->total_size) {
      return low_child->locate(check_size);
    }
    // Not in first part => move to second.
    if (low_child) check_size -= low_child->total_size;
    // NOTE: Checking high_child prevents problems below if there is a precision
    // error that makes check_size >= size.
    if (!high_child || check_size < size) {
      return category;
    }
    // Not in second part => move to third.
    check_size -= size;
    return high_child->locate(check_size);
  }

  static void update_category(optional_node &current, const Category &new_category,
                              Size new_size) {
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

  static void update_category(optional_node &current, const Category &new_category,
                              const std::function <Size(Size)> &update) {
    assert(update);
    if (!current) {
      current.reset(new category_node(new_category, update(Size())));
    } else if (current->category == new_category) {
      current->size = update(current->size);
    } else if (new_category < current->category) {
      update_category(current->low_child, new_category, update);
    } else {
      update_category(current->high_child, new_category, update);
    }
    update_and_rebalance(current);
  }

  static void erase_category(optional_node &current, const Category &new_category) {
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
      if (abs(current->get_balance()) > 1) {
        (current->get_balance() > 1 ? pivot_low : pivot_high)(current);
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
    assert(!temp);
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
    assert(!temp);
    // Update.
    if (current->high_child) {
      current->high_child->update_size();
      current->high_child->update_height();
    }
    current->update_size();
    current->update_height();
  }

  static void remove_node(optional_node &current, optional_node &removed) {
    assert(!removed);
    optional_node new_parent;
    if (current->get_balance() < 0) {
      remove_highest_node(current->low_child, new_parent);
    } else {
      remove_lowest_node(current->high_child, new_parent);
    }
    if (new_parent) {
      assert(!new_parent->low_child);
      assert(!new_parent->high_child);
      // Swap new_parent and current.
      current->low_child.swap(new_parent->low_child);
      current->high_child.swap(new_parent->high_child);
    }
    current.swap(new_parent);
    // new_parent contains the removed node.
    new_parent.swap(removed);
    assert(!new_parent);
    assert(removed);
    // Update nodes.
    if (current) {
      current->update_size();
      current->update_height();
    }
    removed->update_size();
    removed->update_height();
  }

  static void remove_lowest_node(optional_node &current, optional_node &removed) {
    assert(!removed);
    if (current) {
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
  }

  static void remove_highest_node(optional_node &current, optional_node &removed) {
    assert(!removed);
    if (current) {
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
  }

  const Category category;
  Size           size;

  int  height;
  Size total_size;

  optional_node low_child, high_child;

#ifdef TESTING
  FRIEND_TEST(category_node_test, test_exists_child);
  FRIEND_TEST(category_node_test, test_update_size);
  FRIEND_TEST(category_node_test, test_insert_no_rebalance);
  FRIEND_TEST(category_node_test, test_update_category_size);
  FRIEND_TEST(category_node_test, test_update_category_size_with_function);
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
