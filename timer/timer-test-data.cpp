#include <functional>
#include <iostream>
#include <sstream>
#include <utility>

#include <stdio.h>
#include <string.h>

#include "action-timer.hpp"

// For linking of non-template locking-container symbols.
#include "locking-container.inc"

namespace {

std::chrono::microseconds get_current_time() {
  return std::chrono::duration_cast <std::chrono::microseconds> (
    std::chrono::high_resolution_clock::now().time_since_epoch());
}

// Not thread-safe!
class time_printer {
public:
  time_printer(int count, std::function <void()> action) :
  max_count(count), holding_time(), stop_action(action), start_time(get_current_time()) {
    output.setf(std::ios::scientific);
    output.precision(10);
  }

  void action() {
    if (max_count > 0) {
      print_time();
      --max_count;
    } else {
      if (stop_action) {
        stop_action();
      }
    }
  }

  void append_time(double time) {
    holding_time = time;
  }

  std::string get_output() const {
    return output.str();
  }

private:
  void print_time() {
    const auto current_time = get_current_time();
    output << holding_time << ','
           << (current_time - start_time).count() / 1000000.0 << std::endl;
    start_time = current_time;
    holding_time = 0.0;
  }

  int max_count;
  double holding_time;
  std::ostringstream output;
  std::function <void()> stop_action;
  std::chrono::microseconds start_time;
};

// A very dubious class...
class recording_timer : public precise_timer {
public:
  recording_timer(double cancel_granularity, double min_sleep_size,
                  std::function <void(double)> time_callback) :
  precise_timer(cancel_granularity, min_sleep_size), send_time(time_callback) {}

  void sleep_for(double time, std::function <bool()> cancel) override {
    precise_timer::sleep_for(time, cancel);
    if (send_time) {
      send_time(time);
    }
  }

private:
  const std::function <void(double)> send_time;
};

} //namespace

int main(int argc, char *argv[]) {
  if (argc != 3 && argc != 4) {
    fprintf(stderr, "%s [lambda] [count] (min sleep size)\n", argv[0]);
    return 1;
  }

  double lambda = 1.0, min_sleep_size = 0.0;
  int    count = 0;
  char   error = 0;

  if (sscanf(argv[1], "%lf%c", &lambda, &error) != 1) {
    fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], argv[1]);
    return 1;
  }

  if (sscanf(argv[2], "%i%c", &count, &error) != 1) {
    fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], argv[2]);
    return 1;
  }

  if (argc > 3 && sscanf(argv[3], "%lf%c", &min_sleep_size, &error) != 1) {
    fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], argv[3]);
    return 1;
  }

  action_timer <int> actions;

  // Once the printer is executed count times, it stops the action_timer. Since
  // the action_timer owns the printer, async_stop is used to avoid a deadlock.
  time_printer printer(count, [&actions] {
                                actions.async_stop();
                              });

  // Yes, this is kind of a mess. When the sleep it started, it passes the
  // anticipated sleep time to the printer. Then, when the printer is called,
  // it records the expected time along side the actual time since the last
  // call.
  actions.set_timer_factory([min_sleep_size,&printer] {
    return new recording_timer(0.01, min_sleep_size,
                               [&printer](double t) {
                                 printer.append_time(t);
                              });
  });
  actions.set_category(0, lambda);

  std::unique_ptr <abstract_action> action(new sync_action([&printer] {
                                             printer.action();
                                             return true;
                                           }));
  actions.set_action(0, std::move(action));

  actions.start();

  actions.wait_stopping();
  actions.stop();
  std::cout << printer.get_output();
}
