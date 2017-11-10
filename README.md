<!--
 Copyright 2017, Data61
 Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 ABN 41 687 119 230.

 This software may be distributed and modified according to the terms of
 the BSD 2-Clause license. Note that NO WARRANTY is provided.
 See "LICENSE_BSD2.txt" for details.

 @TAG(DATA61_BSD)
-->

# sel4test

Library for creating and running tests for seL4.

## Usage

*Small unit tests* can be defined anywhere, such as libraries outside of `sel4test` or in `sel4test-driver`. *Larger tests* that do things like creating processes need to be declared inside `sel4test-tests`.

### Unit tests
To define a small unit test in a library outside of `sel4test` or in `sel4test-driver`:
1. Declare `libsel4test` as a dependency for your library and include `<sel4test/test.h>`. You may also find the functions in `<sel4test/testutil.h>` handy.
2. Write your tests. Then, for each test you want to run, call one of the macros that define a test, such as the `DEFINE_TEST` macro. They are declared
[here](https://github.com/seL4/seL4_libs/blob/master/libsel4test/include/sel4test/test.h#L88).
3. Add your library as dependency to
[`libsel4testsupport`](https://github.com/seL4/sel4test/blob/master/libsel4testsupport).
Add a call to any function in your test file to `testreporter.c` in [`dummy_func()`](https://github.com/seL4/sel4test/blob/master/libsel4testsupport/src/testreporter.c#L35). If you have multiple test files, then you need to call one function for each test file.

For an example, take a look at [`libsel4serialserver/src/test.c`](https://github.com/seL4/seL4_libs/blob/master/libsel4serialserver/src/test.c) in `sel4_libs`.

### Assumptions
Currently unit tests are assumed to be running sequentially, standalone (i.e. not multi-threaded).
Some tests rely on being the highest priority running thread in the system.

### Other tests
To define a larger test in `sel4test-tests`:
1. Place your test in `apps/sel4test-tests/src/tests`.
2. Include `<../helpers.h>`.
3. Write your tests. Then, for each test you want to run, call one of the macros that define a test,
    such as the `DEFINE_TEST` macro. They are declared [here](https://github.com/seL4/seL4_libs/blob/master/libsel4test/include/sel4test/test.h#L88).

For an example, take a look at [`trivial.c`](https://github.com/seL4/sel4test/blob/master/apps/sel4test-tests/src/tests/trivial.c) in `sel4test`.

# License
The files in this repository are release under standard open source licenses. Please see individual file headers and the `LICENSE_BSD2`.txt file for details.
