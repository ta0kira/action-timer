#include "action-timer.hpp"

void action_timer::set_category(const std::string &category, double lambda) {
  locked_exponential_categorical::write_proxy category_write = locked_categories.get_write();
  assert(category_write);
  if (lambda > 0) {
    category_write->set_category(category, lambda);
  } else {
    category_write->clear_category(category);
  }
}

void action_timer::set_action(const std::string &category, generic_action action) {
  if (action) {
    action->start();
  }
  locked_action_map::write_proxy action_write = locked_actions.get_write();
  assert(action_write);
  if (action) {
    (*action_write)[category].reset(action.release());
  } else {
    action_write->erase(category);
  }
}

void action_timer::start() {
  thread.reset(new std::thread([=] { this->thread_loop(); }));
}

void action_timer::join() {
  if (thread) {
    thread->join();
  }
}

action_timer::~action_timer() {
  if (thread) {
    thread->detach();
  }
}

void action_timer::thread_loop() {
  while (true) {
    locked_exponential_categorical::read_proxy category_read = locked_categories.get_read();
    assert(category_read);
    // NOTE: Need to copy category to avoid a race condition!
    const std::string category = category_read->uniform_to_category(uniform(generator));
    const double       time    = category_read->uniform_to_time(uniform(generator));
    category_read.clear();

    timer.sleep_for(time);

    locked_action_map::write_proxy action_write = locked_actions.get_write();
    assert(action_write);
    auto existing = action_write->find(category);
    if (existing != action_write->end() && existing->second) {
      existing->second->trigger_action();
    }
  }
}
