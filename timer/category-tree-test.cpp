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

#define EXPECT_TRUE(t) std::cout << ((t)? "PASS: " : "FAIL: ") << #t << std::endl

using node_type = category_node <std::string, int>;

struct category_node_test {
  void run() {
    RUN_TEST(test_exists_self());
    RUN_TEST(test_exists_child());
    RUN_TEST(test_locate());
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

    RUN_TEST(test_pivot_high_no_recursion_1_1());
    RUN_TEST(test_pivot_high_no_recursion_2_1());
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

  void test_locate() {
    std::unique_ptr <node_type> node;
    node_type::update_or_add(node, "B", 2);
    node_type::update_or_add(node, "A", 1);
    node_type::update_or_add(node, "D", 4);
    node_type::update_or_add(node, "C", 3);
    EXPECT_TRUE(node->locate(0) == "A");
    EXPECT_TRUE(node->locate(1) == "B");
    EXPECT_TRUE(node->locate(2) == "B");
    EXPECT_TRUE(node->locate(3) == "C");
    EXPECT_TRUE(node->locate(4) == "C");
    EXPECT_TRUE(node->locate(5) == "C");
    EXPECT_TRUE(node->locate(6) == "D");
    EXPECT_TRUE(node->locate(7) == "D");
    EXPECT_TRUE(node->locate(8) == "D");
    EXPECT_TRUE(node->locate(9) == "D");
  }

  void test_update_size() {
    node_type node("B", 1);
    node.low_child.reset(new node_type("A", 2));
    node.high_child.reset(new node_type("C", 3));
    EXPECT_TRUE(node.total_size == 0);
    node.update_size();
    EXPECT_TRUE(node.total_size == 1);
    EXPECT_TRUE(node.low_child->total_size  == 0);
    EXPECT_TRUE(node.high_child->total_size == 0);
    node.low_child->update_size();
    node.high_child->update_size();
    node.update_size();
    EXPECT_TRUE(node.total_size == 6);
    EXPECT_TRUE(node.low_child->total_size  == 2);
    EXPECT_TRUE(node.high_child->total_size == 3);
  }

  void test_remove_lowest_node_no_rebalance_1_0() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    node->low_child.reset(new node_type("A", 2));
    node_type::remove_lowest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "A");
  }

  void test_remove_lowest_node_no_rebalance_1_1() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    node->low_child.reset(new node_type("A", 2));
    node->high_child.reset(new node_type("C", 2));
    node_type::remove_lowest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "A");
  }

  void test_remove_lowest_node_no_rebalance_2_1() {
    std::unique_ptr <node_type> node(new node_type("C", 2)), removed;
    node->low_child.reset(new node_type("B", 2));
    node->low_child->low_child.reset(new node_type("A", 2));
    node->high_child.reset(new node_type("D", 2));
    node_type::remove_lowest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "A");
  }

  void test_remove_highest_node_no_rebalance_0_1() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    node->high_child.reset(new node_type("C", 2));
    node_type::remove_highest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "C");
  }

  void test_remove_highest_node_no_rebalance_1_1() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    node->low_child.reset(new node_type("A", 2));
    node->high_child.reset(new node_type("C", 2));
    node_type::remove_highest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "C");
  }

  void test_remove_highest_node_no_rebalance_1_2() {
    std::unique_ptr <node_type> node(new node_type("B", 2)), removed;
    node->low_child.reset(new node_type("A", 2));
    node->high_child.reset(new node_type("C", 2));
    node->high_child->high_child.reset(new node_type("D", 2));
    node_type::remove_highest_node(node, removed);
    EXPECT_TRUE(removed && removed->data.value == "D");
  }

  void test_insert_no_rebalance() {
    std::unique_ptr <node_type> node;
    node_type::update_or_add(node, "B", 1);
    node_type::update_or_add(node, "A", 2);
    node_type::update_or_add(node, "C", 3);
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
    node_type::update_or_add(node, "B", 1);
    node_type::update_or_add(node, "A", 2);
    node_type::update_or_add(node, "C", 3);
    node_type::pivot_low(node);
    EXPECT_TRUE(node && node->data.value == "C");
    EXPECT_TRUE(node->low_child && node->low_child->data.value == "B");
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->data.value == "A");
    EXPECT_TRUE(node && node->height == 3);
    EXPECT_TRUE(node->low_child && node->low_child->height == 2);
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->height == 1);
    EXPECT_TRUE(node && node->total_size == 6);
    EXPECT_TRUE(node->low_child && node->low_child->total_size == 3);
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->total_size == 2);
  }

  void test_pivot_low_no_recursion_1_2() {
    std::unique_ptr <node_type> node;
    node_type::update_or_add(node, "B", 1);
    node_type::update_or_add(node, "A", 2);
    node_type::update_or_add(node, "C", 3);
    node_type::update_or_add(node, "D", 4);
    node_type::pivot_low(node);
    EXPECT_TRUE(node && node->data.value == "C");
    EXPECT_TRUE(node->low_child && node->low_child->data.value == "B");
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->data.value == "A");
    EXPECT_TRUE(node->high_child && node->high_child->data.value == "D");
    EXPECT_TRUE(node && node->height == 3);
    EXPECT_TRUE(node->low_child && node->low_child->height == 2);
    EXPECT_TRUE(node->high_child && node->high_child->height == 1);
    EXPECT_TRUE(node && node->total_size == 10);
    EXPECT_TRUE(node->low_child && node->low_child->total_size == 3);
    EXPECT_TRUE(node->low_child->low_child && node->low_child->low_child->total_size == 2);
    EXPECT_TRUE(node->high_child && node->high_child->total_size == 4);
  }

  void test_pivot_high_no_recursion_1_1() {
    std::unique_ptr <node_type> node;
    node_type::update_or_add(node, "B", 1);
    node_type::update_or_add(node, "A", 2);
    node_type::update_or_add(node, "C", 3);
    node_type::pivot_high(node);
    EXPECT_TRUE(node && node->data.value == "A");
    EXPECT_TRUE(node->high_child && node->high_child->data.value == "B");
    EXPECT_TRUE(node->high_child->high_child && node->high_child->high_child->data.value == "C");
    EXPECT_TRUE(node && node->height == 3);
    EXPECT_TRUE(node->high_child && node->high_child->height == 2);
    EXPECT_TRUE(node->high_child->high_child && node->high_child->high_child->height == 1);
    EXPECT_TRUE(node && node->total_size == 6);
    EXPECT_TRUE(node->high_child && node->high_child->total_size == 4);
    EXPECT_TRUE(node->high_child->high_child && node->high_child->high_child->total_size == 3);
  }

  void test_pivot_high_no_recursion_2_1() {
    std::unique_ptr <node_type> node;
    node_type::update_or_add(node, "C", 3);
    node_type::update_or_add(node, "B", 1);
    node_type::update_or_add(node, "D", 4);
    node_type::update_or_add(node, "A", 2);
    node_type::pivot_high(node);
    EXPECT_TRUE(node && node->data.value == "B");
    EXPECT_TRUE(node->high_child && node->high_child->data.value == "C");
    EXPECT_TRUE(node->high_child->high_child && node->high_child->high_child->data.value == "D");
    EXPECT_TRUE(node->low_child && node->low_child->data.value == "A");
    EXPECT_TRUE(node && node->height == 3);
    EXPECT_TRUE(node->high_child && node->high_child->height == 2);
    EXPECT_TRUE(node->low_child && node->low_child->height == 1);
    EXPECT_TRUE(node && node->total_size == 10);
    EXPECT_TRUE(node->high_child && node->high_child->total_size == 7);
    EXPECT_TRUE(node->high_child->high_child && node->high_child->high_child->total_size == 4);
    EXPECT_TRUE(node->low_child && node->low_child->total_size == 2);
  }
};

int main() {
  category_node_test tests;
  tests.run();
}
