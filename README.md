Corona is an _experimental_ server-side JavaScript runtime that provides
concurrency support using [coroutines](http://en.wikipedia.org/wiki/Coroutine).
It builds on the work of the [NodeJS](http://nodejs.org) team, particularly in
its integration with the [V8 JavaScript engine](http://code.google.com/p/v8).

## Runtime Model

Like NodeJS, Corona executes in a single process, with JavaScript executing in
a single thread. No API calls exposed to JavaScript will cause the process
itself to block.

However, in contrast to NodeJS, the Corona runtime can support multiple
independent control flows in JavaScript. These control flows are implemented
under the covers as coroutines. Individual coroutines can invoke API calls that
block the coroutine (but not the process itself). This allows usage the linear
programming style familiar to most programmers without sacraficing performance.

A programming model with multiple flows of control, regardless of the
underlying implementation requires some mechanism for mitigating race
conditions. Most thread-like systems implement synchronization primitives such
a mutexes, semaphores and the like. It is an explicit design goal of Corona to
provide a programming model that does not require use of these constructs:
[threads are
hard](http://www.cs.sfu.ca/~vaughan/teaching/431/papers/ousterhout-threads-usenix96.pdf).

That said, there must be some model for allowing control flows to interact with
and reason about global state in the presence of blocking API calls. Corona's
attempt to address this is the following set of runtime features

* _Support for futures_: When invoking an function call that needs to block to
  compute its return value, rather than blocking the process immediately, the
  function can return a _Future_ object immediately, without blocking. This
  Future represents a holder for the future return value of the function. Only
  when this object is interrogated does the process block. This allows
  developers to implement atomic codepaths that span multiple nominally
  blocking functions.
* _Toggling blocking/non-blocking mode_: A set of primitives will be provided
  to ensure that the runtime does not block. These can be used to implement
  critical sections. An exception will be thrown when any system call is
  invoked that would otherwise block the currently executing control flow.

## Corona vs. [NodeJS](http://nodejs.org)

Corona's value proposition in comparison to NodeJS is one of
ease-of-development. While JavaScript is a language uniquely capable in its
support for an asynchronous programing model (chiefly due to closures and
anonymous functions), there is some room for improvement over the purely
asynchronous environment provided by NodeJS:

* Purely asynchronous programming requires developers to maintain the
  application state machine in two places: the stack, and state explicitly
  maintained and passed around to callbacks.
* In practice, purely asynchronous programming can necessitate deeply nested
  callback structures. Such a control flow is more concisely represented in a
  linear sequence of blocking calls.
* Correlating callbacks to the codepath that created them is often difficult.
  The canonical example for this is throwing an exception inside an event
  handler -- who "owns" the event handler and where did it come from?
