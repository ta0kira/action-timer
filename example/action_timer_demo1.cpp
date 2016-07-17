// action_timer_demo1.cpp

#include <iostream>
#include "action-timer.hpp"
#include "locking-container.inc"

int main() {
  action_timer <char> timer;
  timer.start();


  // This action will happen ~10 times per second.
  timer.set_category('A', 10.0);

  // sync_action causes the timer to block while the action is being executed.
  action_timer <char> ::generic_action A_action(new sync_action(
    [] {
      std::cout << "Executing A." << std::endl;
      return true;
    }));

  timer.set_action('A', std::move(A_action));


  // This action will happen ~0.1 times per second.
  timer.set_category('B', 0.1);

  // async_action *doesn't* cause the timer to block. This is helpful for long-
  // running actions, and for actions that are going to change the state of the
  // action_timer.
  action_timer <char> ::generic_action B_action(new async_action(
    [&timer] {
      std::cout << "B is stopping the timer." << std::endl;
      timer.async_stop();
    }));

  timer.set_action('B', std::move(B_action));


  // This action will happen ~0.5 times per second.
  timer.set_category('C', 0.5);

  // Returning false will cause the action_timer to remove the action. This has
  // no effect on the other actions that are still registered.
  action_timer <char> ::generic_action C_action(new sync_action(
    [&timer] {
      std::cout << "C has failed." << std::endl;
      return false;
    }));

  timer.set_action('C', std::move(C_action));

  timer.wait_stopping();
}
