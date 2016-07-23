#include "helpers.hpp"

#include <sstream>

#include <stdio.h>
#include <string.h>

bool parse_lambda_and_label(const std::string &input, double &lambda, std::string &label) {
    char buffer[256];
    memset(buffer, 0, sizeof buffer);
    if (sscanf(input.c_str(), "%lf:%256[\001-~]", &lambda, buffer) != 2) {
      fprintf(stderr, "Failed to parse \"%s\".\n", input.c_str());
      return false;
    } else {
      label = buffer;
      return true;
    }
}
