/*
MIT License

Copyright (c) 2018 Brian T. Park

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef ACE_ROUTINE_COROUTINE_H
#define ACE_ROUTINE_COROUTINE_H

#include <stdint.h> // UINT16_MAX
#include <Print.h> // Print
#include "Flash.h" // ACE_ROUTINE_F()
#include "FCString.h"

/**
 * @file Coroutine.h
 *
 * All coroutines are instances of the Coroutine base class. The COROUTINE()
 * macro creates these instances, and registers them to automatically run when
 * CoroutineScheduler::loop() is called.
 *
 * Various macros use macro overloading to implement a 1-argument and
 * a 2-argument version. See https://stackoverflow.com/questions/11761703 to
 * description of how that works.
 *
 * The computed goto is a GCC extension:
 * https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
 * The __noinline__ and __noclone__ attributes make sure that label pointers are
 * always the same. I'm not 100% sure they are needed here, but they don't seem
 * to hurt.
 */

/**
 * Create a Coroutine instance named 'name'. Two forms are supported
 *
 *   - COROUTINE(name) {...}
 *   - COROUTINE(className, name) {...}
 *
 * The 1-argument form uses the Coroutine class as the base class of the
 * coroutine. The 2-argument form uses the user-provided className which must be
 * a subclass of Coroutine.
 *
 * The code in {} following this macro becomes the body of the
 * Coroutine::run() method.
 */
#define COROUTINE(...) \
    GET_COROUTINE(__VA_ARGS__, COROUTINE2, COROUTINE1)(__VA_ARGS__)

#define GET_COROUTINE(_1, _2, NAME, ...) NAME

#define COROUTINE1(name) \
struct Coroutine_##name : ace_routine::Coroutine { \
  Coroutine_##name(); \
  virtual int run() override \
    __attribute__((__noinline__,__noclone__)); \
} name; \
Coroutine_##name :: Coroutine_##name() { \
  init(ACE_ROUTINE_F(#name)); \
} \
int Coroutine_##name :: run()

#define COROUTINE2(className, name) \
struct className##_##name : className { \
  className##_##name(); \
  virtual int run() override \
    __attribute__((__noinline__,__noclone__)); \
} name; \
className##_##name :: className##_##name() { \
  init(ACE_ROUTINE_F(#name)); \
} \
int className##_##name :: run()

/**
 * Create an extern reference to a coroutine that is defined in another .cpp
 * file. The extern reference is needed before it can be used. Two forms are
 * supported:
 *
 *    - EXTERN_COROUTINE(name);
 *    - EXTERN_COROUTINE(className, name);
 */
#define EXTERN_COROUTINE(...) \
    GET_EXTERN_COROUTINE(\
        __VA_ARGS__, EXTERN_COROUTINE2, EXTERN_COROUTINE1)(__VA_ARGS__)

#define GET_EXTERN_COROUTINE(_1, _2, NAME, ...) NAME

#define EXTERN_COROUTINE1(name) \
struct Coroutine_##name : ace_routine::Coroutine { \
  Coroutine_##name(); \
  virtual int run() override \
    __attribute__((__noinline__,__noclone__)); \
}; \
extern Coroutine_##name name

#define EXTERN_COROUTINE2(className, name) \
struct className##_##name : className { \
  className##_##name(); \
  virtual int run() override \
    __attribute__((__noinline__,__noclone__)); \
}; \
extern className##_##name name

/** Mark the beginning of a coroutine. */
#define COROUTINE_BEGIN() \
    void* p = getJump(); \
    if (p != nullptr) { \
      goto *p; \
    }

/**
 * Mark the beginning of a coroutine loop. Can be used instead of
 * COROUTINE_BEGIN() at the beginning of a Coroutine.
 */
#define COROUTINE_LOOP() \
   COROUTINE_BEGIN(); \
   while (true) \

#define COROUTINE_YIELD_INTERNAL() \
    do { \
      __label__ jumpLabel; \
      setJump(&& jumpLabel); \
      return 0; \
      jumpLabel: ; \
    } while (false)

/** Yield execution to another coroutine. */
#define COROUTINE_YIELD() \
    do { \
      setYielding(); \
      COROUTINE_YIELD_INTERNAL(); \
      setRunning(); \
    } while (false)

/**
 * Yield until condition is true, then execution continues. This is
 * functionally equivalent to:
 *
 * @code
 *    while (!condition) COROUTINE_YIELD();
 * @endcode
 *
 * but the getStatus() during the waiting is set to kStatusAwaiting instead of
 * kStatusYielding. The current scheduler treats the two states the same, but
 * it's possible that a different scheduler may want to treat them differently.
 */
#define COROUTINE_AWAIT(condition) \
    do { \
      while (!(condition)) { \
        setAwaiting(); \
        COROUTINE_YIELD_INTERNAL(); \
      } \
      setRunning(); \
    } while (false)

/**
* Yield for delayMillis. A delayMillis of 0 is functionally equivalent to
* COROUTINE_YIELD(). To save memory, the delayMillis is stored as a uint16_t
* but the actual maximum is limited to 32767 millliseconds. See setDelay()
* for the reason for this limitation.
*
* If you need to wait for longer than that, use a for-loop to call
* COROUTINE_DELAY() as many times as necessary.
*
* This could have been implemented using COROUTINE_AWAIT() but this macro
* matches the global delay(millis) function already provided by the Arduino
* API. Also having a separate kStatusDelaying state allows the
* CoroutineScheduler to be slightly more efficient by avoiding the call to
* Coroutine::run() if the delay has not expired.
*/
#define COROUTINE_DELAY(delayMillis) \
    do { \
      setDelay(delayMillis); \
      while (!isDelayExpired()) { \
        setDelaying(); \
        COROUTINE_YIELD_INTERNAL(); \
      } \
      setRunning(); \
    } while (false)

/**
 * Mark the end of a coroutine. Subsequent calls to Coroutine::run()
 * will do nothing.
 */
#define COROUTINE_END() \
    do { \
      __label__ jumpLabel; \
      setEnding(); \
      setJump(&& jumpLabel); \
      jumpLabel: ; \
      return 0; \
    } while (false)

namespace ace_routine {

/**
 * Base class of all coroutines. The actual coroutine code is an implementation
 * of the virtual run() method.
 */
class Coroutine {
  friend class CoroutineScheduler;

  public:
    /**
     * Get the pointer to the root pointer. Implemented as a function static to
     * fix the C++ static initialization problem, making it safe to use this in
     * other static contexts.
     */
    static Coroutine** getRoot();

    /**
     * Return the next pointer as a pointer to the pointer, similar to
     * getRoot(). This makes it much easier to manipulate a singly-linked list.
     * Also makes setNext() method unnecessary.
     */
    Coroutine** getNext() { return &mNext; }

    /** Human-readable name of the coroutine. */
    const FCString& getName() const { return mName; }

    /**
     * The body of the coroutine. The COROUTINE macro creates a subclass of
     * this class and puts the body of the coroutine into this method.
     *
     * @return The return value is always ignored. This method is declared to
     * return an int to prevent the user from accidentally returning from this
     * method using an explicit 'return' statement instead of through one of
     * the macros (e.g. COROUTINE_YIELD(), COROUTINE_DELAY(), COROUTINE_AWAIT()
     * or COROUTINE_END()).
     */
    virtual int run() = 0;

    /**
     * Returns the current millisecond clock. By default it returns the global
     * millis() function from Arduino but can be overridden for testing.
     */
    virtual unsigned long millis() const;

    /**
     * Suspend the coroutine at the next scheduler iteration. If the coroutine
     * is already in the process of ending or is already terminated, then this
     * method does nothing. A coroutine cannot use this method to suspend
     * itself, it can only suspend some other coroutine. Currently, there is no
     * ability for a coroutine to suspend itself, that would require the
     * addition of a COROUTINE_SUSPEND() macro. Also, this method works only if
     * the CoroutineScheduler::loop() is used because the suspend functionality
     * is implemented by the CoroutineScheduler.
     */
    void suspend() {
      if (isDone()) return;
      mStatus = kStatusSuspended;
    }

    /**
     * Add a Suspended coroutine into the head of the scheduler linked list,
     * and change the state to Yielding. If the coroutine is in any other
     * state, this method does nothing. This method works only if the
     * CoroutineScheduler::loop() is used.
     */
    void resume();

    /** Check if delay time is over. */
    bool isDelayExpired() {
      uint16_t elapsedMillis = millis() - mDelayStartMillis;
      return elapsedMillis >= mDelayDurationMillis;
    }

    /** The coroutine was suspended with a call to suspend(). */
    bool isSuspended() const { return mStatus == kStatusSuspended; }

    /** The coroutine returned using COROUTINE_YIELD(). */
    bool isYielding() const { return mStatus == kStatusYielding; }

    /** The coroutine returned using COROUTINE_AWAIT(). */
    bool isAwaiting() const { return mStatus == kStatusAwaiting; }

    /** The coroutine returned using COROUTINE_DELAY(). */
    bool isDelaying() const { return mStatus == kStatusDelaying; }

    /** The coroutine is currently running. True only within the coroutine. */
    bool isRunning() const { return mStatus == kStatusRunning; }

    /**
     * The coroutine returned using COROUTINE_END(). In most cases, isDone() is
     * recommended instead because it works when coroutines are executed
     * manually or through the CoroutineScheduler.
     */
    bool isEnding() const { return mStatus == kStatusEnding; }

    /**
     * The coroutine was terminated by the scheduler with a call to
     * setTerminated(). In most cases, isDone() should be used instead
     * because it works when coroutines are executed manually or through the
     * CoroutineScheudler.
     */
    bool isTerminated() const { return mStatus == kStatusTerminated; }

    /**
     * The coroutine is either Ending or Terminated. This method is recommended
     * over isEnding() or isTerminated() because it works when the coroutine is
     * executed either manually or through the CoroutineScheduler.
     */
    bool isDone() const {
      return mStatus == kStatusEnding || mStatus == kStatusTerminated;
    }

  protected:
    /**
     * The execution status of the coroutine, corresponding to the
     * COROUTINE_YIELD(), COROUTINE_DELAY(), COROUTINE_AWAIT() and
     * COROUTINE_END() macros.
		 *
     * The finite state diagram looks like this:
     *
     * @verbatim
     *          Suspended
     *          ^   ^   ^
     *         /    |    \
     *        /     |     \
     *       v      |      \
     * Yielding Awaiting Delaying
     *      ^       ^       ^
     *       \      |      /
     *        \     |     /
     *         \    |    /
     *          v   v   v
     *           Running
     *              |
     *              |
     *              v
     *           Ending
     *              |
     *              |
     *              v
     *         Terminated
     * @endverbatim
     */
    typedef uint8_t Status;

    /**
     * Coroutine has been suspended using suspend() and the scheduler should
     * remove it from the queue upon the next iteration. We don't distinguish
     * whether the coroutine is still in the queue or not with this status. We
     * can add that later if we need to.
     */
    static const Status kStatusSuspended = 0;

    /** Coroutine returned using the COROUTINE_YIELD() statement. */
    static const Status kStatusYielding = 1;

    /** Coroutine returned using the COROUTINE_AWAIT() statement. */
    static const Status kStatusAwaiting = 2;

    /** Coroutine returned using the COROUTINE_DELAY() statement. */
    static const Status kStatusDelaying = 3;

    /** Coroutine is currenly running. True only within the coroutine itself. */
    static const Status kStatusRunning = 4;

    /** Coroutine executed the COROUTINE_END() statement. */
    static const Status kStatusEnding = 5;

    /** Coroutine has ended and no longer in the scheduler queue. */
    static const Status kStatusTerminated = 6;

    /** Constructor. */
    Coroutine() {}

    /**
     * Initialize the coroutine, set it to Yielding state, and add it to the
     * linked list of coroutines.
     *
     * @param name The name of the coroutine as a human-readable string.
     */
    void init(const char* name) {
      mName = FCString(name);
      mStatus = kStatusYielding;
      insertSorted();
    }

    /** Same as init(const char*) except using flash string type. */
    void init(const __FlashStringHelper* name) {
      mName = FCString(name);
      mStatus = kStatusYielding;
      insertSorted();
    }

    /** Return the status of the coroutine. Used by the CoroutineScheduler. */
    Status getStatus() const { return mStatus; }

    /** Pointer to label where execute will start on the next call to run(). */
    void setJump(void* jumpPoint) { mJumpPoint = jumpPoint; }

    /** Pointer to label where execute will start on the next call to run(). */
    void* getJump() const { return mJumpPoint; }

    /** Set the kStatusRunning state. */
    void setRunning() { mStatus = kStatusRunning; }

    /** Set the kStatusDelaying state. */
    void setYielding() { mStatus = kStatusYielding; }

    /** Set the kStatusAwaiting state. */
    void setAwaiting() { mStatus = kStatusAwaiting; }

    /** Set the kStatusDelaying state. */
    void setDelaying() { mStatus = kStatusDelaying; }

    /**
     * Configure the delay timer. The maximum duration is set to (UINT16_MAX /
     * 2) (i.e. 32767 milliseconds) if given a larger value. This makes the
     * longest allowable time between two successive calls to isDelayExpired()
     * for a given coroutine to be 32767 (UINT16_MAX - UINT16_MAX / 2 - 1)
     * milliseconds, which should be long enough for basically all real
     * use-cases. (The '- 1' comes from an edge case where isDelayExpired()
     * evaluates to be true in the CoroutineScheduler::runCoroutine() but
     * becomes to be false in the COROUTINE_DELAY() macro inside
     * Coroutine::run()) because the clock increments by 1 millisecond.)
     */
    void setDelay(uint16_t delayMillisDuration) {
      mDelayStartMillis = millis();
      mDelayDurationMillis = (delayMillisDuration >= UINT16_MAX / 2)
          ? UINT16_MAX / 2
          : delayMillisDuration;
    }

    /** Set the kStatusEnding state. */
    void setEnding() { mStatus = kStatusEnding; }

    /**
     * Set status to indicate that the Coroutine has been removed from the
     * Scheduler queue. Should be used only by the CoroutineScheduler.
     */
    void setTerminated() { mStatus = kStatusTerminated; }

  private:
    // Disable copy-constructor and assignment operator
    Coroutine(const Coroutine&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;

    /**
     * Insert the current coroutine into the singly linked list. The order of
     * C++ static initialization is undefined, but if getName() is not null
     * (which will normally be the case when using the COROUTINE() macro), the
     * coroutine will be inserted using getName() as the sorting key. This makes
     * the ordering deterministic, which is required for unit tests.
     *
     * The insertion algorithm is O(N) per insertion, for a total complexity
     * of O(N^2). That's probably good enough for a "small" number of
     * coroutines, where small is around O(100). If a large number of
     * coroutines are inserted, then this method needs to be optimized.
     */
    void insertSorted();

    FCString mName;
    Coroutine* mNext = nullptr;
    void* mJumpPoint = nullptr;
    Status mStatus = kStatusSuspended;
    uint16_t mDelayStartMillis;
    uint16_t mDelayDurationMillis;
};

}

#endif
