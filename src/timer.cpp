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

#include <thread>

#include "timer.hpp"

precise_timer::precise_timer(double cancel_granularity,
                             double min_sleep_size) :
  sleep_granularity(std::chrono::duration <double> (cancel_granularity)),
  spinlock_limit(std::chrono::duration <double> (min_sleep_size)), base_time() {
  this->mark();
}

void precise_timer::mark() {
  base_time = std::chrono::duration_cast <std::chrono::duration <double>> (
    std::chrono::high_resolution_clock::now().time_since_epoch());
}

void precise_timer::sleep_for(double time, std::function <bool()> cancel) {
  const std::chrono::duration <double> target_duration(time);
  base_time += target_duration;
  bool canceled = false;

  for (; !canceled; canceled = cancel && cancel()) {
    const auto sleep_time = base_time -
      std::chrono::duration_cast <std::chrono::duration <double>> (
        std::chrono::high_resolution_clock::now().time_since_epoch());
    if (sleep_time < std::chrono::duration <double> (0.0)) {
      break;
    }
    if (sleep_time < spinlock_limit) {
      this->spinlock_finish();
      break;
    } else if (sleep_time < sleep_granularity) {
      std::this_thread::sleep_for(sleep_time - spinlock_limit);
    } else {
      std::this_thread::sleep_for(sleep_granularity);
    }
  }

  if (canceled) {
    this->mark();
  }
}

void precise_timer::spinlock_finish() const {
  while (true) {
    const auto current_time = std::chrono::duration_cast <std::chrono::duration <double>> (
      std::chrono::high_resolution_clock::now().time_since_epoch());
    if (current_time >= base_time) {
      break;
    }
  }
}
