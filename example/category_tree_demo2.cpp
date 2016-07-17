// category_tree_demo2.cpp

#include <iostream>
#include "category-tree.hpp"

int main() {
  category_tree <int, double> categories;
  for (int i = 0; i < 1000000; ++i) {
    categories.update_category(i, 1.0);
  }

  std::cout << "Choice for 0.56: "
            << categories.locate(0.56 * categories.get_total_size())
            << std::endl;
}
