#include "action-timer.hpp"

const std::chrono::duration <double> precise_timer::sleep_granularity(0.01);

precise_timer::precise_timer() : base_time() {
  this->mark();
}

void precise_timer::mark() {
  base_time = std::chrono::duration_cast <std::chrono::duration <double>> (
    std::chrono::high_resolution_clock::now().time_since_epoch());
}

void precise_timer::sleep_for(double time, std::function <bool()> cancel) {
  const std::chrono::duration <double> target_duration(time);
  base_time += target_duration;

  while (!cancel || !cancel()) {
    const auto current_time = std::chrono::duration_cast <std::chrono::duration <double>> (
      std::chrono::high_resolution_clock::now().time_since_epoch());
    if (base_time - current_time < sleep_granularity) {
      std::this_thread::sleep_for(base_time - current_time);
      break;
    } else {
      std::this_thread::sleep_for(sleep_granularity);
    }
  }
}

void thread_action::set_action(std::function <void()> new_action) {
  action = new_action;
}

void thread_action::start() {
  thread.reset(new std::thread([this] { this->thread_loop(); }));
}

void thread_action::trigger_action() {
  std::unique_lock <std::mutex> local_lock(action_lock);
  action_waiting = true;
  action_wait.notify_all();
}

thread_action::~thread_action() {
  destructor_called = true;
  action_wait.notify_all();
  if (thread) {
    thread->join();
  }
}

void thread_action::thread_loop() {
  while (!destructor_called) {
    {
      std::unique_lock <std::mutex> local_lock(action_lock);
      if (!action_waiting) {
        action_wait.wait(local_lock);
        continue;
      }
      action_waiting = false;
    }
    if (action && !destructor_called) {
      action();
    }
  }
}
