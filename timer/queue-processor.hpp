#ifndef queue_processor_hpp
#define queue_processor_hpp

#include <atomic>
#include <functional>
#include <memory>
#include <queue>
#include <thread>
#include <utility>


template <class Type>
class blocking_strict_queue {
public:
  using queue_type = std::queue <Type>;

  blocking_strict_queue(unsigned int new_capacity) :
  terminated(false), capacity(new_capacity) {}

  void terminate();
  bool is_terminated() const;

  bool transfer_next_item(queue_type &from);
  bool empty();
  bool dequeue(Type &removed);

  ~blocking_strict_queue();

private:
  std::atomic <bool>      terminated;
  std::mutex              empty_lock;
  std::condition_variable empty_wait;

  const unsigned int capacity;
  queue_type         queue;
};

template <class Type>
class queue_processor {
public:
  using queue_type = typename blocking_strict_queue <Type> ::queue_type;
  using locked_queue = lc::locking_container_base <queue_type>;

  queue_processor(std::function <bool(Type)> new_action, unsigned int new_capacity = 1) :
  terminated(false), action(new_action), queue(new_capacity) {}

  void start();
  void terminate();
  bool is_terminated() const;

  bool transfer_next_item(locked_queue &from_queue);

  // TODO: Somehow pass the remaining data in the queue back to the caller?
  ~queue_processor();

private:
  void processor_thread();

  std::atomic <bool> terminated;
  std::unique_ptr <std::thread> thread;

  std::function <bool(Type)>   action;
  blocking_strict_queue <Type> queue;
};


template <class Type>
void blocking_strict_queue <Type> ::terminate() {
  std::unique_lock <std::mutex> local_lock(empty_lock);
  terminated = true;
  empty_wait.notify_all();
}

template <class Type>
bool blocking_strict_queue <Type> ::is_terminated() const {
  return terminated;
}

template <class Type>
bool blocking_strict_queue <Type> ::transfer_next_item(queue_type &from) {
  std::unique_lock <std::mutex> local_lock(empty_lock);
  if (terminated || queue.size() >= capacity || from.empty()) {
    return false;
  } else {
    queue.push(std::move(from.front()));
    from.pop();
    empty_wait.notify_all();
    return true;
  }
}

template <class Type>
bool blocking_strict_queue <Type> ::empty() {
  std::unique_lock <std::mutex> local_lock(empty_lock);
  return queue.empty();
}

template <class Type>
bool blocking_strict_queue <Type> ::dequeue(Type &removed) {
  while (!terminated) {
    std::unique_lock <std::mutex> local_lock(empty_lock);
    if (queue.empty()) {
      empty_wait.wait(local_lock);
      continue;
    } else {
      removed = std::move(queue.front());
      queue.pop();
      return true;
    }
  }
  return false;
}

template <class Type>
blocking_strict_queue <Type> ::~blocking_strict_queue() {
  this->terminate();
}


template <class Type>
void queue_processor <Type> ::start() {
  assert(!thread && !this->is_terminated());
  thread.reset(new std::thread([this] { this->processor_thread(); }));
}

template <class Type>
void queue_processor <Type> ::terminate() {
  terminated = true;
  // NOTE: This causes the thread to continue if blocked.
  queue.terminate();
}

template <class Type>
bool queue_processor <Type> ::is_terminated() const {
  return terminated || queue.is_terminated();
}

template <class Type>
bool queue_processor <Type> ::transfer_next_item(locked_queue &from_queue) {
  if (this->is_terminated()) {
    return false;
  }
  auto write_queue = from_queue.get_write();
  assert(write_queue);
  // Should only block if processor_thread is in the process of removing an
  // item, but hasn't called action yet.
  return queue.transfer_next_item(*write_queue);
}

template <class Type>
queue_processor <Type> ::~queue_processor() {
  this->terminate();
  if (thread) {
    thread->join();
  }
}

template <class Type>
void queue_processor <Type> ::processor_thread() {
  assert(action);
  while (!this->is_terminated()) {
    Type removed;
    if (!queue.dequeue(removed)) {
      break;
    } else {
      if (!action(std::move(removed))) {
        break;
      }
    }
  }
  // Don't accept anything new. This is necessary if action returns false and
  // turns this processor into a zombie.
  this->terminate();
}

#endif //queue_processor_hpp
