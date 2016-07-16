#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include <string.h>

#include "action-timer.hpp"
#include "queue-processor.hpp"
#include "locking-container.hpp"

// For linking of non-template locking-container symbols.
#include "locking-container.inc"

namespace {

using locked_processors =
  lc::locking_container <std::unordered_map <std::string, std::unique_ptr<queue_processor <int>>>, lc::dumb_lock>;

void zombie_cleanup(locked_processors &processors, action_timer <std::string> &actions) {
  auto write_processors = processors.get_write();
  assert(write_processors);
  for (auto current = write_processors->begin(); current != write_processors->end();) {
    if (!current->second || current->second->is_terminated()) {
      auto removed = current++;
      std::cerr << "Cleaning up " << removed->first << "." << std::endl;
      actions.set_category(removed->first, 0.0);
      actions.set_action(removed->first, nullptr);
      write_processors->erase(removed);
    } else {
      ++current;
    }
  }
}

} //namespace

int main(int argc, char *argv[]) {
  // NOTE: Must come before processors and actions!
  lc::locking_container <queue_processor <int> ::queue_type, lc::dumb_lock> queue;

  // NOTE: Must come before actions!
  locked_processors processors;

  action_timer <std::string> actions;
  actions.start();

  actions.set_category("zombie_cleanup", 1.0);
  // NOTE: Needs to be async_action to avoid a deadlock!
  action_timer <std::string> ::generic_action zombie_action(new async_action([&processors,&actions] {
    zombie_cleanup(processors, actions);
    return true;
  }));
  actions.set_action("zombie_cleanup", std::move(zombie_action));

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

    auto write_processors = processors.get_write();
    assert(write_processors);

    if (lambda <= 0.0) {
      // NOTE: Removing a processor will result in the queued data being lost!
      // 1. Remove the category from consideration.
      actions.set_category(category, 0.0);

      // 2. Remove the category's action.
      actions.set_action(category, nullptr);

      // 3. Remove the catgory's processor.
      write_processors->erase(category);
    } else {
      // 1. Create and start a new processor.
      // NOTE: Lambda is used as the capacity based on the probability of an
      // overflow in a fixed time interval.
      const unsigned int capacity = std::max(1, (int) lambda);
      std::unique_ptr<queue_processor <int>> processor(
        new queue_processor <int> ([category,lambda](int value) {
                                    if (!((value + 1) % 1021)) {
                                      // This isn't necessary; it just tests failure.
                                      std::cerr << category << ": FAILURE" << std::endl;
                                      return false;
                                    }
                                    std::cout << category << ": " << value << std::endl;
                                    std::this_thread::sleep_for(
                                      std::chrono::duration <double> (0.9 / lambda));
                                    return true;
                                  }, capacity));

      processor->start();
      auto *const processor_ptr = processor.get();

      action_timer <std::string> ::generic_action action(
        new sync_action([processor_ptr,category,&queue] {
                          if (!processor_ptr->transfer_next_item(queue)) {
                            // This isn't necessary; it just tests failure.
                            std::cerr << category << ": FULL" << std::endl;
                            processor_ptr->terminate();
                            return false;
                          }
                          return true;
                        }));

      // 2. Replace (or add) the action.
      actions.set_action(category, std::move(action));

      // 3. Replace (or add) the processor.
      // NOTE: This must come after set_action so that the old action is removed
      // before its processor is destructed!
      // TODO: Figure out why std::move + emplace causes a deadlock.
      (*write_processors)[category].swap(processor);

      // 4. Update (or add) the category for consideration.
      // TODO: Maybe this should be the only action if the processor already
      // exists? Might not work, since the processor's queue is based on lambda.
      actions.set_category(category, lambda);
    }
  }
}
