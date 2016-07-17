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

#include "poisson-queue.hpp"

// For linking of non-template locking-container symbols.
#include "locking-container.inc"

int main(int argc, char *argv[]) {
  // unique_ptr isn't necessary, but it helps test move correctness.
  typedef std::unique_ptr <int> stored_type;

  poisson_queue <std::string, stored_type> queue;
  queue.start();

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
    char buffer[256];
    memset(buffer, 0, sizeof buffer);
    double lambda = 0.0;
    if (sscanf(input.c_str(), "%lf:%256s", &lambda, buffer) != 2) {
      fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], input.c_str());
      continue;
    }
    const std::string category(buffer);

    if (lambda <= 0.0) {
      queue.remove_action(category);
    } else {
      queue.set_processor(category,
        [category,lambda](stored_type &value) {
          // TODO: Add some sort of failure condition.
          std::cout << category << ": " << *value << std::endl;
          std::this_thread::sleep_for(
            std::chrono::duration <double> (1.0 / lambda));
          return true;
        }, lambda, std::max(1, (int) lambda));
    }
  }
}
