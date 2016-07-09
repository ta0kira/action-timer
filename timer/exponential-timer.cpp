#include "exponential-timer.hpp"

precise_timer::precise_timer() : base_time() {
  this->mark();
}

void precise_timer::mark() {
  base_time = std::chrono::duration_cast <std::chrono::duration <double>> (
    std::chrono::high_resolution_clock::now().time_since_epoch());
}

void precise_timer::sleep_for(double time) {
  std::chrono::duration <double> target_duration(time);
  auto current_time = std::chrono::duration_cast <std::chrono::duration <double>> (
    std::chrono::high_resolution_clock::now().time_since_epoch());
  auto sleep_duration = target_duration - (current_time - base_time);
  // NOTE: If time is short enough then this will fall behind the actual time
  // until there is a sleep long enough to catch up.
  base_time += target_duration;
  std::this_thread::sleep_for(sleep_duration);
}

void thread_action::start() {
  thread.reset(new std::thread([=] { this->thread_loop(); }));
}

void thread_action::trigger_action() {
  {
    std::unique_lock <std::mutex> local_lock(action_lock);
    action_waiting = true;
  }
  action_wait.notify_all();
}

thread_action::~thread_action() {
  if (thread) {
    thread->detach();
  }
}

void thread_action::thread_loop() {
  while (true) {
    {
      std::unique_lock <std::mutex> local_lock(action_lock);
      if (!action_waiting) {
        action_wait.wait(local_lock);
        continue;
      }
      action_waiting = false;
    }
    this->action();
  }
}
