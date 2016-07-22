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

#ifndef queue_processor_hpp
#define queue_processor_hpp

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <thread>
#include <utility>


template <class Type>
class blocking_strict_queue {
public:
  using queue_type = std::list <Type>;

  blocking_strict_queue(unsigned int new_capacity) :
  terminated(false), in_progress(0), capacity(new_capacity) {}

  void terminate();
  bool is_terminated() const;

  bool empty();
  bool full();

  bool enqueue(Type &added,   bool block = true);
  bool dequeue(Type &removed, bool block = true);

  bool requeue_item(Type item);
  void done_with_item();

  void recover_lost_items(queue_type &to_queue);

  ~blocking_strict_queue();

private:
  std::atomic <bool>      terminated;
  std::mutex              empty_lock;
  std::condition_variable empty_wait;

  unsigned int       in_progress;
  const unsigned int capacity;
  queue_type         queue;
};

template <class Type>
class queue_processor {
public:
  using queue_type = typename blocking_strict_queue <Type> ::queue_type;
  using locked_queue = lc::locking_container_base <queue_type>;

  queue_processor(std::function <bool(Type&)> new_action, unsigned int new_capacity = 1) :
  terminated(false), action(std::move(new_action)), queue(new_capacity) {}

  void start();
  void terminate();
  bool is_terminated() const;

  bool enqueue(Type &added, bool block = false);
  bool transfer_next_item(queue_type &from_queue, bool block = false);
  void recover_lost_items(queue_type &to_queue);

  // TODO: Somehow pass the remaining data in the queue back to the caller?
  ~queue_processor();

private:
  void processor_thread();

  std::atomic <bool> terminated;
  std::unique_ptr <std::thread> thread;

  const std::function <bool(Type&)> action;

  blocking_strict_queue <Type> queue;
};


template <class Type>
void blocking_strict_queue <Type> ::terminate() {
  std::unique_lock <std::mutex> local_lock(empty_lock);
  terminated = true;
  empty_wait.notify_all();
}

template <class Type>
bool blocking_strict_queue <Type> ::is_terminated() const {
  return terminated;
}

template <class Type>
bool blocking_strict_queue <Type> ::empty() {
  std::unique_lock <std::mutex> local_lock(empty_lock);
  return queue.empty();
}

template <class Type>
bool blocking_strict_queue <Type> ::full() {
  std::unique_lock <std::mutex> local_lock(empty_lock);
  return queue.size() + in_progress >= capacity;
}

template <class Type>
bool blocking_strict_queue <Type> ::enqueue(Type &added, bool block) {
  while (!terminated) {
    std::unique_lock <std::mutex> local_lock(empty_lock);
    if (queue.size() + in_progress >= capacity) {
      if (block) {
        empty_wait.wait(local_lock);
      } else {
        return false;
      }
      continue;
    } else {
      queue.push_back(std::move(added));
      empty_wait.notify_all();
      return true;
    }
  }
  return false;
}

template <class Type>
bool blocking_strict_queue <Type> ::dequeue(Type &removed, bool block) {
  while (!terminated) {
    std::unique_lock <std::mutex> local_lock(empty_lock);
    if (queue.empty()) {
      if (block) {
        empty_wait.wait(local_lock);
      } else {
        return false;
      }
      continue;
    } else {
      ++in_progress;
      removed = std::move(queue.front());
      queue.pop_front();
      return true;
    }
  }
  return false;
}

template <class Type>
bool blocking_strict_queue <Type> ::requeue_item(Type item) {
  std::unique_lock <std::mutex> local_lock(empty_lock);
  assert(in_progress > 0);
  if (terminated || queue.size() + --in_progress >= capacity) {
    return false;
  } else {
    queue.push_front(std::move(item));
    empty_wait.notify_all();
    return true;
  }
}

template <class Type>
void blocking_strict_queue <Type> ::done_with_item() {
  std::unique_lock <std::mutex> local_lock(empty_lock);
  assert(in_progress > 0);
  --in_progress;
}

template <class Type>
void blocking_strict_queue <Type> ::recover_lost_items(queue_type &to_queue) {
  assert(this->is_terminated());
  while (!queue.empty()) {
    to_queue.push_back(std::move(queue.front()));
    queue.pop_front();
  }
}

template <class Type>
blocking_strict_queue <Type> ::~blocking_strict_queue() {
  this->terminate();
}


template <class Type>
void queue_processor <Type> ::start() {
  assert(!thread && !this->is_terminated());
  thread.reset(new std::thread([this] { this->processor_thread(); }));
}

template <class Type>
void queue_processor <Type> ::terminate() {
  terminated = true;
  // NOTE: This causes the thread to continue if blocked.
  queue.terminate();
}

template <class Type>
bool queue_processor <Type> ::is_terminated() const {
  return terminated || queue.is_terminated();
}

template <class Type>
bool queue_processor <Type> ::enqueue(Type &added, bool block) {
  return queue.enqueue(added, block);
}

template <class Type>
bool queue_processor <Type> ::transfer_next_item(queue_type &from_queue, bool block) {
  if (this->is_terminated()) {
    return false;
  }
  if (from_queue.empty()) {
    return false;
  }
  Type temp(std::move(from_queue.front()));
  from_queue.pop_front();
  if (queue.enqueue(temp, block)) {
    return true;
  } else {
    from_queue.push_front(std::move(temp));
    return false;
  }
}

template <class Type>
void queue_processor <Type> ::recover_lost_items(queue_type &to_queue) {
  assert(this->is_terminated());
  queue.recover_lost_items(to_queue);
}

template <class Type>
queue_processor <Type> ::~queue_processor() {
  this->terminate();
  if (thread) {
    thread->join();
  }
}

template <class Type>
void queue_processor <Type> ::processor_thread() {
  assert(action);
  while (!this->is_terminated()) {
    Type removed;
    if (!queue.dequeue(removed)) {
      break;
    } else {
      if (!action(removed)) {
        queue.requeue_item(std::move(removed));
        break;
      } else {
        queue.done_with_item();
      }
    }
  }
  // Don't accept anything new. This is necessary if action returns false and
  // turns this processor into a zombie.
  this->terminate();
}

#endif //queue_processor_hpp
