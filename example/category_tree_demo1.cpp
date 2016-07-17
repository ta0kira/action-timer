// category_tree_demo1.cpp

#include <iostream>
#include "category-tree.hpp"

int main() {
  category_tree <char, double> categories;
  categories.update_category('A', 1.0);
  categories.update_category('B', 5.0);
  categories.update_category('C', 4.0);

  std::cout << "Choice for 0.56: "
            << categories.locate(0.56 * categories.get_total_size())
            << std::endl;
}
