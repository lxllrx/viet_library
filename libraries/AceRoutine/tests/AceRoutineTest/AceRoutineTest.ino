#line 2 "AceCoroutineTest.ino"

#include <AceRoutine.h>
#include <AUnitVerbose.h>
#include "ace_routine/testing/TestableCoroutine.h"

using namespace ace_routine;
using namespace ace_routine::testing;
using namespace aunit;

test(FCString_compareTo) {
  FCString n;
  FCString a("a");
  FCString b("b");
  FCString fa(F("a"));
  FCString fb(F("b"));

  assertEqual(n.compareTo(n), 0);
  assertLess(n.compareTo(a), 0);
  assertLess(n.compareTo(fa), 0);

  assertEqual(a.compareTo(fa), 0);
  assertEqual(fb.compareTo(b), 0);

  assertLess(a.compareTo(b), 0);
  assertLess(a.compareTo(fb), 0);
  assertMore(fb.compareTo(a), 0);
  assertMore(fb.compareTo(fa), 0);
}

// An external flag to await upon, for testing.
bool simpleCoroutineFlag = false;

// A coroutine that yields, delays, then ends.
COROUTINE(TestableCoroutine, simpleCoroutine) {
  COROUTINE_BEGIN();
  COROUTINE_YIELD();
  COROUTINE_DELAY(1);
  COROUTINE_AWAIT(simpleCoroutineFlag);
  COROUTINE_END();
}

// Verify that multiple calls to Coroutine::run() after it ends is ok.
test(simpleCoroutine) {
  simpleCoroutine.millis(0);
  assertTrue(simpleCoroutine.isSuspended());

  simpleCoroutine.run();
  assertTrue(simpleCoroutine.isYielding());

  simpleCoroutine.run();
  assertTrue(simpleCoroutine.isDelaying());

  simpleCoroutine.run();
  assertTrue(simpleCoroutine.isDelaying());

  simpleCoroutine.millis(1);
  simpleCoroutine.run();
  assertTrue(simpleCoroutine.isAwaiting());

  simpleCoroutine.run();
  assertTrue(simpleCoroutine.isAwaiting());

  simpleCoroutineFlag = true;
  simpleCoroutine.run();
  assertTrue(simpleCoroutine.isEnding());

  simpleCoroutine.run();
  assertTrue(simpleCoroutine.isEnding());
}

// c is defined in another .cpp file
EXTERN_COROUTINE(TestableCoroutine, c);

// Define b before a because 'a' uses 'b'
COROUTINE(TestableCoroutine, b) {
  COROUTINE_BEGIN();
  COROUTINE_YIELD();
  COROUTINE_DELAY(25);
  COROUTINE_AWAIT(c.isDone());
  COROUTINE_END();
}

// Define a last. If there is a circular dependency between a and b, we can use
// a pointer (Coroutine *a and Coroutine* b) to break the circular dependency,
// just like any other normal objects.
COROUTINE(TestableCoroutine, a) {
  COROUTINE_LOOP() {
    COROUTINE_DELAY(25);
    COROUTINE_YIELD();
    COROUTINE_AWAIT(b.isDone());
  }
}

// An extra coroutine, initially suspended using extra.suspend().
COROUTINE(TestableCoroutine, extra) {
  COROUTINE_BEGIN();
  COROUTINE_END();
}

// Only 3 coroutines are initially active: a, b, c
test(scheduler) {
  // initially everything is enabled
  assertTrue(a.isYielding());
  assertTrue(b.isYielding());
  assertTrue(c.isYielding());

  // run a
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());
  assertTrue(b.isYielding());
  assertTrue(c.isYielding());

  // run b
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());
  assertTrue(b.isYielding());
  assertTrue(c.isYielding());

  // run c
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());
  assertTrue(b.isYielding());
  assertTrue(c.isDelaying());

  a.millis(10);
  b.millis(10);
  c.millis(10);

  // run a
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());
  assertTrue(b.isYielding());
  assertTrue(c.isDelaying());

  // run b
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());
  assertTrue(b.isDelaying());
  assertTrue(c.isDelaying());

  // run c
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());
  assertTrue(b.isDelaying());
  assertTrue(c.isDelaying());

  a.millis(36);
  b.millis(36);
  c.millis(36);

  // run a
  CoroutineScheduler::loop();
  assertTrue(a.isYielding());
  assertTrue(b.isDelaying());
  assertTrue(c.isDelaying());

  // run b
  CoroutineScheduler::loop();
  assertTrue(a.isYielding());
  assertTrue(b.isAwaiting());
  assertTrue(c.isDelaying());

  // run c
  CoroutineScheduler::loop();
  assertTrue(a.isYielding());
  assertTrue(b.isAwaiting());
  assertTrue(c.isDelaying());

  a.millis(101);
  b.millis(101);
  c.millis(101);

  // run a
  CoroutineScheduler::loop();
  assertTrue(a.isAwaiting());
  assertTrue(b.isAwaiting());
  assertTrue(c.isDelaying());

  // run b
  CoroutineScheduler::loop();
  assertTrue(a.isAwaiting());
  assertTrue(b.isAwaiting());
  assertTrue(c.isDelaying());

  // run c
  CoroutineScheduler::loop();
  assertTrue(a.isAwaiting());
  assertTrue(b.isAwaiting());
  assertTrue(c.isEnding());

  a.millis(102);
  b.millis(102);
  c.millis(102);

  // run a
  CoroutineScheduler::loop();
  assertTrue(a.isAwaiting());
  assertTrue(b.isAwaiting());
  assertTrue(c.isEnding());

  // run b
  CoroutineScheduler::loop();
  assertTrue(a.isAwaiting());
  assertTrue(b.isEnding());
  assertTrue(c.isEnding());

  // run c - removed from list
  CoroutineScheduler::loop();
  assertTrue(a.isAwaiting());
  assertTrue(b.isEnding());
  assertTrue(c.isTerminated());

  a.millis(103);
  b.millis(103);
  c.millis(103);

  // run a, waiting for b over, loops around to delay(25)
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());
  assertTrue(b.isEnding());

  // run b - removed from list
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());
  assertTrue(b.isTerminated());

  a.millis(104);
  b.millis(104);
  c.millis(104);

  // run a - continues to run the COROUTINE_LOOP()
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());

  a.millis(130);
  b.millis(130);
  c.millis(130);

  // run a - continues to run the COROUTINE_LOOP()
  CoroutineScheduler::loop();
  assertTrue(a.isYielding());

  a.millis(131);
  b.millis(131);
  c.millis(131);
  extra.millis(131);

  // resume 'extra'
  assertTrue(extra.isSuspended());
  extra.resume();

  // run 'extra'
  CoroutineScheduler::loop();
  assertTrue(extra.isEnding());
  assertTrue(a.isYielding());

  // run 'a'
  CoroutineScheduler::loop();
  assertTrue(extra.isEnding());
  assertTrue(a.isDelaying());

  a.millis(132);
  b.millis(132);
  c.millis(132);
  extra.millis(132);

  // run 'extra'
  CoroutineScheduler::loop();
  assertTrue(extra.isTerminated());
  assertTrue(a.isDelaying());

  // run 'a'
  CoroutineScheduler::loop();
  assertTrue(a.isDelaying());
}

void setup() {
  delay(1000); // some boards reboot twice
  Serial.begin(115200);
  while (!Serial); // Leonardo/Micro

  extra.suspend();
  simpleCoroutine.suspend();
  CoroutineScheduler::setup();
  CoroutineScheduler::list(Serial);
}

void loop() {
  TestRunner::run();
}
