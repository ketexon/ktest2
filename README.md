# ktest2

Ketexon's small testing library (version 2) written in C++23.

Rewritten for a better API and better naming conventions.

## Installation

ktest2 does not require any dependencies other than the C++ standard library. It does require C++23.

### CMake

```cmake
include(FetchContent)

FetchContent_Declare(
    ktest2
    URL https://github.com/ketexon/ktest2/archive/refs/heads/main.zip # or whatever head/tag you want
)

FetchContent_MakeAvailable(ktest2)

target_link_libraries(
    <your_target>
    PRIVATE ktest2_with_main
)
```

This automatically defines a main function. If you want to have your own main function, you can link against `ktest2` instead. See [usage](#manually-running-test-cases) for more details.

## Usage

### Basic Usage

This is basically just jest lol

```cpp
#include <ktest2/ktest2.hpp>

int factorial(int n) {
	if (n < 0) {
		throw std::invalid_argument("negative");
	}
	int r = 1;
	for (int i = 2; i <= n; ++i) {
		r *= i;
	}
	return r;
}

KTEST2_TESTS {
	describe("factorial", []() {
		describe("factorial", [] {
			describe("base cases", [] {
				it("0! == 1", [] { expect(factorial(0)).to_be(1); });
				it("1! == 1", [] { expect(factorial(1)).to_be(1); });
			});

			describe("recursive cases", [] {
				it("2! == 2", [] { expect(factorial(2)).to_be(2); });
				it("3! == 6", [] { expect(factorial(3)).to_be(6); });
				it("5! == 120", [] { expect(factorial(5)).to_be(120); });
				it("10! == 3628800",
				   [] { expect(factorial(10)).to_be(3628800); });
			});

			describe("properties", [] {
				it("n! == n * (n-1)!", [] {
					for (int n = 1; n <= 10; ++n)
						expect(factorial(n)).to_be(n * factorial(n - 1));
				});
			});
		});
	});
}
```

This creates a CLI that allows you to run test cases. You can use the `-h` flag to see the available options.

```bash
./your_program
Running 4 tests
Test/should use before each to set x: PASSED
Test/happens: FAILED
        EXPECTATION FAILED (/home/ketexon/programming/c/ktest2/unit/basic.cpp:17)
                "This will fail, but won't stop the test!"
                true == false
        ASSERTION FAILED (/home/ketexon/programming/c/ktest2/unit/basic.cpp:19)
                "My message!"
                false != false
Test/When pigs fly/never happens: PASSED
Test/When pigs fly/sometimes happens: PASSED
factorial/factorial/base cases/0! == 1: PASSED
factorial/factorial/base cases/1! == 1: PASSED
factorial/factorial/recursive cases/2! == 2: PASSED
factorial/factorial/recursive cases/3! == 6: PASSED
factorial/factorial/recursive cases/5! == 120: PASSED
factorial/factorial/recursive cases/10! == 3628800: PASSED
factorial/factorial/properties/n! == n * (n-1)!: PASSED
10/11 test cases passed
```

You can cherry-pick test cases by passing the *identifier* as a positional argument, which will do a substring match.

```bash
./your_program Factorial0 Factorial2
Running 2 tests
Running test case Factorial0: Factorial of 0 is 1... PASSED
Running test case Factorial2: Factorial of 2 is 2... PASSED
Passed: 2/2
```

### API

TODO

### CLI

TODO

### Internals

`KTEST2_TESTS` just creates a function and registers it to a global vector of functions.

When you call these function, the library uses a visitor pattern to decide what to do with `describe`/`it` calls.

The `it`'s body has some special visitors for when an assertion fails, expectation fails, or other exceptions happen.