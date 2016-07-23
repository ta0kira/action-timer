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

#include "action.hpp"

void async_action_base::start() {
  if (!thread) {
    thread.reset(new std::thread([this] { this->thread_loop(); }));
  }
}

bool async_action_base::trigger_action() {
  std::unique_lock <std::mutex> local_lock(action_lock);
  if (!action_error) {
    action_waiting = true;
  }
  action_wait.notify_all();
  return !destructor_called && !action_error;
}
  void terminate();

async_action_base::~async_action_base() {
  this->terminate();
}

void async_action_base::terminate() {
  destructor_called = true;
  action_wait.notify_all();
  if (thread) {
    thread->join();
    thread.reset();
  }
}

void async_action_base::thread_loop() {
  while (!destructor_called) {
    {
      std::unique_lock <std::mutex> local_lock(action_lock);
      if (!action_waiting) {
        action_wait.wait(local_lock);
        continue;
      }
      action_waiting = false;
    }
    if (!destructor_called) {
       if (!this->action()) {
         action_error = true;
         break;
       }
    }
  }
}

bool sync_action::action() {
  auto read_action = action_callback.get_read();
  assert(read_action);
  assert(*read_action);
  return (*read_action)();
}

// NOTE: This waits for an ongoing action to complete.
sync_action::~sync_action() {
  auto write_action = action_callback.get_write();
}
