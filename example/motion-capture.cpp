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

class camera_reader {
public:
  camera_reader(int new_number, const std::string &new_window,
                int new_width, int new_height) :
  recording(false), terminated(false), number(new_number),
  width(new_width), height(new_height), window(new_window),
  blank_image(cv::Mat::zeros(height, width, CV_8UC3)),
  base_time(std::chrono::duration_cast <std::chrono::microseconds> (
    std::chrono::high_resolution_clock::now().time_since_epoch())) {}

  void start() {
    assert(!thread);
    cv::namedWindow(window);
    this->clear_window();
    thread.reset(new std::thread([this] { this->capture_thread(); }));
  }

  optional_camera_data get_frame() {
    {
      std::unique_lock <std::mutex> local_lock(record_lock);
      if (terminated) {
        std::cerr << "Camera " << number << " is stopped." << std::endl;
        return nullptr;
      }
      if (!recording) {
      std::cerr << "Getting static frame from " << number << "." << std::endl;
        if (!this->capture_frame(true)) {
          local_lock.unlock();
          this->terminate();
          return nullptr;
        }
      } else {
        std::cerr << "Getting dynamic frame from " << number << "." << std::endl;
      }
    }
    auto frame_read = last_frame.get_read();
    assert(frame_read);
    assert(*frame_read);
    return optional_camera_data(new camera_data(**frame_read));
  }

  void start_recording() {
    std::cerr << "Starting recording for " << number << "." << std::endl;
    assert(thread);
    std::unique_lock <std::mutex> local_lock(record_lock);
    recording = true;
    this->open_camera();
    record_wait.notify_all();
  }

  void stop_recording() {
    std::cerr << "Stopping recording for " << number << "." << std::endl;
    assert(thread);
    std::unique_lock <std::mutex> local_lock(record_lock);
    recording = false;
    this->close_camera();
    record_wait.notify_all();
    this->clear_window();
  }

  ~camera_reader() {
    this->terminate();
    if (thread) {
      thread->join();
    }
  }

private:
  void terminate() {
    std::unique_lock <std::mutex> local_lock(record_lock);
    terminated = true;
    record_wait.notify_all();
  }

  void clear_window() {
    // Clear the window.
    if (!blank_image.empty()) {
      std::cerr << "Clearing window for " << number << "." << std::endl;
      cv::imshow(window, blank_image);
      cv::waitKey(1);
    }
  }

  void open_camera() {
    capture.open(number);
    if (width && height) {
      capture.set(CV_CAP_PROP_FRAME_WIDTH,  width);
      capture.set(CV_CAP_PROP_FRAME_HEIGHT, height);
    }
  }

  void close_camera() {
    capture.release();
  }

  bool capture_frame(bool is_static) {
    // NOTE: Assumes that record_lock is locked!
    if (is_static) {
      this->open_camera();
    }

    if (!capture.isOpened()) {
      std::cerr << "Camera " << number << " not present." << std::endl;
      return false;
    }
    cv::Mat frame;
    if (!capture.read(frame)) {
      std::cerr << "Failed to get frame from " << number << "." << std::endl;
      return false;
    }

    // This makes sure that frames aren't buffered when we aren't recording;
    // otherwise, there will be a lag in frame processing.
    if (is_static) {
      this->close_camera();
    }

    const auto time =  std::chrono::duration_cast <std::chrono::microseconds> (
      std::chrono::high_resolution_clock::now().time_since_epoch()) - base_time;

    optional_camera_data new_data(new camera_data {
      time,
      number,
      frame,
    });

    auto frame_write = last_frame.get_write();
    assert(frame_write);

    // Unfortunately, OpenCV seems to not have a portable way to detect if the
    // camera has been unplugged => as a heuristic, consider it crashed if we
    // get two identical frames in a row. (Probably not perfect.)
    if (*frame_write && !(*frame_write)->frame.empty() &&
        (*frame_write)->frame.size() == frame.size()) {
      const auto diff = cv::sum((*frame_write)->frame - frame);
      if (std::accumulate(diff.val, diff.val + 4, 0.0) == 0.0) {
        std::cerr << "Camera " << number << " seems to have crashed." << std::endl;
        return false;
      }
    }

    *frame_write = std::move(new_data);
    return true;
  }

  void capture_thread() {
    while (!terminated) {
      {
        std::unique_lock <std::mutex> local_lock(record_lock);
        if (!recording) {
          record_wait.wait(local_lock);
          if (terminated) {
            break;
          }
          continue;
        }
        cv::Mat frame;
        if (!this->capture_frame(false)) {
          break;
        }
      }
      auto frame_read = last_frame.get_read();
      assert(frame_read);
      if (*frame_read && !(*frame_read)->frame.empty()) {
        cv::Mat with_label = (*frame_read)->frame.clone();
        std::ostringstream formatted;
        formatted.precision(1);
        formatted << std::fixed << **frame_read;
        int baseline = 0;
        const cv::Point corner = cv::Point(10, height - 10);
        const double font_size = 1.0, text_thickness = 1.0;
        const cv::Point size = cv::Point(
          cv::getTextSize(formatted.str(), cv::FONT_HERSHEY_DUPLEX,
                          font_size, text_thickness, &baseline));
        const cv::Point corner2 = cv::Point(corner.x + size.x, corner.y - size.y);
        cv::rectangle(with_label, corner, corner2, cv::Scalar(31, 31, 31), CV_FILLED);
        cv::rectangle(with_label, corner, corner2, cv::Scalar(0, 0, 0), 2);
        cv::putText(with_label, formatted.str(), corner,
                    cv::FONT_HERSHEY_DUPLEX, font_size, cv::Scalar(255, 255, 255),
                    text_thickness);
        cv::imshow(window, with_label);
        cv::waitKey(1);
      }
    }
    terminated = true;
  }

  std::unique_ptr <std::thread> thread;

  std::mutex              record_lock;
  std::condition_variable record_wait;
  std::atomic <bool>      recording, terminated;

  cv::VideoCapture capture;
  lc::locking_container <optional_camera_data, lc::rw_lock> last_frame;

  const int                        number, width, height;
  const std::string                window;
  const cv::Mat                    blank_image;
  const std::chrono::microseconds  base_time;
};

class motion_detector : public queue_processor_base <optional_camera_data> {
public:
  motion_detector(camera_reader &new_reader) :
  last_frame_changed(false), reader(new_reader) {}

private:
  bool process(optional_camera_data &data) override {
    assert(data);
    if (!data->frame.empty()) {
      std::cerr << "Processing frame " << *data << "." << std::endl;
      cv::Mat frame          = this->preprocess(data->frame);
      const cv::Mat new_base = frame.clone();
      if (!last_frame.empty()) {
        frame = this->diff_since_last(frame);
        this->check_for_motion(frame, data);
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

  void check_for_motion(const cv::Mat &orig_frame, const optional_camera_data &data) {
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
        reader.start_recording();
      }
    } else {
      // Just show the differences.
      if (!this_frame_changed && last_frame_changed) {
        std::cerr << "No change detected in " << *data << "." << std::endl;
        last_frame_changed = false;
        reader.stop_recording();
      }
    }
  }

  bool                       last_frame_changed;
  std::chrono::microseconds  last_changed_time;
  cv::Mat                    last_frame;
  camera_reader             &reader;
};

bool queue_next_frame(camera_reader &reader, motion_detector &detector) {
  optional_camera_data next_frame = reader.get_frame();
  if (!next_frame) {
    return false;
  } else {
    return detector.enqueue(next_frame, false) || !detector.is_terminated();
  }
}

} // namespace

int main(int argc, char *argv[]) {
  for (auto signal : { 2, 3, 6, 15 }) {
    // Forces release of hardware resources. Without this, the camera(s) might
    // not be available again without unplugging/replugging.
    std::signal(signal, &exit);
  }

  if (argc != 3 && argc != 5) {
    fprintf(stderr, "%s [lambda] [camera num] (width) (height)\n", argv[0]);
    return 1;
  }

  std::istringstream parse;
  std::string extra;

  double lambda = 0.0;
  int number = 0;
  int width = 640, height = 480;

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

  action_timer <int> timer;
  timer.start();

  camera_reader camera(number, "camera_monitor", width, height);
  camera.start();

  motion_detector detector(camera);
  detector.start();

  abstract_scaled_timer::generic_action detector_action(
    new async_action([&camera,&detector] {
      return queue_next_frame(camera, detector);
    }));
  timer.set_action(number, std::move(detector_action));
  timer.set_timer(number, lambda);

  timer.wait_empty();
}
