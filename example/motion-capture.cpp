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

#include <atomic>
#include <cassert>
#include <csignal>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include <stdio.h>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "action.hpp"
#include "action-timer.hpp"
#include "queue-processor.hpp"

#include "locking-container.hpp"

namespace {

struct camera_data {
  const std::chrono::microseconds time;
  const int                       number;
  const cv::Mat                   frame;
};

std::ostream &operator << (std::ostream &out, const camera_data &data) {
  return out << data.number << "@" << data.time.count() / 1000000.0;
}

using optional_camera_data  = std::unique_ptr <camera_data>;
using camera_data_processor = queue_processor_base <optional_camera_data>;

class process_camera_data : public queue_processor_base <optional_camera_data> {
public:
  process_camera_data(abstract_scaled_timer &new_timer, const std::string &new_window) :
  last_frame_changed(false), window(new_window), timer(new_timer) {}

private:
  bool process(optional_camera_data &data) override {
    assert(data);
    if (!data->frame.empty()) {
      cv::Mat frame          = this->preprocess(data->frame);
      const cv::Mat new_base = frame.clone();
      if (!last_frame.empty()) {
        frame = this->diff_since_last(frame);
        this->check_and_display(frame, data);
      }
      last_frame = new_base;
    }
    return true;
  }

  cv::Mat preprocess(const cv::Mat &orig_frame) const {
    cv::Mat frame = orig_frame, frame_temp;
    cv::cvtColor(frame, frame_temp, CV_BGR2GRAY);
    frame = frame_temp;
    cv::normalize(frame, frame_temp, 0, 255, cv::NORM_MINMAX);
    frame = frame_temp;
    cv::blur(frame, frame_temp, cv::Size(10, 10));
    return frame_temp;
  }

  cv::Mat diff_since_last(const cv::Mat &orig_frame) const {
    cv::Mat frame = orig_frame - last_frame, frame_temp;
    cv::Canny(frame, frame_temp, 1.0, 75.0);
    frame = frame_temp;
    cv::blur(frame, frame_temp, cv::Size(10, 10));
    frame = frame_temp;
    cv::normalize(frame, frame_temp, 0, 255, cv::NORM_MINMAX);
    cv::cvtColor(frame_temp, frame, CV_GRAY2BGR);
    return frame;
  }

  void check_and_display(const cv::Mat &orig_frame, const optional_camera_data &data) {
    cv::Mat frame = orig_frame.clone(), frame_temp;
    const bool this_frame_changed = cv::norm(frame) / (frame.rows * frame.cols) > 0.000001;
    if (this_frame_changed) {
      last_changed_time = data->time;
    }
    // Keep showing frames for 10s after the last detected motion.
    if (this_frame_changed || (data->time - last_changed_time).count() / 1000000.0 < 10.0) {
      if (this_frame_changed && !last_frame_changed) {
        std::cerr << "Change detected in " << *data << "." << std::endl;
        last_frame_changed = true;
        timer.set_scale(10.0);
      }
      // Make it red, then overlay it on the original frame.
      frame.reshape(1, frame.rows*frame.cols).col(0).setTo(cv::Scalar(0.0));
      frame.reshape(1, frame.rows*frame.cols).col(1).setTo(cv::Scalar(0.0));
      cv::scaleAdd(frame, 0.25, data->frame, frame_temp);
      cv::imshow(window, frame_temp);
    } else {
      // Just show the differences.
      if (!this_frame_changed && last_frame_changed) {
        std::cerr << "No change detected in " << *data << "." << std::endl;
        last_frame_changed = false;
        timer.set_scale(1.0);
      }
      cv::imshow(window, frame);
    }
    cv::waitKey(1);
  }

  bool                       last_frame_changed;
  std::chrono::microseconds  last_changed_time;
  const std::string          window;
  cv::Mat                    last_frame;
  abstract_scaled_timer     &timer;
};

class camera_reader : public async_action_base {
public:
  camera_reader(cv::VideoCapture new_capture, int new_number,
                camera_data_processor &new_processor,
                std::chrono::microseconds time) :
  capture(std::move(new_capture)), base_time(time), number(new_number),
  processor(new_processor) {}

  ~camera_reader() override {
    // Call terminate before ~async_action_base does so that the thread is
    // stopped before the members of this instance destruct.
    this->terminate();
  }

private:
  bool action() override {
    if (!capture.isOpened()) {
      std::cerr << "Camera " << number << " not present." << std::endl;
      return false;
    }
    cv::Mat frame;
    if (!capture.read(frame)) {
      std::cerr << "Failed to get frame from " << number << "." << std::endl;
      return false;
    }

    // Unfortunately, OpenCV seems to not have a portable way to detect if the
    // camera has been unplugged => as a heuristic, consider it crashed if we
    // get two identical frames in a row. (Probably not perfect.)
    if (!last_frame.empty() && last_frame.size() == frame.size()) {
      const auto diff = cv::sum(last_frame - frame);
      if (std::accumulate(diff.val, diff.val + 4, 0.0) == 0.0) {
        std::cerr << "Camera " << number << " seems to have crashed." << std::endl;
        return false;
      }
    }
    last_frame = frame.clone();

    const auto time =  std::chrono::duration_cast <std::chrono::microseconds> (
      std::chrono::high_resolution_clock::now().time_since_epoch()) - base_time;

    optional_camera_data new_data(new camera_data {
      time,
      number,
      frame,
    });

    if (!processor.enqueue(new_data, false)) {
      std::cerr << "Unable to queue " << number << " sample: " << *new_data << std::endl;
      if (processor.is_terminated()) {
        std::cerr << "Frame processor has stopped." << std::endl;
        return false;
      }
    }

    return true;
  }

  cv::Mat          last_frame;
  cv::VideoCapture capture;

  const std::chrono::microseconds  base_time;
  const int                        number;
  camera_data_processor           &processor;

};

class frame_processor {
public:
  frame_processor(const std::string &name) :
  base_time(std::chrono::duration_cast <std::chrono::microseconds> (
    std::chrono::high_resolution_clock::now().time_since_epoch())),
  my_name(name), processor(timer, my_name) {}

  void start() {
    // NOTE: One or both of these should catch a repeated call to start.
    timer.start();
    processor.start();
    std::cerr << "Creating window " << my_name << "." << std::endl;
    cv::namedWindow(my_name, CV_WINDOW_AUTOSIZE);
  }

  void create_camera(int number, double lambda, int width = 0, int height = 0) {
    if (!timer.action_exists(number)) {
      std::cerr << "Creating camera " << number << " with lambda "
                << lambda << "." << std::endl;
      cv::VideoCapture capture(number);
      if (width && height) {
        capture.set(CV_CAP_PROP_FRAME_WIDTH,  width);
        capture.set(CV_CAP_PROP_FRAME_HEIGHT, height);
      }
      action_timer <int> ::generic_action camera_action(
        new camera_reader(std::move(capture), number, processor, base_time));
      timer.set_action(number, std::move(camera_action));
      timer.set_timer(number, lambda);
    } else {
      std::cerr << "Camera " << number << " is running." << std::endl;
    }
  }

  void wait_empty() {
    timer.wait_empty();
  }

  ~frame_processor() {
    // Call terminate before ~queue_processor does so that the thread is stopped
    // before the members of this instance destruct.
    processor.terminate();
    // Ditto, but for ~action_timer.
    timer.stop();
    cv::destroyWindow(my_name);
  }

private:
  frame_processor(const frame_processor&) = delete;
  frame_processor(frame_processor&&) = delete;
  frame_processor &operator = (const frame_processor&) = delete;
  frame_processor &operator = (frame_processor&&) = delete;

  const std::chrono::microseconds base_time;
  const std::string               my_name;

  action_timer <int>  timer;
  process_camera_data processor;
};

} // namespace

int main(int argc, char *argv[]) {
  for (auto signal : { 2, 3, 6, 11, 15 }) {
    // Forces release of hardware resources. Without this, the camera(s) might
    // not be available again without unplugging/replugging.
    std::signal(signal, &exit);
  }

  if (argc != 3 && argc != 5) {
    fprintf(stderr, "%s [lambda] [camera num] (width) (height)\n", argv[0]);
    return 1;
  }

  frame_processor monitor("camera_monitor");

  std::istringstream parse;
  std::string extra;

  double lambda = 0.0;
  int number = 0;
  int width = 0, height = 0;

  parse.str(argv[1]);
  parse.clear();
  if (!(parse >> lambda) || parse >> extra || lambda <= 0.0) {
    fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], argv[1]);
    return 1;
  }

  parse.str(argv[2]);
  parse.clear();
  if (!(parse >> number) || parse >> extra) {
    fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], argv[2]);
    return 1;
  }

  if (argc == 5) {
    parse.str(argv[3]);
    parse.clear();
    if (!(parse >> width) || parse >> extra) {
      fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], argv[3]);
      return 1;
    }
    parse.str(argv[4]);
    parse.clear();
    if (!(parse >> height) || parse >> extra) {
      fprintf(stderr, "%s: Failed to parse \"%s\".\n", argv[0], argv[4]);
      return 1;
    }
  }

  monitor.create_camera(number, lambda, width, height);

  monitor.start();
  monitor.wait_empty();
}
