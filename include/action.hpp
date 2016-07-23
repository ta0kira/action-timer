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

#ifndef action_hpp
#define action_hpp

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include "locking-container.hpp"

class abstract_action {
public:
  virtual void start() = 0;
  virtual bool trigger_action() = 0;
  virtual ~abstract_action() = default;
};

// Thread-safe, except for start.
class async_action_base : public abstract_action {
public:
  // NOTE: A callback is used rather than a virtual function to avoid a race
  // condition when destructing while trying to execute the action.

  explicit async_action_base() :
  destructor_called(), action_error(), action_waiting() {}

  void start() override;
  bool trigger_action() override;

  void terminate();

  // NOTE: This waits for the thread to reach an exit point, which could result
  // in waiting for the current action to finish executing. The consequences
  // should be no worse than the action being executed. For this reason, the
  // action shouldn't block forever for an reason.
  ~async_action_base() override;

private:
  virtual bool action() = 0;

  void thread_loop();

  std::atomic <bool> destructor_called, action_error;
  std::unique_ptr <std::thread> thread;

  bool action_waiting;

  std::mutex               action_lock;
  std::condition_variable  action_wait;
};

// Thread-safe, except for start.
class async_action : public async_action_base {
public:
  explicit async_action(std::function <bool()> new_action) :
  action_callback(std::move(new_action)) {}

private:
  bool action() override {
    assert(action_callback);
    return this->action_callback();
  }

  const std::function <bool()> action_callback;
};

// Thread-safe.
class sync_action_base : public abstract_action {
public:
  void start() override {}
  bool trigger_action() override {
    return this->action();
  }

private:
  virtual bool action() = 0;
};

// Thread-safe.
class sync_action : public sync_action_base {
public:
  explicit sync_action(std::function <bool()> new_action) :
  action_callback(std::move(new_action)) {}

  // NOTE: This waits for an ongoing action to complete.
  ~sync_action() override;

private:
  bool action() override;

  typedef lc::locking_container <const std::function <bool()>, lc::dumb_lock> locked_action;

  locked_action action_callback;
};

#endif //action_hpp
