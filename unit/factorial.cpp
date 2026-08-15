#undef KTEST2_MAIN
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