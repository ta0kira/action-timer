#include "action-timer.hpp"

precise_timer::precise_timer(double cancel_granularity,
                             double max_precision) :
  sleep_granularity(std::chrono::duration <double> (cancel_granularity)),
  spinlock_limit(std::chrono::duration <double> (max_precision)), base_time() {
  this->mark();
}

void precise_timer::mark() {
  base_time = std::chrono::duration_cast <std::chrono::duration <double>> (
    std::chrono::high_resolution_clock::now().time_since_epoch());
}

void precise_timer::sleep_for(double time, std::function <bool()> cancel) {
  const std::chrono::duration <double> target_duration(time);
  base_time += target_duration;
  bool canceled = false;

  for (; !canceled; canceled = cancel && cancel()) {
    const auto current_time = std::chrono::duration_cast <std::chrono::duration <double>> (
      std::chrono::high_resolution_clock::now().time_since_epoch());
    if (current_time >= base_time) {
      break;
    }
    if (base_time - current_time < spinlock_limit) {
      this->spinlock_finish();
      break;
    } else if (base_time - current_time < sleep_granularity) {
      std::this_thread::sleep_for((base_time - current_time) - spinlock_limit);
    } else {
      std::this_thread::sleep_for(sleep_granularity - spinlock_limit);
    }
  }

  if (canceled) {
    this->mark();
  }
}

void precise_timer::spinlock_finish() const {
  while (true) {
    const auto current_time = std::chrono::duration_cast <std::chrono::duration <double>> (
      std::chrono::high_resolution_clock::now().time_since_epoch());
    if (current_time >= base_time) {
      break;
    }
  }
}

void thread_action::set_action(std::function <void()> new_action) {
  action = new_action;
}

void thread_action::start() {
  if (!thread) {
    thread.reset(new std::thread([this] { this->thread_loop(); }));
  }
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

void direct_action::set_action(std::function <void()> new_action) {
  auto write_action = action.get_write();
  assert(write_action);
  write_action->swap(new_action);
}

void direct_action::trigger_action() {
  auto read_action = action.get_write();
  assert(read_action);
  if (*read_action) {
    (*read_action)();
  }
}

// NOTE: This waits for an ongoing action to complete.
direct_action::~direct_action() {
  auto write_action = action.get_write();
}
