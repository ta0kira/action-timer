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

#ifndef timer_hpp
#define timer_hpp

#include <chrono>
#include <functional>

struct sleep_timer {
  virtual void mark() = 0;
  virtual void sleep_for(double time, std::function <bool()> cancel = nullptr) = 0;
  virtual ~sleep_timer() = default;
};

// The antithesis of thread-safe!
class precise_timer : public sleep_timer {
public:
  // cancel_granularity dictates how often the cancel callback passed to
  // sleep_for will be called to check for cancelation. In general, you should
  // not count on cancelation being ultra-fast; it's primarily intended for use
  // with stopping threads in action_timer.
  // min_sleep_size sets a lower limit on what sleep length will be handled with
  // an actual sleep call. Below that limit, a spinlock will be used. Set this
  // value to something other than zero if you need precise timing for sleeps
  // that are shorter than your kernel's latency. (This is mostly a matter of
  // experimentation.) Higher values will cause the timer to consume more CPU;
  // therefore, set min_sleep_size to the lowest value possible. 0.0001 is a
  // good place to start.
  // min_sleep_size should be much smaller than cancel_granularity. If it isn't,
  // however, sleeps will occur in chunks of cancel_granularity size until the
  // remainder is smaller than the smaller of the two.
  explicit precise_timer(double cancel_granularity = 0.01,
                         double min_sleep_size = 0.0);

  void mark() override;
  void sleep_for(double time, std::function <bool()> cancel = nullptr) override;

private:
  void spinlock_finish() const;

  const std::chrono::duration <double> sleep_granularity, spinlock_limit;
  std::chrono::duration <double> base_time;
};

#endif //timer_hpp
