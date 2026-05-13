// challenges_test.cpp
// Compile & run: g++ -std=c++17 -I answers/ challenges_test.cpp -o challenges_test && ./challenges_test
//
// Each test compiles the corresponding answer file as an #include with
// its own main() renamed, or tests the expected behavior via subprocess.
// Since each answer file has its own main(), we test by compiling and
// running each one separately via system().
//
// Usage:
//   g++ -std=c++17 challenges_test.cpp -o challenges_test && ./challenges_test
//
// Prerequisites: answer files in answers/ directory:
//   1_enum.cpp, 2_addr.cpp, 3_rvaluelvalue.cpp, 4_move.cpp, 5_sensor.cpp,
//   6_crtp.cpp, 7_duck.cpp, 8_layout.cpp, 9_regions.cpp, 10_buffer.cpp,
//   11_memcpy.cpp, 12_smart.cpp, 14_compute.cpp, 16_factorial.cpp,
//   17_cache.cpp, 18_volatile.cpp, 20_structclass.cpp

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <array>
#include <vector>

// ── helpers ──────────────────────────────────────────────────────────────

static int pass_count = 0;
static int fail_count = 0;
static int skip_count = 0;

static bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// Compile a single .cpp, return true if compilation succeeded
static bool compile(const std::string& src, const std::string& out,
                    const std::string& extra_flags = "") {
    std::string cmd = "g++ -std=c++17 " + extra_flags + " " + src +
                      " -o " + out + " 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

// Run a binary and capture stdout
static std::string run_and_capture(const std::string& bin,
                                   const std::string& input = "") {
    std::string cmd = bin;
    if (!input.empty()) {
        cmd = "echo '" + input + "' | " + cmd;
    }
    cmd += " 2>/dev/null";

    std::array<char, 4096> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        result += buffer.data();
    pclose(pipe);
    return result;
}

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

static void report(int q, const std::string& name, bool passed) {
    if (passed) {
        std::cout << "  [PASS] Q" << q << " — " << name << std::endl;
        pass_count++;
    } else {
        std::cout << "  [FAIL] Q" << q << " — " << name << std::endl;
        fail_count++;
    }
}

static void skip(int q, const std::string& name) {
    std::cout << "  [SKIP] Q" << q << " — " << name << " (file not found)" << std::endl;
    skip_count++;
}

// ── per-question tests ──────────────────────────────────────────────────

void test_q1() {
    const std::string src = "answers/1_enum.cpp";
    if (!file_exists(src)) { skip(1, "enum cast"); return; }
    if (!compile(src, "/tmp/q1_test")) { report(1, "enum cast (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q1_test");
    // Should print 0, 1, 2 (in some form)
    bool ok = contains(out, "0") && contains(out, "1") && contains(out, "2");
    report(1, "enum cast", ok);
}

void test_q2() {
    const std::string src = "answers/2_addr.cpp";
    if (!file_exists(src)) { skip(2, "hardware register"); return; }
    // Q2 writes to a fake address — just check it compiles
    bool ok = compile(src, "/tmp/q2_test");
    report(2, "hardware register (compiles)", ok);
}

void test_q3() {
    const std::string src = "answers/3_rvaluelvalue.cpp";
    if (!file_exists(src)) { skip(3, "rvalue/lvalue"); return; }
    if (!compile(src, "/tmp/q3_test")) { report(3, "rvalue/lvalue (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q3_test");
    bool ok = contains(out, "lvalue") && contains(out, "rvalue");
    report(3, "rvalue/lvalue", ok);
}

void test_q4() {
    const std::string src = "answers/4_move.cpp";
    if (!file_exists(src)) { skip(4, "std::move vector"); return; }
    if (!compile(src, "/tmp/q4_test")) { report(4, "std::move vector (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q4_test");
    // After move: v1 size should be 0, v2 size should be 1000000
    bool ok = contains(out, "0") && contains(out, "1000000");
    report(4, "std::move vector", ok);
}

void test_q5() {
    const std::string src = "answers/5_sensor.cpp";
    if (!file_exists(src)) { skip(5, "polymorphism ISensor"); return; }
    if (!compile(src, "/tmp/q5_test")) { report(5, "polymorphism ISensor (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q5_test");
    bool ok = contains(out, "36.5") && contains(out, "-1");
    report(5, "polymorphism ISensor", ok);
}

void test_q6() {
    const std::string src = "answers/6_crtp.cpp";
    if (!file_exists(src)) { skip(6, "CRTP sensor"); return; }
    if (!compile(src, "/tmp/q6_test")) { report(6, "CRTP sensor (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q6_test");
    bool ok = contains(out, "36.5") && contains(out, "-1");
    report(6, "CRTP sensor", ok);
}

void test_q7() {
    const std::string src = "answers/7_duck.cpp";
    if (!file_exists(src)) { skip(7, "duck typing"); return; }
    if (!compile(src, "/tmp/q7_test")) { report(7, "duck typing (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q7_test");
    // Should print each struct's to_string() twice
    bool ok = out.length() > 0;  // basic: produced output
    report(7, "duck typing", ok);
}

void test_q8() {
    const std::string src = "answers/8_layout.cpp";
    if (!file_exists(src)) { skip(8, "memory layout"); return; }
    if (!compile(src, "/tmp/q8_test")) { report(8, "memory layout (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q8_test");
    // sizeof Packet should be 24, offsets: 0, 8, 16
    bool ok = contains(out, "24") && contains(out, "0") && contains(out, "8") && contains(out, "16");
    report(8, "memory layout", ok);
}

void test_q9() {
    const std::string src = "answers/9_regions.cpp";
    if (!file_exists(src)) { skip(9, "memory regions"); return; }
    if (!compile(src, "/tmp/q9_test")) { report(9, "memory regions (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q9_test");
    // Should print addresses (contain 0x or some hex)
    bool ok = out.length() > 0;
    report(9, "memory regions", ok);
}

void test_q10() {
    const std::string src = "answers/10_buffer.cpp";
    if (!file_exists(src)) { skip(10, "Buffer copy/move"); return; }
    if (!compile(src, "/tmp/q10_test")) { report(10, "Buffer copy/move (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q10_test");
    // Should mention copy and move in output
    std::string lower = out;
    for (auto& c : lower) c = tolower(c);
    bool ok = contains(lower, "copy") && contains(lower, "move");
    report(10, "Buffer copy/move", ok);
}

void test_q11() {
    const std::string src = "answers/11_memcpy.cpp";
    if (!file_exists(src)) { skip(11, "memcpy UB"); return; }
    // Just check it compiles — running it would be UB
    bool ok = compile(src, "/tmp/q11_test");
    report(11, "memcpy UB (compiles)", ok);
}

void test_q12() {
    const std::string src = "answers/12_smart.cpp";
    if (!file_exists(src)) { skip(12, "unique_ptr"); return; }
    if (!compile(src, "/tmp/q12_test")) { report(12, "unique_ptr (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q12_test");
    bool ok = contains(out, "36.5");
    report(12, "unique_ptr", ok);
}

void test_q13() {
    // Q13 is shell commands — check if answers/13_build.sh exists
    const std::string src = "answers/13_build.sh";
    if (!file_exists(src)) { skip(13, "build pipeline"); return; }
    // Just verify the script file exists and contains g++ commands
    std::ifstream f(src);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    bool ok = contains(content, "g++") && contains(content, "-E") &&
              contains(content, "-c") && contains(content, "-o");
    report(13, "build pipeline", ok);
}

void test_q14() {
    const std::string src = "answers/14_compute.cpp";
    if (!file_exists(src)) { skip(14, "shared library"); return; }
    // Check it compiles as a shared library
    bool ok = compile(src, "/tmp/libcompute_test.dylib", "-shared -fPIC");
    report(14, "shared library (compiles to .dylib)", ok);
}

void test_q15() {
    // Q15 involves multiple files — check directory exists
    bool has_dir = file_exists("answers/15_inline/helper.h");
    if (!has_dir) {
        // Also accept a single file
        if (file_exists("answers/15_inline.cpp")) {
            bool ok = compile("answers/15_inline.cpp", "/tmp/q15_test");
            report(15, "inline ODR", ok);
        } else {
            skip(15, "inline ODR");
        }
        return;
    }
    // Try compiling with inline version
    bool ok = compile("answers/15_inline/app1.cpp answers/15_inline/app2.cpp",
                      "/tmp/q15_test", "-I answers/15_inline/");
    report(15, "inline ODR", ok);
}

void test_q16() {
    const std::string src = "answers/16_factorial.cpp";
    if (!file_exists(src)) { skip(16, "constexpr factorial"); return; }
    if (!compile(src, "/tmp/q16_test")) { report(16, "constexpr factorial (compile)", false); return; }
    // static_assert passes at compile time; run with input "5"
    std::string out = run_and_capture("/tmp/q16_test", "5");
    bool ok = contains(out, "120");
    report(16, "constexpr factorial", ok);
}

void test_q17() {
    const std::string src = "answers/17_cache.cpp";
    if (!file_exists(src)) { skip(17, "mutable cache"); return; }
    if (!compile(src, "/tmp/q17_test")) { report(17, "mutable cache (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q17_test");
    bool ok = out.length() > 0;
    report(17, "mutable cache", ok);
}

void test_q18() {
    const std::string src = "answers/18_volatile.cpp";
    if (!file_exists(src)) { skip(18, "volatile"); return; }
    bool ok = compile(src, "/tmp/q18_test");
    report(18, "volatile (compiles)", ok);
}

void test_q19() {
    bool has_dir = file_exists("answers/19_guards/config.h");
    if (!has_dir) {
        if (file_exists("answers/19_guards.cpp")) {
            bool ok = compile("answers/19_guards.cpp", "/tmp/q19_test");
            report(19, "header guards", ok);
        } else {
            skip(19, "header guards");
        }
        return;
    }
    // Compile the fixed version with guards
    bool ok = compile("answers/19_guards/main.cpp", "/tmp/q19_test",
                      "-I answers/19_guards/");
    report(19, "header guards", ok);
}

void test_q20() {
    const std::string src = "answers/20_structclass.cpp";
    if (!file_exists(src)) { skip(20, "struct vs class"); return; }
    if (!compile(src, "/tmp/q20_test")) { report(20, "struct vs class (compile)", false); return; }
    std::string out = run_and_capture("/tmp/q20_test");
    bool ok = out.length() > 0;
    report(20, "struct vs class", ok);
}

// ── main ─────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== C++ Challenges Test Runner ===" << std::endl;
    std::cout << std::endl;

    test_q1();
    test_q2();
    test_q3();
    test_q4();
    test_q5();
    test_q6();
    test_q7();
    test_q8();
    test_q9();
    test_q10();
    test_q11();
    test_q12();
    test_q13();
    test_q14();
    test_q15();
    test_q16();
    test_q17();
    test_q18();
    test_q19();
    test_q20();

    std::cout << std::endl;
    std::cout << "=== Results ===" << std::endl;
    std::cout << "  PASS: " << pass_count << std::endl;
    std::cout << "  FAIL: " << fail_count << std::endl;
    std::cout << "  SKIP: " << skip_count << std::endl;
    std::cout << "  TOTAL: " << (pass_count + fail_count + skip_count) << "/20" << std::endl;

    return fail_count > 0 ? 1 : 0;
}
