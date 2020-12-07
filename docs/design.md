<!--
     Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)

     SPDX-License-Identifier: CC-BY-SA-4.0
-->

# seL4test Design

seL4test is an seL4 testing framework that runs test suites on seL4. It is used
to test both kernel and user code.

## Motivation

Even though seL4 is a verified microkernel we still require a comprehensive set
of tests that can be used as a way to easily check if the kernel is working.
Here are some reasons:
- Verification takes a long time: It takes many hours to run the proofs, being able
  to run tests faster means that development can be faster due to faster error
  detection and correction.
- Verification doesn't cover all configurations and platforms: A test suite is
  able to cover a much larger range of configurations and platforms.
- Verification has assumptions: Tests can be used to try and check if any verification
  assumptions aren't true.
- Validate correct configuration and toolchains: The tests can be used to sanity check
  whether a build configuration and installed set of host dependencies are working.
- Help clarify correct operation of kernel APIs: Tests can be used to demonstrate
  how an interface is expected to handle edge cases more precisely.
- Serve as test runner for non-kernel code: For any other code that runs with seL4,
  there needs to be a way to easily test if it works correctly.

## Goals

In order to achieve the above, each test added to sel4test should have the following
properties in roughly the following priority ordering:
- Test independence: The behavior of one test shouldn't be affected by how other tests run.
- Tests are unambiguous in success or failure: A test's success should indicate that
  something works correctly, and a failure that something hasn't worked correctly. Tests
  shouldn't use success to indicate that something may work, or failure to indicate that
  something might not be working (See: law of excluded middle).
- Tests are repeatable: A test can be deterministic and rerunning it should lead to
  the same result.
- Tests are self-contained and don't rely on external interactions: A test can't have
  any external dependencies that aren't already provided by its test environment.
- Tests complete quickly: Each test should finish quickly as the overall test duration
  is an accumulation of all of the individual tests. Tests shouldn't take longer than 10ms each.
- Tests are easy to understand: Learning and understanding a test's behavior shouldn't
  require a large cognitive load.
- Tests are easy to add/remove: Adding and removing tests is a common operation and
  shouldn't be expensive to perform. This includes running the test to confirm that it
  functions correctly.

Additionally a universal goal is that there are at least as many tests present to
validate that the systems under test are functioning correctly.


## Features

This project has the following components/features:

### Roottask

`sel4test-driver` is an executable that runs as an seL4 root task. It expects to
be given the initial set of seL4 capabilities (`seL4_Bootinfo_t`) and it uses them
to bootstrap its own system environment and create testing environments for then
running all of the tests.  It has basic operating system functionality for creating
and destroying multiple test runs and supporting different testing environments.

### Test environments

A test environment defines what resources a test has access to when it runs. An environment
may have start-up and shut-down procedures that are run before and after the test
is run. A test definition has a test environment attribute that is used to link a
test with the environment it requires.

#### Bootstrap environment

Each test environment runs the test in a separate "process" from the root task. This
is to isolate tests from each other. However there is a `bootstrap` test environment
that runs tests within the root task. This environment is for running tests that test
the functionality for creating and communicating with different environment "processes".


### Tests

A test is a function that is invoked with a reference to its environment. Each test
performs a set of actions that produce a result. The result is compared to an expected
value to determine success or failure. A test is expected to terminate with a success
or failure result and isn't allowed to run forever.

### Test selection

A reference to the test is added to a special linker section and the roottask
uses this to select and run the tests at runtime. The tests run are selected based on
whether a test is enabled and whether the test name matches a regex that the roottask
is configured with. Tests are expected to automatically select whether they are enabled
based on the build configuration at build time. The regex is used for further filtering
which tests are run. The default regex is `.*` for selecting all enabled tests.

### Test running

Tests are run sequentially and their test environments are reset between each test run.
The roottask can be configured whether to stop or continue running on test failure conditions.
A test failure shouldn't result in the entire application crashing. This isn't enforced
by most test environments as many of the tests are testing kernel mechanisms and may
require permissions that allow them to crash the system. A test needs to be written such
that this outcome is minimized.

### Reporting results

Test results are reported via functions that the test calls which are defined by a
common testing API. The roottask can choose how to report the results of a test
based on its configuration. Some reporting formats should be machine-parsable to support
test running automation. Human readable formats should also be available.

## See also

seL4bench is another application similar to seL4test that is used for running
benchmarks on seL4.
