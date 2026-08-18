#ifndef KTEST2_HPP
#define KTEST2_HPP

#include <source_location>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <iostream>
#include <unordered_set>

#define KTEST2_TESTS \
	namespace ktest2_tests { static void _ktest2_register_tests(); } \
	static ktest2::impl::TestRegistrar _ktest2_registrar { ktest2_tests::_ktest2_register_tests }; \
	void ktest2_tests::_ktest2_register_tests()

#ifndef KTEST2_DISABLE_FORMATTERS
#include <cstddef>
#include <format>
#include <array>
#include <version>

// Byte is not formattable by default and can't cast to unsigned char (treats as string), so need a custom formatter
template<>
struct std::formatter<std::byte> : std::formatter<unsigned> {
	auto format(std::byte b, auto& ctx) const {
		return std::formatter<unsigned>::format(std::to_integer<unsigned>(b), ctx);
	}
};

#if !defined(__cpp_lib_format_ranges) || __cpp_lib_format_ranges < 202207
template<std::ranges::input_range R>
	requires (!std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, R>)
		&& std::formattable<std::ranges::range_value_t<R>, char>
struct std::formatter<R, char>
	: std::formatter<std::remove_cvref_t<std::ranges::range_reference_t<R>>, char> {
	using T = std::remove_cvref_t<std::ranges::range_reference_t<R>>;
	using base = std::formatter<T, char>;

	auto format(const R& r, auto& ctx) const {
		auto out = ctx.out();
		out = std::format_to(out, "[");
		bool first = true;
		for (auto&& e : r) {
			if (!first) {
				out = std::format_to(out, ", ");
			}
			first = false;
			ctx.advance_to(out);
			out = base::format(e, ctx);
		}
		return std::format_to(out, "]");
	}
};
#endif // #if !defined(__cpp_lib_format_ranges) || __cpp_lib_format_ranges < 202207

#endif // #ifndef KTEST2_DISABLE_FORMATTERS

namespace ktest2 {

struct Path {
	std::string path;

	Path join(std::string child) const {
		return path.empty() ? Path { std::move(child) } : Path { path + "/" + child };
	}
};

struct TestContext {
	Path path;
	std::function<void()> run;
};

struct AssertionFailure {
	std::string message;
	std::string detail;
	std::source_location source_location;
};

struct TestVisitor {
	virtual ~TestVisitor() = default;
	virtual void enter_suite(Path) {}
	virtual void exit_suite() {}
	virtual void visit_test(TestContext) {}

	virtual void visit_assert_failed(AssertionFailure) {}
	virtual void visit_expect_failed(AssertionFailure) {}
	virtual void visit_unhandled_exception(const std::exception&) {}
	virtual void visit_unknown_exception() {}
};

namespace impl {

struct TestSuite {
	TestSuite(std::string name, TestSuite& parent)
		: path{parent.path.join(name)}
		, parent{&parent}
	{}

	TestSuite() {}

	TestSuite* parent = nullptr;

	Path path;
	size_t num_tests = 0;
	size_t num_suites = 0;

	std::unordered_set<std::string> test_case_name_set;
	std::unordered_set<std::string> test_suite_name_set;
	std::vector<std::string> test_case_names;
	std::vector<std::string> test_suite_names;

	std::vector<std::function<void()>> before_each_functions;
	std::vector<std::function<void()>> after_each_functions;

	void add_case(std::string name) {
		if (test_case_name_set.contains(name)) {
			std::println(std::cerr, "Suite '{}' contains duplicate test case name '{}'", path.path, name);
		} else {
			test_case_name_set.insert(name);
		}

		test_case_names.push_back(name);
		num_tests += 1;
	}

	void add_suite(std::string name) {
		if (test_suite_name_set.contains(name)) {
			std::println(std::cerr, "Suite '{}' contains duplicate test suite '{}'", path.path, name);
		} else {
			test_suite_name_set.insert(name);
		}

		test_suite_names.push_back(name);
		num_suites += 1;
	}

	void validate() {
		if (num_suites == 0 && num_tests == 0) {
			std::println(std::cerr, "Suite '{}' has no tests", path.path);
		}
	}

	void before_each() {
		for (auto& f : before_each_functions) {
			f();
		}
		if (parent) parent->before_each();
	}

	void after_each() {
		if (parent) parent->after_each();
		for (auto it = after_each_functions.rbegin(); it != after_each_functions.rend(); ++it) {
			(*it)();
		}
	}
};

inline std::vector<std::unique_ptr<TestSuite>> test_suite_stack; 

struct TestRegistrar {
	using RegistrationFunction = void(*)();

	inline TestRegistrar(RegistrationFunction f) {
		get_registration_functions().push_back(f);
	}

	static inline void run_tests() {
		test_suite_stack.push_back(std::make_unique<TestSuite>());
		for (auto& f : get_registration_functions()) {
			f();
		}
		test_suite_stack.pop_back();
	}

	static inline std::vector<RegistrationFunction>& get_registration_functions() {
		static std::vector<RegistrationFunction> registration_functions;
		return registration_functions;
	}
};

inline TestVisitor* test_visitor = nullptr;

template<class U> requires(std::formattable<U, char>)
decltype(auto) printable(const U& x) {
	return static_cast<const U&>(x);
}

template<class U>
[[deprecated("Unprintable type")]]
constexpr std::string_view unprintable() { return "<unprintable>"; }

template<class U> requires(!std::formattable<U, char>)
decltype(auto) printable(const U& x) {
	return unprintable<U>();
}

} // namespace impl

inline void describe(std::string name, std::function<void()> body) {
	auto& parent_suite = impl::test_suite_stack.back();
	parent_suite->add_suite(name);
	impl::test_suite_stack.push_back(std::make_unique<impl::TestSuite>(
		name,
		*parent_suite
	));
	auto& suite = impl::test_suite_stack.back();
	impl::test_visitor->enter_suite(suite->path);
	struct Pop {~Pop() {
		const auto& suite = impl::test_suite_stack.back();
		suite->validate();
		impl::test_suite_stack.pop_back();
		impl::test_visitor->exit_suite();
	}} pop;
	body();
}

inline void it(std::string description, std::function<void()> body) {
	auto& suite = impl::test_suite_stack.back();
	suite->add_case(description);
	Path path = suite->path.join(description);

	impl::test_visitor->visit_test(TestContext {
		std::move(path),
		[&]() {
			suite->before_each();
			try {
				body();
			} catch (const AssertionFailure& failure) {
				impl::test_visitor->visit_assert_failed(failure);
			} catch(const std::exception& e) {
				impl::test_visitor->visit_unhandled_exception(e);
			} catch(...) {
				impl::test_visitor->visit_unknown_exception();
			}
			suite->after_each();
		}
	});
}

template<typename T, bool Fatal>
struct Assert {
	Assert(const T& value, std::source_location source_location)
		: value{value}
		, source_location{source_location}
	{}

	const T& value;
	std::source_location source_location;
	bool negated = false;

	Assert& not_() {
		negated = !negated;
		return *this;
	}

	template<typename U>
	void to_be(const U& other, std::string msg = "") {
		if (negated) {
			negated = false;
			return not_to_be(other, msg);
		}
		if ((value == other) == negated) {
			fail(msg, format("{} == {}", value, other));
		}
	}

	template<typename U>
	void not_to_be(const U& other, std::string msg = "") {
		if (negated) {
			negated = false;
			return to_be(other, msg);
		}
		if ((value != other) == negated) {
			fail(msg, format("{} != {}", value, other));
		}
	}

private:
	template<class... Args>
	std::string format(
		std::format_string<decltype(impl::printable(std::declval<Args&>()))...> fmt,
		Args&&... args
	) {
		std::string s = std::format(fmt, impl::printable(args)...);
		return negated ? std::format("not({})", s) : s;
	}	

	void fail(std::string message, std::string detail) {
		AssertionFailure failure {
			message,
			detail,
			source_location
		};
		if constexpr (Fatal) {
			throw failure;
		} else {
			impl::test_visitor->visit_expect_failed(failure);
		}
	}
};

template<typename T>
Assert<T, true> require(const T& value, std::source_location loc = std::source_location::current()) {
	return Assert<T, true>(value, loc);
}

template<typename T>
Assert<T, false> expect(const T& value, std::source_location loc = std::source_location::current()) {
	return Assert<T, false>(value, loc);
}


inline void before_each(std::function<void()> f) {
	impl::test_suite_stack.back()->before_each_functions.push_back(f);
}

inline void after_each(std::function<void()> f) {
	impl::test_suite_stack.back()->after_each_functions.push_back(f);
}


inline void visit(TestVisitor& visitor) {
	impl::test_visitor = &visitor;
	impl::TestRegistrar::run_tests();
	impl::test_visitor = nullptr;
}

}

namespace ktest2_tests {
	using ktest2::describe;
	using ktest2::it;

	using ktest2::before_each;
	using ktest2::after_each;

	using ktest2::require;
	using ktest2::expect;
}

#ifdef KTEST2_MAIN

#include <string_view>
#include <print>
#include <regex>
#include <algorithm>

#if defined(_WIN32)
	#include <io.h>
	#include <windows.h>
	static bool is_tty() {
		return _isatty(_fileno(stdout));
	}
	static void enable_ansi() {
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD mode = 0;
		if (GetConsoleMode(h, &mode))
			SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
#else
	#include <unistd.h>
	static bool is_tty() {
		return isatty(fileno(stdout));
	}
	static void enable_ansi() {}
#endif

static bool tty = false;

#define ANSI_RED (tty ? "\x1b[31m" : "")
#define ANSI_GREEN (tty ? "\x1b[32m" : "")
#define ANSI_RESET (tty ? "\x1b[0m" : "")

static std::regex glob_to_regex(std::string_view glob) {
	std::string re = "^";
	for (size_t i = 0; i < glob.size(); ++i) {
		char c = glob[i];
		if (c == '*' && i + 1 < glob.size() && glob[i + 1] == '*') {
			re += ".*";
			i += 1;
			continue;
		}
		switch (c) {
			case '*': 
				re += "[^/]*";
				break;
			case '?':
				re += "[^/]";
				break;
			case '.': case '+': case '(': case ')':
			case '[': case ']': case '{': case '}':
			case '^': case '$': case '\\': case '|':
				re += '\\';
				re += c;
				break;
			default:
				re += c;
		}
	}
	re += "$";
	return std::regex{re};
}

static void print_help(std::string_view exec_name) {
	std::println("Usage: {}", exec_name);
	std::println("\t-h, --help\n\t\tprints help");
}

struct Filter {
	std::vector<std::regex> include;
	std::vector<std::regex> exclude;

	static bool any_match(std::span<const std::regex> regular_expressions, std::string_view value) {
		return std::ranges::any_of(regular_expressions, [&](const std::regex& re) {
			return std::regex_match(value.begin(), value.end(), re);
		});
	}

	bool is_included(std::string_view sv) const {
		return (
			(include.empty() || any_match(include, sv))
			&& !any_match(exclude, sv)
		);
	}
};

struct PrintTestSuitesVisitor : ktest2::TestVisitor {
	Filter filter;

	std::vector<size_t> test_count_stack;
	std::vector<ktest2::Path> suite_path_stack;
	size_t test_count = 0;

	std::string lines;
	std::vector<std::string> lines_stack;

	void enter_suite(ktest2::Path path) override {
		if(!filter.is_included(path.path)) return;
		test_count_stack.push_back(test_count);
		suite_path_stack.push_back(std::move(path));
		lines_stack.push_back(std::move(lines));

		lines.clear();
		test_count = 0;
	};

	void exit_suite() override {
		auto& path = suite_path_stack.back();
		auto& old_lines = lines_stack.back();
		old_lines += std::format(
			"{} - {} case{}\n",
			path.path,
			test_count,
			test_count == 1 ? "" : "s"
		);
		old_lines += lines;

		lines = std::move(old_lines);
		lines_stack.pop_back();

		test_count += test_count_stack.back();
		test_count_stack.pop_back();
		suite_path_stack.pop_back();
	}

	void visit_test(ktest2::TestContext context) override {
		test_count += 1;
	};
};

struct RunTestsVisitor : ktest2::TestVisitor {
	Filter filter;

	size_t num_tests = 0;
	size_t num_passed = 0;
	bool test_passed = true;
	std::string test_failures;

	void visit_test(ktest2::TestContext ctx) override {
		if(!filter.is_included(ctx.path.path)) return;
		std::print("{}: ", ctx.path.path);
		test_passed = true;
		test_failures = "\n";
		num_tests += 1;

		ctx.run();

		if (test_passed) {
			std::println("{}PASSED{}", ANSI_GREEN, ANSI_RESET);
			num_passed += 1;
		} else {
			std::print("{}FAILED{}{}", ANSI_RED, ANSI_RESET, test_failures);
		}
	}

	void visit_assert_failed(ktest2::AssertionFailure failure) override {
		test_passed = false;
		test_failures += format_failure(true, failure);
	}

	void visit_expect_failed(ktest2::AssertionFailure failure) override {
		test_passed = false;
		test_failures += format_failure(false, failure);
	}

	void visit_unhandled_exception(const std::exception& e) override {
		test_passed = false;
		test_failures += std::format("\tUNHANDLED EXCEPTION:\n\t\t{}\n", e.what());
	}

	void visit_unknown_exception() override {
		test_passed = false;
		test_failures += std::format("\tUNKNOWN EXCEPTION\n");
	}

	std::string format_failure(bool is_assertion, const ktest2::AssertionFailure& failure) {
		return std::format(
			"\t{}{} FAILED{} ({}:{})\n{}{}",
			ANSI_RED,
			is_assertion ? "ASSERTION" : "EXPECTATION",
			ANSI_RESET,
			failure.source_location.file_name(),
			failure.source_location.line(),
			failure.message.empty() ? failure.message : std::format("\t\t\"{}\"\n", failure.message),
			failure.detail.empty() ? failure.detail : std::format("\t\t{}\n", failure.detail)
		);
	}
};

int main(int argc, char* argv_raw[]) {
	std::span<char*> argv(argv_raw, static_cast<size_t>(argc));

	tty = is_tty();
	if (tty) {
		enable_ansi();
	}

	bool list = false;
	bool rest_is_patterns = false;
	bool next_is_include_regex = false;
	bool next_is_exclude_regex = false;
	bool next_is_include_pattern = false;
	bool next_is_exclude_pattern = false;

	Filter filter;

	std::string_view exec_name = argv[0];
	for (int i = 1; i < argc; ++i) {
		std::string_view arg = argv[i];
		if (rest_is_patterns || next_is_include_pattern) {
			filter.include.push_back(glob_to_regex(arg));
			next_is_include_pattern = false;
			continue;
		}
		if (next_is_exclude_pattern) {
			filter.exclude.push_back(glob_to_regex(arg));
			next_is_exclude_pattern = false;
			continue;
		}

		if(next_is_include_regex) {
			filter.exclude.push_back(std::regex{std::string(arg)});
			next_is_include_regex = false;
			continue;
		}
		if(next_is_exclude_regex) {
			filter.exclude.push_back(std::regex{std::string(arg)});
			next_is_exclude_regex = false;
			continue;
		}

		if (arg == "-l" || arg == "--list") {
			list = true;
			continue;
		}

		if (arg == "-i" || arg == "--include") {
			next_is_include_pattern = true;
			continue;
		}

		if (arg == "-I" || arg == "--include-regex") {
			next_is_include_regex = true;
			continue;
		}

		if (arg == "-x" || arg == "--exclude") {
			next_is_exclude_pattern = true;
			continue;
		}

		if (arg == "-X" || arg == "--exclude-regex") {
			next_is_exclude_regex = true;
			continue;
		}

		if (arg == "-h" || arg == "--help") {
			print_help(exec_name);
			return 0;
		}

		if (arg == "--") {
			rest_is_patterns = true;
			continue;
		}

		if (arg.starts_with('-')) {
			std::println("Error: Unknown argument: {}", arg);
			print_help(exec_name);
			return 1;
		}

		std::string glob = std::format("**{}**", arg);
		filter.include.push_back(glob_to_regex(glob));
	}
	bool nexts_remaining = next_is_include_pattern;
	nexts_remaining |= next_is_exclude_pattern;
	nexts_remaining |= next_is_include_regex;
	nexts_remaining |= next_is_exclude_regex;
	if (nexts_remaining) {
		std::println("Error: Expected argument after '{}'", argv.back());
		print_help(exec_name);
		return 1;
	}

	if (list) {
		PrintTestSuitesVisitor visitor;
		visitor.filter = filter;
		ktest2::visit(visitor);
		if (visitor.lines.empty()) {
			std::println("No test cases");
		} else {
			std::print("{}", visitor.lines);
		}
		return 0;
	}

	RunTestsVisitor visitor;
	visitor.filter = filter;
	ktest2::visit(visitor);
	std::println("{}/{} test cases passed", visitor.num_passed, visitor.num_tests);
	return 0;
}

#endif
#endif