#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include <string.h>

#include "action-timer.hpp"
#include "locking-container.hpp"

// For linking of non-template locking-container symbols.
#include "locking-container.inc"

namespace {

template <class Type>
class blocking_strict_queue {
public:
  using queue_type = std::queue <Type>;

  blocking_strict_queue(unsigned int new_capacity) : terminated(false), capacity(new_capacity) {}

  void terminate() {
    std::unique_lock <std::mutex> local_lock(empty_lock);
    terminated = true;
    empty_wait.notify_all();
  }

  bool transfer_next_item(queue_type &from) {
    std::unique_lock <std::mutex> local_lock(empty_lock);
    if (terminated || queue.size() >= capacity || from.empty()) {
      return false;
    } else {
      queue.push(std::move(from.front()));
      from.pop();
      empty_wait.notify_all();
      return true;
    }
  }

  bool empty() {
    std::unique_lock <std::mutex> local_lock(empty_lock);
    return queue.empty();
  }

  bool dequeue(Type &removed) {
    while (!terminated) {
      std::unique_lock <std::mutex> local_lock(empty_lock);
      if (queue.empty()) {
        empty_wait.wait(local_lock);
        continue;
      } else {
        removed = std::move(queue.front());
        queue.pop();
        return true;
      }
    }
    return false;
  }

  ~blocking_strict_queue() {
    this->terminate();
  }

private:
  std::atomic <bool>       terminated;
  std::mutex               empty_lock;
  std::condition_variable  empty_wait;

  const unsigned int capacity;
  queue_type         queue;
};

template <class Type>
class queue_processor {
public:
  using queue_type = typename blocking_strict_queue <Type> ::queue_type;
  using locked_queue = lc::locking_container_base <queue_type>;

  queue_processor(std::function <bool(Type)> new_action, unsigned int new_capacity = 1) :
  destructor_called(false), action(new_action), queue(new_capacity) {}

  void start() {
    assert(!thread && !destructor_called);
    thread.reset(new std::thread([this] { this->processor_thread(); }));
  }

  bool transfer_next_item(locked_queue &from_queue) {
    auto write_queue = from_queue.get_write();
    assert(write_queue);
    // Should only block if processor_thread is in the process of removing an
    // item, but hasn't called action yet.
    return queue.transfer_next_item(*write_queue);
  }

  // TODO: Somehow pass the remaining data in the queue back to the caller?
  ~queue_processor() {
    destructor_called = true;
    queue.terminate();
    if (thread) {
      thread->join();
    }
  }

private:
  void processor_thread() {
    assert(action);
    while (!destructor_called) {
      Type removed;
      if (!queue.dequeue(removed)) {
        break;
      } else {
        if (!action(std::move(removed))) {
          break;
        }
      }
    }
    // Don't accept anything new. This is necessary if action returns false and
    // turns this processor into a zombie.
    // TODO: Should zombies actually be handled, or just left there as clutter?
    queue.terminate();
  }

  std::atomic <bool> destructor_called;
  std::unique_ptr <std::thread> thread;

  std::function <bool(Type)>   action;
  blocking_strict_queue <Type> queue;
};

} //namespace

int main(int argc, char *argv[]) {
  // NOTE: Must come before processors and actions!
  lc::locking_container <queue_processor <int> ::queue_type, lc::dumb_lock> queue;

  // NOTE: Must come before actions!
  std::unordered_map <std::string, std::unique_ptr<queue_processor <int>>> processors;

  action_timer <std::string> actions;
  actions.set_category("check_for_updates", 10.0);
  actions.start();

  {
    auto write_queue = queue.get_write();
    for (int i = 0; i < 10000; ++i) {
      write_queue->push(i);
    }
  }

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

    if (lambda <= 0.0) {
      // NOTE: Removing a processor will result in the queued data being lost!
      // 1. Remove the category from consideration.
      actions.set_category(category, lambda);
      // 2. Remove the category's action.
      actions.set_action(category, nullptr);
      // 3. Remove the catgory's processor.
      processors.erase(category);
    } else {
      // 1. Create and start a new processor.
      const unsigned int capacity = std::max(1, (int) lambda);
      std::unique_ptr<queue_processor <int>> processor(
        new queue_processor <int> ([category,lambda](int value) {
                                     std::cout << category << ": " << value << std::endl;
                                     std::this_thread::sleep_for(
                                       std::chrono::duration <double> (0.9 / lambda));
                                     return true;
                                   }, capacity));
      processor->start();
      auto *const processor_ptr = processor.get();
      action_timer <std::string> ::generic_action action(new sync_action([processor_ptr,category,&queue] {
                                                           if (!processor_ptr->transfer_next_item(queue)) {
                                                             std::cerr << category << ": FULL" << std::endl;
                                                           }
                                                         }));
      // 2. Replace (or add) the action.
      actions.set_action(category, std::move(action));
      // 3. Replace (or add) the processor.
      // NOTE: This must come after set_action so that the old action is removed
      // before its processor is destructed!
      // TODO: Figure out why std::move + emplace causes a deadlock.
      processors[category].swap(processor);
      // 4. Update (or add) the category for consideration.
      // TODO: Maybe this should be the only action if the processor already
      // exists? Might not work, since the processor's queue is based on lambda.
      actions.set_category(category, lambda);
    }
  }
}
