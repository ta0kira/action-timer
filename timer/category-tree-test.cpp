#include <iostream>
#include <memory>
#include <string>

#define TESTING
#include "category-tree.hpp"
#undef TESTING

#define RUN_TEST(t) { \
  std::cerr << "##### " << #t << " >>>>>" << std::endl; \
  t; \
  std::cerr << "<<<<< " << #t << " #####" << std::endl << std::endl; \
}

#define EXPECT_TRUE(t) std::cerr << ((t)? "PASS: " : "FAIL: ") << #t << std::endl

using node_type = category_node <std::string, int>;

struct category_node_test {
  void run() {
    RUN_TEST(test_exists_self());
    RUN_TEST(test_exists_child());
    RUN_TEST(test_find());
    RUN_TEST(test_update_size());

    RUN_TEST(test_remove_lowest_node_no_rebalance_1_0());
    RUN_TEST(test_remove_lowest_node_no_rebalance_1_1());
    RUN_TEST(test_remove_lowest_node_no_rebalance_2_1());

    RUN_TEST(test_remove_highest_node_no_rebalance_0_1());
    RUN_TEST(test_remove_highest_node_no_rebalance_1_1());
    RUN_TEST(test_remove_highest_node_no_rebalance_1_2());

    RUN_TEST(test_insert_no_rebalance());

    RUN_TEST(test_pivot_low_no_recursion_1_1());
    RUN_TEST(test_pivot_low_no_recursion_1_2());
  }

  void test_exists_self() {
    node_type node("A", 2);
    EXPECT_TRUE(node.category_exists("A"));
    EXPECT_TRUE(!node.category_exists("B"));
  }

  void test_exists_child() {
    // NOTE: These must be manually sorted!
    node_type node("B", 2);
    node.low_child.reset(new node_type("A", 2));
    node.high_child.reset(new node_type("C", 2));
    EXPECT_TRUE(node.category_exists("B"));
    EXPECT_TRUE(node.category_exists("C"));
    EXPECT_TRUE(!node.category_exists("D"));
  }

  void test_find() {
    node_type node("B", 1);
    node.low_child.reset(new node_type("A", 2));
    node.high_child.reset(new node_type("C", 3));
    node.total_size = 6;
    EXPECT_TRUE(node.find(0) == "B");
    EXPECT_TRUE(node.find(1) == "B");
    EXPECT_TRUE(node.find(2) == "A");
    EXPECT_TRUE(node.find(3) == "A");
    EXPECT_TRUE(node.find(4) == "C");
    EXPECT_TRUE(node.find(5) == "C");
    EXPECT_TRUE(node.find(6) == "C");
  }

  void test_update_size() {
    node_type node("B", 1);
    node.low_child.reset(new node_type("A", 2));
    node.high_child.reset(new node_type("C", 3));
    EXPECT_TRUE(node.total_size == 0);
    node_type::update_size(&node);
    EXPECT_TRUE(node.total_size == 1);
    EXPECT_TRUE(node.low_child->total_size  == 0);
    EXPECT_TRUE(node.high_child->total_size == 0);
    node_type::update_size(node.low_child.get());
    node_type::update_size(node.high_child.get());
    node_type::update_size(&node);
    EXPECT_TRUE(node.total_size == 6);
    EXPECT_TRUE(node.low_child->total_size  == 2);
    EXPECT_TRUE(node.high_child->total_size == 3);
  }

  void test_remove_lowest_node_no_rebalance_1_0() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    int height_change = 0;
    node->low_child.reset(new node_type("A", 2));
    height_change = node_type::remove_lowest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "A");
    EXPECT_TRUE(height_change == -1);
  }

  void test_remove_lowest_node_no_rebalance_1_1() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    int height_change = 0;
    node->low_child.reset(new node_type("A", 2));
    node->high_child.reset(new node_type("C", 2));
    height_change = node_type::remove_lowest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "A");
    EXPECT_TRUE(height_change == -1);
  }

  void test_remove_lowest_node_no_rebalance_2_1() {
    std::unique_ptr <node_type> node(new node_type("C", 2)), removed;
    int height_change = 0;
    node->low_child.reset(new node_type("B", 2));
    node->low_child->low_child.reset(new node_type("A", 2));
    node->high_child.reset(new node_type("D", 2));
    height_change = node_type::remove_lowest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "A");
    EXPECT_TRUE(height_change == -1);
  }

  void test_remove_highest_node_no_rebalance_0_1() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    int height_change = 0;
    node->high_child.reset(new node_type("C", 2));
    height_change = node_type::remove_highest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "C");
    EXPECT_TRUE(height_change == -1);
  }

  void test_remove_highest_node_no_rebalance_1_1() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    int height_change = 0;
    node->low_child.reset(new node_type("A", 2));
    node->high_child.reset(new node_type("C", 2));
    height_change = node_type::remove_highest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "C");
    EXPECT_TRUE(height_change == -1);
  }

  void test_remove_highest_node_no_rebalance_1_2() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    int height_change = 0;
    node->low_child.reset(new node_type("A", 2));
    node->high_child.reset(new node_type("C", 2));
    node->high_child->high_child.reset(new node_type("D", 2));
    height_change = node_type::remove_highest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "D");
    EXPECT_TRUE(height_change == -1);
  }

  void test_insert_no_rebalance() {
    std::unique_ptr <node_type> node;
    EXPECT_TRUE(node_type::update_or_add(node, "B", 1) == 1);
    EXPECT_TRUE(node_type::update_or_add(node, "A", 2) == 1);
    EXPECT_TRUE(node_type::update_or_add(node, "C", 3) == 0);
    EXPECT_TRUE(node && node->data.value == "B");
    EXPECT_TRUE(node && node->data.size == 1);
    EXPECT_TRUE(node && node->total_size == 6);
    EXPECT_TRUE(node->low_child && node->low_child->data.value == "A");
    EXPECT_TRUE(node->low_child && node->low_child->data.size == 2);
    EXPECT_TRUE(node->low_child && node->low_child->total_size == 2);
    EXPECT_TRUE(node->high_child && node->high_child->data.value == "C");
    EXPECT_TRUE(node->high_child && node->high_child->data.size == 3);
    EXPECT_TRUE(node->high_child && node->high_child->total_size == 3);
  }

  void test_pivot_low_no_recursion_1_1() {
    std::unique_ptr <node_type> node;
    EXPECT_TRUE(node_type::update_or_add(node, "B", 1) == 1);
    EXPECT_TRUE(node_type::update_or_add(node, "A", 2) == 1);
    EXPECT_TRUE(node_type::update_or_add(node, "C", 3) == 0);
    EXPECT_TRUE(node_type::pivot_low(node) == 1);
    EXPECT_TRUE(node && node->data.value == "C");
    EXPECT_TRUE(node->low_child && node->low_child->data.value == "B");
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->data.value == "A");
    EXPECT_TRUE(node && node->balance == -2);
    EXPECT_TRUE(node->low_child && node->low_child->balance == -1);
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->balance == 0);
    EXPECT_TRUE(node && node->total_size == 6);
    EXPECT_TRUE(node->low_child && node->low_child->total_size == 3);
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->total_size == 2);
  }

  void test_pivot_low_no_recursion_1_2() {
    std::unique_ptr <node_type> node;
    EXPECT_TRUE(node_type::update_or_add(node, "B", 1) == 1);
    EXPECT_TRUE(node_type::update_or_add(node, "A", 2) == 1);
    EXPECT_TRUE(node_type::update_or_add(node, "C", 3) == 0);
    EXPECT_TRUE(node_type::update_or_add(node, "D", 4) == 1);
    EXPECT_TRUE(node_type::pivot_low(node) == 0);
    EXPECT_TRUE(node && node->data.value == "C");
    EXPECT_TRUE(node->low_child && node->low_child->data.value == "B");
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->data.value == "A");
    EXPECT_TRUE(node->high_child && node->high_child->data.value == "D");
    EXPECT_TRUE(node && node->balance == -1);
    EXPECT_TRUE(node->low_child && node->low_child->balance == -1);
    EXPECT_TRUE(node->high_child && node->high_child->balance == 0);
    EXPECT_TRUE(node && node->total_size == 10);
    EXPECT_TRUE(node->low_child && node->low_child->total_size == 3);
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->total_size == 2);
    EXPECT_TRUE(node->high_child && node->high_child->total_size == 4);
  }
};

int main() {
  category_node_test tests;
  tests.run();
}
