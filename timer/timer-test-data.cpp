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

// Not thread-safe!
class time_printer {
public:
  time_printer(int count, std::function <void()> action) :
  max_count(count), stop_action(action), start_time(get_current_time()) {
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

  std::string get_output() const {
    return output.str();
  }

private:
  void print_time() {
    const auto current_time = get_current_time();
    output << (current_time - start_time).count() / 1000000.0 << std::endl;
  }

  static std::chrono::microseconds get_current_time() {
    return std::chrono::duration_cast <std::chrono::microseconds> (
      std::chrono::high_resolution_clock::now().time_since_epoch());
  }

  int max_count;
  std::ostringstream output;
  std::function <void()> stop_action;
  const std::chrono::microseconds start_time;
};

} //namespace

int main(int argc, char *argv[]) {
  if (argc != 3 && argc != 4) {
    fprintf(stderr, "%s [lambda] [count] (min sleep size)\n", argv[0]);
    return 1;
  }

  double lambda = 1.0, min_sleep_size = 0.001;
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
  actions.set_timer_factory([min_sleep_size] { return new precise_timer(0.01, min_sleep_size); });
  actions.set_category(0, lambda);
  time_printer printer(count, [&actions] { actions.async_stop(); });
  std::unique_ptr <abstract_action> action(new direct_action([&printer] { printer.action(); }));
  actions.set_action(0, std::move(action));
  actions.start();

  actions.wait_stopping();
  actions.stop();
  std::cout << printer.get_output();
}
