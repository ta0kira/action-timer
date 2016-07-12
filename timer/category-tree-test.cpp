#include <gtest/gtest.h>

#define TESTING
#include "category-tree.hpp"
#undef TESTING

#include <iostream>
#include <vector>
#include <memory>
#include <string>

using string_node_type = category_node <std::string, int>;
using num_node_type    = category_node <int, int>;

class node_printer {
public:
  template <class Type, class Size>
  static void print_node(const category_node <Type, Size> &node) {
    print_value(node);
    print_node(node, "");
  }

private:
  template <class Type, class Size>
  static void print_node(const category_node <Type, Size> &node,
                         const std::string &padding) {
    if (node.low_child) {
      std::cerr << padding << "|- ";
      print_value(*node.low_child);
      print_node(*node.low_child, padding + "|  ");
    } else if (node.high_child) {
      std::cerr << padding << "|- _" << std::endl;
    }
    if (node.high_child) {
      std::cerr << padding << "\\- ";
      print_value(*node.high_child);
      print_node(*node.high_child, padding + "   ");
    } else if (node.low_child) {
      std::cerr << padding << "\\- _" << std::endl;
    }
  }

  template <class Type, class Size>
  static void print_value(const category_node <Type, Size> &node) {
    std::cerr << node.data.value << "  ["
              << node.data.size << "/"
              << node.total_size << "]";
    if (!node.low_child && !node.high_child) {
      std::cerr << " *";
    }
    std::cerr << std::endl;
  }
};

TEST(category_node_test, test_exists_self) {
  string_node_type node("A", 2);
  EXPECT_TRUE(node.category_exists("A"));
  EXPECT_FALSE(node.category_exists("B"));
}

TEST(category_node_test, test_exists_child) {
  // NOTE: These must be manually sorted!
  string_node_type node("B", 2);
  node.low_child.reset(new string_node_type("A", 2));
  node.high_child.reset(new string_node_type("C", 2));
  EXPECT_TRUE(node.category_exists("B"));
  EXPECT_TRUE(node.category_exists("C"));
  EXPECT_FALSE(node.category_exists("D"));
}

TEST(category_node_test, test_locate) {
  std::unique_ptr <string_node_type> node;
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "A", 1);
  string_node_type::update_or_add(node, "D", 4);
  string_node_type::update_or_add(node, "C", 3);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("A", node->locate(0));
  EXPECT_EQ("B", node->locate(1));
  EXPECT_EQ("B", node->locate(2));
  EXPECT_EQ("C", node->locate(3));
  EXPECT_EQ("C", node->locate(4));
  EXPECT_EQ("C", node->locate(5));
  EXPECT_EQ("D", node->locate(6));
  EXPECT_EQ("D", node->locate(7));
  EXPECT_EQ("D", node->locate(8));
  EXPECT_EQ("D", node->locate(9));
}

TEST(category_node_test, test_update_size) {
  string_node_type node("B", 1);
  node.low_child.reset(new string_node_type("A", 2));
  node.high_child.reset(new string_node_type("C", 3));
  EXPECT_EQ(0, node.total_size);
  node.update_size();
  EXPECT_EQ(1, node.total_size);
  EXPECT_EQ(0, node.low_child->total_size);
  EXPECT_EQ(0, node.high_child->total_size);
  node.low_child->update_size();
  node.high_child->update_size();
  node.update_size();
  EXPECT_EQ(6, node.total_size);
  EXPECT_EQ(2, node.low_child->total_size);
  EXPECT_EQ(3, node.high_child->total_size);
}

TEST(category_node_test, test_insert_no_rebalance) {
  std::unique_ptr <string_node_type> node;
  string_node_type::update_or_add(node, "B", 1);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "C", 3);
  EXPECT_NE(nullptr, node);
  EXPECT_TRUE(node->validate_balanced());
  EXPECT_TRUE(node->validate_sorted());
  EXPECT_TRUE(node->validate_sized());
}

TEST(category_node_test, test_insert_rebalance_ordered) {
  std::unique_ptr <num_node_type> node;
  const int element_count = (1 << 6) + (1 << 5);
  for (int i = 0; i < element_count; ++i) {
    num_node_type::update_or_add(node, i, 1);
    EXPECT_EQ(i + 1, node->get_total_size());
    // (Yes, this makes it quadratic...)
    EXPECT_TRUE(node->validate_balanced());
    EXPECT_TRUE(node->validate_sorted());
    EXPECT_TRUE(node->validate_sized());
  }
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ(element_count, node->get_total_size());
  EXPECT_TRUE(node->validate_balanced());
  EXPECT_TRUE(node->validate_sorted());
  EXPECT_TRUE(node->validate_sized());
  for (int i = 0; i < node->get_total_size(); ++i) {
    EXPECT_EQ(i, node->locate(i));
  }
  for (int i = 0; i < element_count; ++i) {
    EXPECT_TRUE(node->category_exists(i));
  }
}

TEST(category_node_test, test_insert_rebalance_unordered) {
  std::unique_ptr <num_node_type> node;
  const int element_count = (1 << 6) + (1 << 5);
  for (int i = 0; i < element_count; ++i) {
    const int adjusted = ((i + 13) * 19) % element_count;
    num_node_type::update_or_add(node, adjusted, 1);
    EXPECT_EQ(i + 1, node->get_total_size());
    // (Yes, this makes it quadratic...)
    EXPECT_TRUE(node->validate_balanced());
    EXPECT_TRUE(node->validate_sorted());
    EXPECT_TRUE(node->validate_sized());
  }
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ(element_count, node->get_total_size());
  EXPECT_TRUE(node->validate_balanced());
  EXPECT_TRUE(node->validate_sorted());
  EXPECT_TRUE(node->validate_sized());
  for (int i = 0; i < node->get_total_size(); ++i) {
    EXPECT_EQ(i, node->locate(i));
  }
  for (int i = 0; i < element_count; ++i) {
    EXPECT_TRUE(node->category_exists(i));
  }
}

TEST(category_node_test, erase_all_unordered) {
  std::unique_ptr <num_node_type> node;
  const int element_count = (1 << 6) + (1 << 5);
  for (int i = 0; i < element_count; ++i) {
    num_node_type::update_or_add(node, i, 1);
  }
  EXPECT_NE(nullptr, node);
  EXPECT_EQ(element_count, node->get_total_size());
  for (int i = 0; i < element_count; ++i) {
    const int adjusted = ((i + 13) * 19) % element_count;
    EXPECT_TRUE(node->category_exists(adjusted));
    num_node_type::erase(node, adjusted);
    if (i == element_count - 1) {
      EXPECT_EQ(nullptr, node);
    } else {
      EXPECT_NE(nullptr, node);
      EXPECT_FALSE(node->category_exists(adjusted));
      EXPECT_EQ(element_count - (i + 1), node->get_total_size());
      // (Yes, this makes it quadratic...)
      EXPECT_TRUE(node->validate_balanced());
      EXPECT_TRUE(node->validate_sorted());
      EXPECT_TRUE(node->validate_sized());
    }
  }
  EXPECT_EQ(nullptr, node);
}

TEST(category_node_test, test_remove_lowest_node_no_rebalance_1_0) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "B", 1);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::remove_lowest_node(node, removed);
  EXPECT_NE(nullptr, removed);
  node_printer::print_node(*node);
  EXPECT_EQ("A", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_lowest_node_no_rebalance_1_1) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "C", 2);
  string_node_type::remove_lowest_node(node, removed);
  EXPECT_NE(nullptr, removed);
  node_printer::print_node(*node);
  EXPECT_EQ("A", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_lowest_node_no_rebalance_2_1) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "C", 2);
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "D", 2);
  string_node_type::remove_lowest_node(node, removed);
  EXPECT_NE(nullptr, removed);
  node_printer::print_node(*node);
  EXPECT_EQ("A", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_lowest_node_rebalance_1_2) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "A", 1);
  string_node_type::update_or_add(node, "C", 3);
  string_node_type::update_or_add(node, "D", 4);
  string_node_type::remove_lowest_node(node, removed);
  EXPECT_NE(nullptr, removed);
  node_printer::print_node(*node);
  EXPECT_EQ("A", removed->data.value);
  EXPECT_EQ(1, removed->height);
  EXPECT_TRUE(node->validate_balanced());
  EXPECT_TRUE(node->validate_sorted());
  EXPECT_TRUE(node->validate_sized());
}

TEST(category_node_test, test_remove_highest_node_no_rebalance_0_1) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "C", 2);
  string_node_type::remove_highest_node(node, removed);
  EXPECT_NE(nullptr, removed);
  node_printer::print_node(*node);
  EXPECT_EQ("C", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_highest_node_no_rebalance_1_1) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "C", 2);
  string_node_type::remove_highest_node(node, removed);
  EXPECT_NE(nullptr, removed);
  node_printer::print_node(*node);
  EXPECT_EQ("C", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_highest_node_no_rebalance_1_2) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "C", 2);
  string_node_type::update_or_add(node, "D", 2);
  string_node_type::remove_highest_node(node, removed);
  EXPECT_NE(nullptr, removed);
  node_printer::print_node(*node);
  EXPECT_EQ("D", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_highest_node_rebalance_2_1) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "C", 3);
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "A", 1);
  string_node_type::update_or_add(node, "D", 4);
  string_node_type::remove_highest_node(node, removed);
  EXPECT_NE(nullptr, removed);
  node_printer::print_node(*node);
  EXPECT_EQ("D", removed->data.value);
  EXPECT_EQ(1, removed->height);
  EXPECT_TRUE(node->validate_balanced());
  EXPECT_TRUE(node->validate_sorted());
  EXPECT_TRUE(node->validate_sized());
}

TEST(category_node_test, test_remove_node_no_rebalance_low) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::remove_node(node, removed);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("A", node->data.value);
  EXPECT_NE(nullptr, removed);
  EXPECT_EQ("B", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_node_no_rebalance_high) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::remove_node(node, removed);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("B", node->data.value);
  EXPECT_NE(nullptr, removed);
  EXPECT_EQ("A", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_node_no_rebalance_low_low) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "C", 2);
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "D", 2);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::remove_node(node, removed);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("B", node->data.value);
  EXPECT_NE(nullptr, removed);
  EXPECT_EQ("C", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_node_no_rebalance_low_high) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "C", 2);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "D", 2);
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::remove_node(node, removed);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("B", node->data.value);
  EXPECT_NE(nullptr, removed);
  EXPECT_EQ("C", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_node_no_rebalance_high_low) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "D", 2);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "C", 2);
  string_node_type::remove_node(node, removed);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("C", node->data.value);
  EXPECT_NE(nullptr, removed);
  EXPECT_EQ("B", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_remove_node_no_rebalance_high_high) {
  std::unique_ptr <string_node_type> node, removed;
  string_node_type::update_or_add(node, "B", 2);
  string_node_type::update_or_add(node, "C", 2);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "D", 2);
  string_node_type::remove_node(node, removed);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("C", node->data.value);
  EXPECT_NE(nullptr, removed);
  EXPECT_EQ("B", removed->data.value);
  EXPECT_EQ(1, removed->height);
}

TEST(category_node_test, test_pivot_low_no_recursion_1_1) {
  std::unique_ptr <string_node_type> node;
  string_node_type::update_or_add(node, "B", 1);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "C", 3);
  string_node_type::pivot_low(node);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("C", node->data.value);
  EXPECT_NE(nullptr, node->low_child);
  EXPECT_EQ("B", node->low_child->data.value);
  EXPECT_NE(nullptr, node->low_child->low_child);
  EXPECT_EQ("A", node->low_child->low_child->data.value);
  EXPECT_EQ(3, node->height);
  EXPECT_EQ(2, node->low_child->height);
  EXPECT_EQ(1, node->low_child->low_child->height);
  EXPECT_EQ(6, node->total_size);
  EXPECT_EQ(3, node->low_child->total_size);
  EXPECT_EQ(2, node->low_child->low_child->total_size);
}

TEST(category_node_test, test_pivot_low_no_recursion_1_2) {
  std::unique_ptr <string_node_type> node;
  string_node_type::update_or_add(node, "B", 1);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "C", 3);
  string_node_type::update_or_add(node, "D", 4);
  string_node_type::pivot_low(node);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("C", node->data.value);
  EXPECT_NE(nullptr, node->low_child);
  EXPECT_EQ("B", node->low_child->data.value);
  EXPECT_NE(nullptr, node->low_child->low_child);
  EXPECT_EQ("A", node->low_child->low_child->data.value);
  EXPECT_EQ("D", node->high_child->data.value);
  EXPECT_EQ(3, node->height);
  EXPECT_EQ(2, node->low_child->height);
  EXPECT_EQ(1, node->high_child->height);
  EXPECT_EQ(10, node->total_size);
  EXPECT_EQ(3, node->low_child->total_size);
  EXPECT_EQ(2, node->low_child->low_child->total_size);
  EXPECT_EQ(4, node->high_child->total_size);
}

TEST(category_node_test, test_pivot_low_high_recursion_1_2) {
  std::unique_ptr <string_node_type> node;
  string_node_type::update_or_add(node, "B", 1);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "D", 4);
  string_node_type::update_or_add(node, "C", 3);
  string_node_type::pivot_low(node);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("C", node->data.value);
  EXPECT_NE(nullptr, node->low_child);
  EXPECT_EQ("B", node->low_child->data.value);
  EXPECT_NE(nullptr, node->low_child->low_child);
  EXPECT_EQ("A", node->low_child->low_child->data.value);
  EXPECT_EQ("D", node->high_child->data.value);
  EXPECT_EQ(3, node->height);
  EXPECT_EQ(2, node->low_child->height);
  EXPECT_EQ(1, node->high_child->height);
  EXPECT_EQ(10, node->total_size);
  EXPECT_EQ(3, node->low_child->total_size);
  EXPECT_EQ(2, node->low_child->low_child->total_size);
  EXPECT_EQ(4, node->high_child->total_size);
}

TEST(category_node_test, test_pivot_high_no_recursion_1_1) {
  std::unique_ptr <string_node_type> node;
  string_node_type::update_or_add(node, "B", 1);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "C", 3);
  string_node_type::pivot_high(node);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("A", node->data.value);
  EXPECT_NE(nullptr, node->high_child);
  EXPECT_EQ("B", node->high_child->data.value);
  EXPECT_NE(nullptr, node->high_child->high_child);
  EXPECT_EQ("C", node->high_child->high_child->data.value);
  EXPECT_EQ(3, node->height);
  EXPECT_EQ(2, node->high_child->height);
  EXPECT_EQ(1, node->high_child->high_child->height);
  EXPECT_EQ(6, node->total_size);
  EXPECT_EQ(4, node->high_child->total_size);
  EXPECT_EQ(3, node->high_child->high_child->total_size);
}

TEST(category_node_test, test_pivot_high_no_recursion_2_1) {
  std::unique_ptr <string_node_type> node;
  string_node_type::update_or_add(node, "C", 3);
  string_node_type::update_or_add(node, "B", 1);
  string_node_type::update_or_add(node, "D", 4);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::pivot_high(node);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("B", node->data.value);
  EXPECT_NE(nullptr, node->high_child);
  EXPECT_EQ("C", node->high_child->data.value);
  EXPECT_NE(nullptr, node->high_child->high_child);
  EXPECT_EQ("D", node->high_child->high_child->data.value);
  EXPECT_EQ("A", node->low_child->data.value);
  EXPECT_EQ(3, node->height);
  EXPECT_EQ(2, node->high_child->height);
  EXPECT_EQ(1, node->low_child->height);
  EXPECT_EQ(10, node->total_size);
  EXPECT_EQ(7, node->high_child->total_size);
  EXPECT_EQ(4, node->high_child->high_child->total_size);
  EXPECT_EQ(2, node->low_child->total_size);
}

TEST(category_node_test, test_pivot_high_low_recursion_2_1) {
  std::unique_ptr <string_node_type> node;
  string_node_type::update_or_add(node, "C", 3);
  string_node_type::update_or_add(node, "A", 2);
  string_node_type::update_or_add(node, "D", 4);
  string_node_type::update_or_add(node, "B", 1);
  string_node_type::pivot_high(node);
  EXPECT_NE(nullptr, node);
  node_printer::print_node(*node);
  EXPECT_EQ("B", node->data.value);
  EXPECT_NE(nullptr, node->high_child);
  EXPECT_EQ("C", node->high_child->data.value);
  EXPECT_NE(nullptr, node->high_child->high_child);
  EXPECT_EQ("D", node->high_child->high_child->data.value);
  EXPECT_EQ("A", node->low_child->data.value);
  EXPECT_EQ(3, node->height);
  EXPECT_EQ(2, node->high_child->height);
  EXPECT_EQ(1, node->low_child->height);
  EXPECT_EQ(10, node->total_size);
  EXPECT_EQ(7, node->high_child->total_size);
  EXPECT_EQ(4, node->high_child->high_child->total_size);
  EXPECT_EQ(2, node->low_child->total_size);
}

TEST(category_tree_test, integration_test) {
  category_tree <int> tree;
  const int element_count = (1 << 8) + (1 << 7);
  for (int i = 0; i < element_count; ++i) {
    const int adjusted = ((i + 19) * 13) % element_count;
    tree.update_or_add(adjusted, 2);
    EXPECT_EQ(2 * (i + 1), tree.get_total_size());
    // (Yes, this makes it quadratic...)
    EXPECT_TRUE(tree.root->validate_balanced());
    EXPECT_TRUE(tree.root->validate_sorted());
    EXPECT_TRUE(tree.root->validate_sized());
  }
  EXPECT_NE(nullptr, tree.root);
  EXPECT_EQ(2 * element_count, tree.get_total_size());
  for (int i = 0; i < tree.get_total_size(); ++i) {
    EXPECT_EQ(i / 2, tree.locate(i));
  }
  for (int i = 0; i < element_count; ++i) {
    EXPECT_TRUE(tree.category_exists(i));
  }
  for (int i = 0; i < element_count; ++i) {
    const int adjusted = ((i + 13) * 19) % element_count;
    EXPECT_TRUE(tree.category_exists(adjusted));
    tree.erase(adjusted);
    EXPECT_EQ(2 * (element_count - (i + 1)), tree.get_total_size());
    EXPECT_FALSE(tree.category_exists(adjusted));
    if (i == element_count - 1) {
      EXPECT_EQ(nullptr, tree.root);
    } else {
      EXPECT_NE(nullptr, tree.root);
      // (Yes, this makes it quadratic...)
      EXPECT_TRUE(tree.root->validate_balanced());
      EXPECT_TRUE(tree.root->validate_sorted());
      EXPECT_TRUE(tree.root->validate_sized());
    }
  }
  EXPECT_EQ(0, tree.get_total_size());
  EXPECT_EQ(nullptr, tree.root);
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
