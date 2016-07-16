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

#ifndef poisson_queue_hpp
#define poisson_queue_hpp

#include <cassert>
#include <functional>
#include <map>
#include <memory>

#include "action-timer.hpp"
#include "queue-processor.hpp"
#include "locking-container.hpp"


// NOTE: It's assumed that:
// - Type might not be copyable.
// - Type *is* movable.
// - Category has < and ==.
template <class Category, class Type>
class poisson_queue {
public:
  template <class ... Args>
  poisson_queue(Args ... args) : actions(args ...) {}

  void start();

  // TODO: Add a version that adds multiple items, e.g., with iterators.
  void queue_item(Type item);

  // The action doesn't use queue data.
  void set_action(const Category &category,
                  typename action_timer <Category> ::generic_action action,
                  double lambda);

  // The action uses queue data.
  void set_processor(const Category &category,
                     std::function <bool(Type)> process_function,
                     double lambda, unsigned int capacity);

  // Remove the action/processor associated with the category.
  void remove_action(const Category &category);

  // Cleans up processors that have terminated on their own. This is useful as
  // an action added with set_action.
  bool zombie_cleanup();

private:
  using locked_processors =
    lc::locking_container <std::map <Category, std::unique_ptr<queue_processor <Type>>>, lc::dumb_lock>;

  // NOTE: Must come before processors and actions!
  lc::locking_container <typename queue_processor <Type> ::queue_type, lc::dumb_lock> queue;
  // NOTE: Must come before actions!
  locked_processors processors;
  action_timer <Category> actions;
};


template <class Category, class Type>
void poisson_queue <Category, Type> ::start() {
  actions.start();
}

template <class Category, class Type>
void poisson_queue <Category, Type> ::queue_item(Type item) {
  auto write_queue = queue.get_write();
  assert(write_queue);
  write_queue->push_back(std::move(item));
}

template <class Category, class Type>
void poisson_queue <Category, Type> ::set_action(const Category &category,
                                                 typename action_timer <Category> ::generic_action action,
                                                 double lambda) {
  actions.set_action(category, std::move(action));
  actions.set_category(category, lambda);
  auto write_processors = processors.get_write();
  assert(write_processors);
  write_processors->erase(category);
}

template <class Category, class Type>
void poisson_queue <Category, Type> ::set_processor(const Category &category,
                                                    std::function <bool(Type)> process_function,
                                                    double lambda, unsigned int capacity) {
  // 1. Create and start a new processor.
  std::unique_ptr<queue_processor <Type>> processor(
    new queue_processor <Type> (process_function, capacity));
  processor->start();
  auto *const processor_ptr = processor.get();
  action_timer <std::string> ::generic_action action(
    new sync_action([this,processor_ptr] {
                      processor_ptr->transfer_next_item(queue);
                      return true;
                    }));

  // 2. Replace (or add) the action.
  actions.set_action(category, std::move(action));

  // 3. Replace (or add) the processor.
  // NOTE: This must come after set_action so that the old action is removed
  // before its processor is destructed!
  // TODO: Figure out why std::move + emplace causes a deadlock.
  auto write_processors = processors.get_write();
  assert(write_processors);
  // TODO: Add item recovery here.
  (*write_processors)[category].swap(processor);

  // 4. Update (or add) the category for consideration.
  // TODO: Maybe this should be the only action if the processor already
  // exists? Might not work, since the processor's queue is based on lambda.
  actions.set_category(category, lambda);
}

template <class Category, class Type>
void poisson_queue <Category, Type> ::remove_action(const Category &category) {
  // NOTE: Removing a processor will result in the queued data being lost!
  // 1. Remove the category from consideration.
  actions.set_category(category, 0.0);

  // 2. Remove the category's action.
  actions.set_action(category, nullptr);

  // 3. Remove the catgory's processor.
  auto write_processors = processors.get_write();
  assert(write_processors);
  // TODO: Add item recovery here.
  write_processors->erase(category);
}

template <class Category, class Type>
bool poisson_queue <Category, Type> ::zombie_cleanup() {
  typename queue_processor <Type> ::queue_type recovered;
  auto write_processors = processors.get_write();
  assert(write_processors);
  for (auto current = write_processors->begin(); current != write_processors->end();) {
    if (!current->second || current->second->is_terminated()) {
      auto removed = current++;
      if (removed->second) {
        removed->second->recover_lost_items(recovered);
      }
      actions.set_category(removed->first, 0.0);
      actions.set_action(removed->first, nullptr);
      write_processors->erase(removed);
    } else {
      ++current;
    }
  }
  if (!recovered.empty()) {
    auto write_queue = queue.get_write();
    assert(write_queue);
    while (!recovered.empty()) {
      // NOTE: Recovered items are *prepended* to the queue.
      write_queue->push_front(std::move(recovered.back()));
      recovered.pop_back();
    }
  }
  // This has no meaning, but is here so that this function can be added to
  // this object as an action.
  return true;
}

#endif //poisson_queue_hpp
