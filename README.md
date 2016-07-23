# Action Timer Library
#### https://github.com/ta0kira/action-timer

This library is in an alpha stage, and is by no means ready for wide use. In
fact, it doesn't even compile into a library yet.

**tl;dr:** This library:

1. Provides a useful representation of a categorical distribution.
1. Generates Poisson-distributed events in the time dimension.
1. Distributes incoming requests to request processors based on a fixed rate of
   traffic that they have committed to.

## `category_tree`

(See [category-tree.hpp](include/category-tree.hpp) for more info.)

### Background

`category_tree` is a representation of a [categorical distribution][categorical].
An example of this is a 6-sided die where the different numbers have different
probabilities.

I ended up writing a special class for this for efficiency reasons. I've needed
to sample data from this sort of distribution on numerous occasions, and the
general procedure has been to represent it with a vector of probabilities and
a vector of categories. For example:

```c++
double p = { 0.1, 0.5, 0.4 };
char c[] = { 'A', 'B', 'C' };
```

Then, with a uniform random number, I would iterate to see where it fell. I
believe this is the canonical approach to generating categorical samples.

The main problem here is that generating the value is linear, which doesn't
scale for large _n_.

Another approach would be to precompute a cumulative sum:

```c++
double p_le = { 0.1, 0.6, 1.0 };
```

Then, a binary search is possible. The problem here is that _updating_ any of
the probabilities is now linear.

So, it seems that the simple choices have either linear lookup or linear
updating, but for this particular project, I wanted to have logarithmic time for
both. Thus, `category_tree`.

### Details

Imagine that you have _n_ categories each with independent sizes, e.g., **A**
with size 1, **B** with size 5, and **C** with size 4:

```text
0 1         6       10
|)|--------)|------)|
 A     B        C
```

This obviously won't work as a categorical distribution, since the parts add up
to greater-than 1; however, if we know their sum (10), we can multiply a uniform
value by that number, then perform a binary search. For example, if our uniform
number is 0.56, we can search for 5.6:

```text
         ~5.6
0 1        |6       10
|)|--------)|------)|
 A     B        C
```

Here we choose **B**. Fine. This is how it normally works.

The differences here are that:
- We don't need to normalize any of the category sizes.
- The category sizes might have other meaning, e.g., they might be from a
  [Dirichlet prior][dirichlet] or parameters for
  [exponential distributions][exponential]. (The latter is the case here.)
- If the categories can be sorted, this can be represented as a tree. This
  allows logarithmic changes, as well as logarithmic lookup.

`category_tree` is an [AVL tree][avl] sorted by category label, but it has the
additional property that each parent stores the total of all category sizes of
all of its children. This allows both a binary search for a category label,
_and_ a binary search for the category corresponding to a target value.

```c++
// example/category_tree_demo1.cpp

#include <iostream>
#include "category-tree.hpp"

int main() {
  category_tree <char, double> categories;
  categories.update_category('A', 1.0);
  categories.update_category('B', 5.0);
  categories.update_category('C', 4.0);

  // Note that the lookup value must be strictly less-than the total size!
  std::cout << "Choice for 0.56: "
            << categories.locate(0.56 * categories.get_total_size())
            << std::endl;

  categories.update_category('C', [](int x) { return 3.0*x; });
  std::cout << "Choice for 0.56: "
            << categories.locate(0.56 * categories.get_total_size())
            << std::endl;
}
```

```shell
$ c++ -std=c++11 -O2 -g -Iinclude example/category_tree_demo1.cpp -o category_tree_demo1
$ ./category_tree_demo1
```

This also scales very well:

```c++
// example/category_tree_demo2.cpp

#include <iostream>
#include "category-tree.hpp"

int main() {
  category_tree <int, double> categories;
  for (int i = 0; i < 1000000; ++i) {
    categories.update_category(i, 1.0);
  }

  std::cout << "Choice for 0.56: "
            << categories.locate(0.56 * categories.get_total_size())
            << std::endl;
}
```

```shell
$ c++ -std=c++11 -O2 -g -Iinclude example/category_tree_demo2.cpp -o category_tree_demo2
$ ./category_tree_demo2
```

## `action_timer`

(See [action-timer.hpp](include/action-timer.hpp) for more info.)

### Background

`action_timer` is an event timer that triggers real-time events that are
[Poisson-distributed][poisson] in the time dimension. For each action, you
specify a _&lambda;_ parameter that indicates how often, on average, the action
 should occur per second, and the `action_timer` generates events accordingly.

**Why would you ever want this?**

1. If you have numerous such processes that target a common service or resource,
   the usage remains somewhat balanced without the processes coordinating, or
   knowing at what rate any of the others are executing.
1. This is an approximate model for many things related to queueing.
1. _Most importantly_, a single timer can be used to manage an unlimited number
   of processes _without_ spawning a new thread to time each of them. This is
   due to the fact that you can combine a [categorical distribution][categorical]
   with _n_ categories and an [exponential distribution][exponential] to create
   _n_ [exponential distributions][exponential].

### Details

`action_timer` manages callbacks that are each labeled with a unique category
and each have a _&lambda;_ parameter indicating how many times per second, _on
average_, they should be executed. Sometimes it will be much shorter or longer
between executions, but the long-term average should be extremely close to that
specified.

```c++
// example/action_timer_demo1.cpp

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
      return false;
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
```

```shell
$ c++ -std=c++11 -O2 -g -Iinclude -Ilocking-container/include \
    example/action_timer_demo1.cpp src/{action,timer}.cpp -o action_timer_demo1 -pthread
$ ./action_timer_demo1
```

## `poisson_queue`

(See [poisson-queue.hpp](include/poisson-queue.hpp) for more info.)

### Background

`poisson_queue` manages a queue of items and a dynamic set of processors that
process these items. At the time each process is registered, the caller commits
to a certain average level of traffic via a _&lambda;_ Poisson parameter. The
`poisson_queue` attempts to pass a new item to each processor according to the
respective rate that has been committed to.

**Why would you ever want this?**

If a processor doesn't accept an item (due it being behind, or dying):
- The rate of items being sent to the other processors remains unchanged.
- The item will still be processed in about the same amount of time as it would
  have if that processor had not existed.

If a processor actually stops processing, the `poisson_queue` will recover the
items that have yet to be processed and will requeue them.

### Details

```c++
// example/poisson_queue_demo1.cpp

#include <iostream>
#include <string>
#include <thread>
#include "poisson-queue.hpp"
#include "locking-container.inc"

int main() {
  poisson_queue <std::string, int> queue;
  queue.start();

  // poisson_queue manages two types of actions:
  // - Regular actions, like what action_timer uses.
  // - Processor actions, which process items from the queue.


  // A zombie_cleanup action ensures that items are recovered if a processor
  // dies. This isn't automatically started by poisson_queue for numerous
  // reasons, the main one being that there is no logical way to determine what
  // the category label should be.
  action_timer <std::string> ::generic_action zombie_action(new async_action([&queue] {
    queue.zombie_cleanup();
    return true;
  }));
  queue.set_action("zombie_cleanup", std::move(zombie_action), 1.0);


  // A processor takes a value from the queue and does something with it. If the
  // processor returns false, the *modified* item is placed back in the queue
  // and the processor dies; otherwise, the item gets destructed and the
  // processor continues.
  // The lambda value should be approximately how many items the processor can
  // handle per second. Deciding on the queue size can be slightly complicated,
  // but it's mathematically well-defined at least.
  queue.set_processor("printer",
    [](int &value) {
      std::cout << "Processing " << value << "." << std::endl;
      // Note that this *doesn't* block the queue!
      std::this_thread::sleep_for(
        std::chrono::duration <double> (0.1));
      return true;
    }, 10.0, 10);


  // Adding items to the queue can be done from any thread, but it makes the
  // most sense from the thread that owns the queue.
  for (int i = 0; i < 100; ++i) {
    queue.queue_item(i);
  }

  // At the moment there isn't a clear way to wait for the poisson_queue to
  // finish doing what it's doing. This is because:
  // - A processor can die, causing items to be requeued.
  // - A processor could be finishing up, and exiting will kill it.
  while (!queue.empty()) {
    std::this_thread::sleep_for(
      std::chrono::duration <double> (0.1));
  }
  std::this_thread::sleep_for(
    std::chrono::duration <double> (1.0));
}
```

```shell
$ c++ -std=c++11 -O2 -g -Iinclude -Ilocking-container/include \
    example/poisson_queue_demo1.cpp src/{action,timer}.cpp -o poisson_queue_demo1 -pthread
$ ./poisson_queue_demo1
```

[categorical]: https://en.wikipedia.org/wiki/Categorical_distribution
[dirichlet]: https://en.wikipedia.org/wiki/Dirichlet_distribution
[exponential]: https://en.wikipedia.org/wiki/Exponential_distribution
[avl]: https://en.wikipedia.org/wiki/AVL_tree
[poisson]: https://en.wikipedia.org/wiki/Poisson_distribution
