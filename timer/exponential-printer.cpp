#include <iostream>
#include <string>
#include <utility>

#include <stdio.h>
#include <string.h>

#include "action-timer.hpp"

namespace {

void print_action(const std::string &output) {
  std::cout << output;
  std::cout.flush();
}

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
        SINGLE_CHAR_CASE('\\', '\\')
        SINGLE_CHAR_CASE('a', '\a')
        SINGLE_CHAR_CASE('b', '\b')
        SINGLE_CHAR_CASE('e', '\x1b')
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
  action_timer <std::string> actions(4);
  actions.set_category("check_for_updates", 4.0);
  actions.start();

  std::string input;

  while (std::getline(std::cin, input)) {
    char buffer[256];
    memset(buffer, 0, sizeof buffer);
    double lambda = 0.0;
    if (sscanf(input.c_str(), "%lf:%256[\001-~]", &lambda, buffer) < 1) {
      fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], input.c_str());
      continue;
    }
    const std::string category(buffer);
    std::string text(category);
    // NOTE: The string might contain '\0' after expanding, which is fine, but
    // needs to be accounted for when printing.
    if (!expand_escapes(text)) {
      fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], input.c_str());
      continue;
    }
    actions.set_category(category, lambda);
    if (category != "check_for_updates") {
      action_timer <std::string> ::generic_action action;
      if (lambda > 0) {
        action.reset(new thread_action([text] { print_action(text); }));
      }
      actions.set_action(category, std::move(action));
    }
  }
}
