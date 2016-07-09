#include <iostream>
#include <string>

#include <stdio.h>
#include <string.h>

#include "action-timer.hpp"

namespace {

class print_action : public thread_action {
public:
  print_action(const std::string &new_output) : output(new_output) {}

  void action() override {
    std::cout << output;
    std::cout.flush();
  }

private:
  const std::string output;
};

inline int unsafe_hex(char val) {
  if (val >= 'a' && val <= 'f') {
    return 10 + val - 'a';
  } else if (val >= 'A' && val <= 'F') {
    return 10 + val - 'A';
  } else {
    return val - '0';
  }
}

inline int unsafe_oct(char val) {
  return val - '0';
}

bool expand_escapes(std::string &escaped) {
  std::string unescaped;
  for (unsigned int i = 0; i < escaped.size(); ++i) {
    if (escaped[i] == '\\') {
      if (i == escaped.size() - 1) {
        // TODO: Error message!
        return false;
      }

#define SINGLE_CHAR_CASE(s, v) \
  case s: unescaped.push_back(v); ++i; break;

      switch (escaped[++i]) {
        SINGLE_CHAR_CASE('a', '\a')
        SINGLE_CHAR_CASE('b', '\b')
        SINGLE_CHAR_CASE('f', '\f')
        SINGLE_CHAR_CASE('n', '\n')
        SINGLE_CHAR_CASE('r', '\r')
        SINGLE_CHAR_CASE('t', '\t')
        SINGLE_CHAR_CASE('v', '\v')
        case 'x':
          if (i >= escaped.size() - 2) {
            // TODO: Error message!
            return false;
          }
          unescaped.push_back((char) (16*unsafe_hex(escaped[i+1]) + unsafe_hex(escaped[i+2])));
          i += 2;
          break;
        default:
          if (escaped[i] < '0' || escaped[i] > '9') {
            // TODO: Error message!
            return false;
          }
          if (i >= escaped.size() - 2) {
            // TODO: Error message!
            return false;
          }
          unescaped.push_back((char) (64*unsafe_oct(escaped[i]) + 8*unsafe_oct(escaped[i+1]) + unsafe_oct(escaped[i+2])));
          i += 2;
          break;
      }

#undef SINGLE_CHAR_CASE

    } else {
      unescaped.push_back(escaped[i]);
    }
  }

  escaped.swap(unescaped);
  return true;
}

}

int main(int argc, char *argv[]) {
  action_timer actions;
  actions.set_category("update", 1.0);
  actions.start();

  std::string input;

  while (std::getline(std::cin, input)) {
    char buffer[256];
    memset(buffer, 0, sizeof buffer);
    double lambda = 0.0;
    if (sscanf(input.c_str(), "%lf:%256[\001-~]", &lambda, buffer) < 1) {
      fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], input.c_str());
      return 1;
    }
    std::string category(buffer);
    if (!expand_escapes(category)) {
      fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], input.c_str());
      return 1;
    }
    actions.set_category(category, lambda);
    actions.set_action(category, action_timer::generic_action(new print_action(category)));
  }

  actions.join();
}
