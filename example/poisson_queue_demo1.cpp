// poisson_queue_demo1.cpp

#include <iostream>
#include <string>
#include <thread>
#include "poisson-queue.hpp"
#include "locking-container.inc"

int main() {
  poisson_queue <std::string, int> queue;
  queue.start();

  // poisson_queue manages two types of actions:
  // - Regular actions, like what action_timer uses.
  // - Processor actions, which process items from the queue.


  // A zombie_cleanup action ensures that items are recovered if a processor
  // dies. This isn't automatically started by poisson_queue for numerous
  // reasons, the main one being that there is no logical way to determine what
  // the category label should be.
  action_timer <std::string> ::generic_action zombie_action(new async_action([&queue] {
    queue.zombie_cleanup();
    return true;
  }));
  queue.set_action("zombie_cleanup", std::move(zombie_action), 1.0);


  // A processor takes a value from the queue and does something with it. If the
  // processor returns false, the *modified* item is placed back in the queue
  // and the processor dies; otherwise, the item gets destructed and the
  // processor continues.
  // The lambda value should be approximately how many items the processor can
  // handle per second. Deciding on the queue size can be slightly complicated,
  // but it's mathematically well-defined at least.
  queue.set_processor("printer",
    [](int &value) {
      std::cout << "Processing " << value << "." << std::endl;
      // Note that this *doesn't* block the queue!
      std::this_thread::sleep_for(
        std::chrono::duration <double> (0.1));
      return true;
    }, 10.0, 10);


  // Adding items to the queue can be done from any thread, but it makes the
  // most sense from the thread that owns the queue.
  for (int i = 0; i < 100; ++i) {
    queue.queue_item(i);
  }

  // At the moment there isn't a clear way to wait for the poisson_queue to
  // finish doing what it's doing. This is because:
  // - A processor can die, causing items to be requeued.
  // - A processor could be finishing up, and exiting will kill it.
  while (!queue.empty()) {
    std::this_thread::sleep_for(
      std::chrono::duration <double> (0.1));
  }
  std::this_thread::sleep_for(
    std::chrono::duration <double> (1.0));
}
