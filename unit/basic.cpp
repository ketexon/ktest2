#undef KTEST2_MAIN
#include <ktest2/ktest2.hpp>

using namespace ktest2;

KTEST2_TESTS {
    describe("Test", []() {
        int x;
        before_each([&x]() {
            x = 10;
        });

        it("should use before each to set x", [&x]() {
            require(x).to_be(10);
        });

        it("happens", []() {
            expect(true).to_be(false, "This will fail, but won't stop the test!");
            require(false).to_be(false);
            require(false).not_().to_be(false, "My message!");
            require(true).to_be(true, "This won't print");
        });

        describe("When pigs fly", []() {
            it("never happens", [](){});
            it("sometimes happens", [](){});
        });
    });
}