#include <functional>
#include <iostream>
#include <utility>

#include <stdio.h>
#include <string.h>

#include "action-timer.hpp"

// For linking of non-template locking-container symbols.
#include "locking-container.inc"

namespace {

class time_printer : public abstract_action {
public:
  time_printer(int count, std::function <void()> action) :
  max_count(count), stop_action(action), start_time(get_current_time()) {}

  void start() override {}

  void trigger_action() override {
    if (max_count > 0) {
      print_time();
      --max_count;
    } else {
      if (stop_action) {
        stop_action();
      }
    }
  }

private:
  void print_time() const {
    const auto current_time = get_current_time();
    std::cout << (current_time - start_time).count() / 1000000.0 << std::endl;
  }

  static std::chrono::microseconds get_current_time() {
    return std::chrono::duration_cast <std::chrono::microseconds> (
      std::chrono::high_resolution_clock::now().time_since_epoch());
  }

  int max_count;
  std::function <void()> stop_action;
  const std::chrono::microseconds start_time;
};

} //namespace

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "%s [lambda] [count]\n", argv[0]);
    return 1;
  }

  std::cout.setf(std::ios::scientific);
  std::cout.precision(10);

  double lambda = 1.0;
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

  action_timer <int> actions;
  actions.set_category(0, lambda);
  std::unique_ptr <abstract_action> action(new time_printer(count, [&actions] { actions.passive_stop(); }));
  actions.set_action(0, std::move(action));
  actions.start();

  while (!actions.is_stopping()) {
    std::this_thread::sleep_for(std::chrono::duration <double> (0.5));
  }
}
