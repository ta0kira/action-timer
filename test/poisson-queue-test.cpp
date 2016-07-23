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
#include <memory>
#include <string>

#include <string.h>

#include "helpers.hpp"
#include "poisson-queue.hpp"

int main(int argc, char *argv[]) {
  // unique_ptr isn't necessary, but it helps test move correctness.
  typedef std::unique_ptr <int> stored_type;

  poisson_queue <std::string, stored_type> queue(1, [] { return new precise_timer(0.01, 0.0001); });
  queue.start();

  if (argc > 1) {
    double scale = 0.0;
    char error;
    if (sscanf(argv[1], "%lf%c", &scale, &error) != 1) {
      fprintf(stderr, "%s: Failed to parse scale from \"%s\".\n", argv[0], argv[1]);
      return 1;
    }
    queue.set_scale(scale);
  }

  // NOTE: Needs to be async_action to avoid a deadlock!
  action_timer <std::string> ::generic_action zombie_action(new async_action([&queue] {
    queue.zombie_cleanup();
    return true;
  }));
  queue.set_action("zombie_cleanup", std::move(zombie_action), 1.0);

  for (int i = 0; i < 10000; ++i) {
    queue.queue_item(stored_type(new int(i)));
  }

  std::string input;

  while (std::getline(std::cin, input)) {
    double      lambda;
    std::string category;
    if (!parse_lambda_and_label(input, lambda, category)) {
      continue;
    }

    if (lambda <= 0.0) {
      queue.remove_action(category);
    } else {
      queue.set_processor(category,
        [category,lambda](stored_type &value) {
          if (*value > 0 && *value % 256 == 0) {
            // An arbitrary failure condition.
            std::cout << category << " failed: " << *value << std::endl;
            // (Change the value so that it actually gets processed later.)
            *value *= -1;
            return false;
          }
          std::cout << category << ": " << *value << std::endl;
          std::this_thread::sleep_for(
            std::chrono::duration <double> (1.0 / lambda));
          return true;
        }, lambda, std::max(1, (int) lambda));
    }
  }
}
