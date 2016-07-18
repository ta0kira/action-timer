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

#include <iostream>
#include <string>
#include <utility>

#include <stdio.h>
#include <string.h>

#include "action-timer.hpp"

// For linking of non-template locking-container symbols.
#include "locking-container.inc"

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
  case s: unescaped.push_back(v); break;

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

} //namespace

int main(int argc, char *argv[]) {
  action_timer <std::string> actions;
  actions.set_category("check_for_updates", 1.0);
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
        action.reset(new async_action([text] {
          print_action(text);
        }));
      }
      actions.set_action(category, std::move(action));
    }
  }
}
