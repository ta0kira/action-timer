/* -----------------------------------------------------------------------------
Copyright (c) 2016, Google Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
----------------------------------------------------------------------------- */

// Author: Kevin P. Barry [ta0kira@gmail.com] [kevinbarry@google.com]

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>

#include <string.h>

#include "action.hpp"
#include "helpers.hpp"
#include "action-timer.hpp"
#include "queue-processor.hpp"

namespace {

struct sensor_data {
  const std::chrono::microseconds time;
  const std::string               label;
};

std::ostream &operator << (std::ostream &out, const sensor_data &data) {
  return out << data.label << "@" << data.time.count() / 1000000.0;
}

using optional_sensor_data  = std::unique_ptr <sensor_data>;
using sensor_data_processor = queue_processor <optional_sensor_data>;

bool process_sensor_data(action_timer <std::string> &timer,
                         optional_sensor_data &data) {
  assert(data);
  std::cerr << "Processing sample: " << *data << std::endl;
  const double time = data->time.count() / 1000000.0;
  const double current_scale = timer.get_scale();

  // Arbitrary "alert" condition, which causes timing to scale by 25x. This
  // simulates a situation where incoming sensor data is much more time-
  // sensitive than usual.
  const bool alert_state = fmod(time, 10) < 3.0;
  if (alert_state) {
    if (current_scale != 25.0) {
      std::cerr << "In alert state." << std::endl;
      timer.set_scale(25.0);
    }
  } else {
    if (current_scale != 1.0) {
      std::cerr << "In normal state." << std::endl;
      timer.set_scale(1.0);
    }
  }
  return true;
}

class sensor_reader : public async_action_base {
public:
  sensor_reader(const std::string &new_label, sensor_data_processor &new_processor,
                std::chrono::microseconds time) :
  base_time(time), label(new_label), processor(new_processor) {}

  ~sensor_reader() override {
    // Call terminate before ~async_action_base does so that the thread is
    // stopped before the members of this instance destruct.
    this->terminate();
  }

private:
  bool action() override {
    optional_sensor_data new_data(new sensor_data {
      std::chrono::duration_cast <std::chrono::microseconds> (
        std::chrono::high_resolution_clock::now().time_since_epoch()) - base_time,
      label
    });
    if (!processor.enqueue(new_data, false)) {
      std::cerr << "Unable to queue sample: " << *new_data << std::endl;
    }
    return true;
  }

  const std::chrono::microseconds  base_time;
  const std::string                label;
  sensor_data_processor           &processor;
};

bool find_new_sensors(action_timer <std::string> &timer,
                      sensor_data_processor &processor,
                      std::chrono::microseconds base_time,
                      const std::string &my_name) {
  if (processor.is_terminated()) {
    std::cerr << "Processor is terminated => stopping timer." << std::endl;
    return false;
  }

  std::string input;
  if (!std::getline(std::cin, input)) {
    std::cerr << "Unable to check for new sensors => stopping timer." << std::endl;
    timer.async_stop();
    return false;
  }

  double      lambda;
  std::string category;
  if (!parse_lambda_and_label(input, lambda, category)) {
    // Parsing error, but not fatal.
    return true;
  }

  if (category == my_name) {
    if (lambda <= 0.0) {
      std::cerr << "Refusing to remove " << category << "." << std::endl;
    } else {
      std::cerr << "Changing timing for " << category << "." << std::endl;
      timer.set_timer(category, lambda);
    }
    return true;
  }

  if (lambda <= 0.0) {
    if (timer.action_exists(category)) {
      std::cerr << "Stopping sensor " << category << "." << std::endl;
      timer.erase_action(category);
      timer.erase_timer(category);
    }
    return true;
  }

  if (timer.action_exists(category)) {
    std::cerr << "Changing timing for sensor " << category << "." << std::endl;
    // NOTE: The sensor could die between checking and updating, but that
    // shouldn't matter, since we check via action_exists, i.e., if it comes up
    // again then it will be restarted.
    timer.set_timer(category, lambda);
    return true;
  }

  std::cerr << "Starting sensor " << category << "." << std::endl;
  action_timer <std::string> ::generic_action sensor(new sensor_reader(category, processor, base_time));
  timer.set_timer(category, lambda);
  timer.set_action(category, std::move(sensor));
  return true;
}

} // namespace

int main(int argc, char *argv[]) {
  action_timer <std::string> timer;
  timer.start();

  const auto base_time = std::chrono::duration_cast <std::chrono::microseconds> (
    std::chrono::high_resolution_clock::now().time_since_epoch());

  sensor_data_processor processor([&timer](optional_sensor_data &data) {
    return process_sensor_data(timer, data);
  });
  processor.start();

  const std::string find_new_sensors_label("find_new_sensors");
  action_timer <std::string> ::generic_action find_new_sensors_action(
    new async_action([&timer,&processor,base_time,&find_new_sensors_label] {
      return find_new_sensors(timer, processor, base_time, find_new_sensors_label);
    }));
  timer.set_action(find_new_sensors_label, std::move(find_new_sensors_action));
  timer.set_timer(find_new_sensors_label, 1.0);

  timer.wait_stopping();
}
