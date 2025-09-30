#include <stdio.h>
#include <math.h>
#include "tf32.h"

// put_tf32 helper function (from main.c)
void put_tf32(tf32 in) {
    int i;
    for (i = 0; i < 19; ++i) {
        putchar((in & 0x40000U) ? '1' : '0');
        in <<= 1;
    }
}

#define TEST_SECTION(name) printf("\n" \
    "================================================================\n" \
    "  %s\n" \
    "================================================================\n", name)

#define TEST_CASE(desc) printf("\n[TEST] %s\n", desc)

#define ASSERT_EQ(actual, expected, desc) do { \
    if ((actual) == (expected)) { \
        printf("  ✓ PASS: %s (0x%05x)\n", desc, (unsigned int)(actual)); \
    } else { \
        printf("  ✗ FAIL: %s\n", desc); \
        printf("    Expected: 0x%05x\n", (unsigned int)(expected)); \
        printf("    Got:      0x%05x\n", (unsigned int)(actual)); \
    } \
} while(0)

#define ASSERT_INT_EQ(actual, expected, desc) do { \
    if ((actual) == (expected)) { \
        printf("  ✓ PASS: %s (%d)\n", desc, (actual)); \
    } else { \
        printf("  ✗ FAIL: %s\n", desc); \
        printf("    Expected: %d\n", (expected)); \
        printf("    Got:      %d\n", (actual)); \
    } \
} while(0)

void test_int2tf32() {
    TEST_SECTION("int2tf32 - Integer to TF32 Conversion");
    
    TEST_CASE("Zero");
    ASSERT_EQ(int2tf32(0), 0x00000, "0 -> 0");
    
    TEST_CASE("Positive integers");
    ASSERT_EQ(int2tf32(1), 0x1fc00, "1");
    ASSERT_EQ(int2tf32(37), 0x210a0, "37");
    ASSERT_EQ(int2tf32(3001), 0x229dc, "3001");
    
    TEST_CASE("Negative integers");
    ASSERT_EQ(int2tf32(-1), 0x5fc00, "-1");
    ASSERT_EQ(int2tf32(-13), 0x60a80, "-13");
    
    TEST_CASE("Boundary values");
    ASSERT_EQ(int2tf32(-2147483648), 0x67800, "INT_MIN (-2^31)");
    ASSERT_EQ(int2tf32(2147483647), 0x27800, "INT_MAX (2^31-1)");
    
    TEST_CASE("Powers of 2");
    ASSERT_EQ(int2tf32(2), 0x20000, "2^1");
    ASSERT_EQ(int2tf32(4), 0x20400, "2^2");
    ASSERT_EQ(int2tf32(1024), 0x22400, "2^10");
}

void test_tf322int() {
    TEST_SECTION("tf322int - TF32 to Integer Conversion");
    
    TEST_CASE("Zero");
    ASSERT_INT_EQ(tf322int(0x00000), 0, "+0");
    ASSERT_INT_EQ(tf322int(0x40000), 0, "-0");
    
    TEST_CASE("Positive integers");
    ASSERT_INT_EQ(tf322int(0x210a0), 37, "37");
    ASSERT_INT_EQ(tf322int(0x23800), 32768, "32768");
    
    TEST_CASE("Negative integers");
    ASSERT_INT_EQ(tf322int(0x60a80), -13, "-13");
    ASSERT_INT_EQ(tf322int(0x63e00), -98304, "-98304");
    
    TEST_CASE("Special values");
    ASSERT_INT_EQ(tf322int(0x3fc00), 0x7FFFFFFF, "+Inf -> INT_MAX");
    ASSERT_INT_EQ(tf322int(0x7fc00), 0x80000000, "-Inf -> INT_MIN");
    ASSERT_INT_EQ(tf322int(0x3FE00), 0x80000000, "NaN -> INT_MIN");
    
    TEST_CASE("Denormalized numbers");
    ASSERT_INT_EQ(tf322int(0x00001), 0, "Denorm -> 0");
    ASSERT_INT_EQ(tf322int(0x003ff), 0, "Max denorm -> 0");
}

void test_double2tf32() {
    TEST_SECTION("double2tf32 - Double to TF32 Conversion");
    
    TEST_CASE("Zero");
    ASSERT_EQ(double2tf32(0.0), 0x00000, "+0.0");
    ASSERT_EQ(double2tf32(-0.0), 0x40000, "-0.0");
    
    TEST_CASE("Normal numbers");
    ASSERT_EQ(double2tf32(1.0), 0x1fc00, "1.0");
    ASSERT_EQ(double2tf32(-1.0), 0x5fc00, "-1.0");
    ASSERT_EQ(double2tf32(2.0), 0x20000, "2.0");
    ASSERT_EQ(double2tf32(-2025.5), 0x627ea, "-2025.5");
    
    TEST_CASE("Special values");
    ASSERT_EQ(double2tf32(INFINITY), 0x3fc00, "+Inf");
    ASSERT_EQ(double2tf32(-INFINITY), 0x7fc00, "-Inf");
    ASSERT_EQ(double2tf32(NAN), 0x3FE00, "NaN");
    
    TEST_CASE("Very large numbers (overflow)");
    ASSERT_EQ(double2tf32(1e40), 0x3fc00, "1e40 -> +Inf");
    ASSERT_EQ(double2tf32(-1e40), 0x7fc00, "-1e40 -> -Inf");
    
    TEST_CASE("Very small numbers (underflow)");
    ASSERT_EQ(double2tf32(1e-50), 0x00000, "1e-50 -> 0");
    ASSERT_EQ(double2tf32(-1e-50), 0x40000, "-1e-50 -> -0");
}

void test_tf322double() {
    TEST_SECTION("tf322double - TF32 to Double Conversion");
    
    TEST_CASE("Zero");
    printf("  tf322double(0x00000) = %f\n", tf322double(0x00000));
    printf("  tf322double(0x40000) = %f\n", tf322double(0x40000));
    
    TEST_CASE("Normal numbers");
    printf("  tf322double(0x1fc00) = %f (expect 1.0)\n", tf322double(0x1fc00));
    printf("  tf322double(0x20000) = %f (expect 2.0)\n", tf322double(0x20000));
    printf("  tf322double(0x20f29) = %f (expect 28.640625)\n", tf322double(0x20f29));
    
    TEST_CASE("Special values");
    printf("  tf322double(0x3fc00) = %f (expect +inf)\n", tf322double(0x3fc00));
    printf("  tf322double(0x7fc00) = %f (expect -inf)\n", tf322double(0x7fc00));
    printf("  tf322double(0x3FE00) = %f (expect nan)\n", tf322double(0x3FE00));
    
    TEST_CASE("Denormalized numbers");
    printf("  tf322double(0x00001) = %.15e (denorm)\n", tf322double(0x00001));
    printf("  tf322double(0x003ff) = %.15e (max denorm)\n", tf322double(0x003ff));
}

void test_tf32_add() {
    TEST_SECTION("tf32_add - TF32 Addition");
    
    TEST_CASE("Basic addition");
    ASSERT_EQ(tf32_add(0x1fc00, 0x1fc00), 0x20000, "1.0 + 1.0 = 2.0");
    ASSERT_EQ(tf32_add(0x21f45, 0x20ce8), 0x21f93, "465.25 + 19.625");
    
    TEST_CASE("Zero cases");
    ASSERT_EQ(tf32_add(0x00000, 0x1fc00), 0x1fc00, "0 + 1");
    ASSERT_EQ(tf32_add(0x1fc00, 0x00000), 0x1fc00, "1 + 0");
    ASSERT_EQ(tf32_add(0x00000, 0x40000), 0x40000, "+0 + -0");
    
    TEST_CASE("Sign combinations");
    ASSERT_EQ(tf32_add(0x1fc00, 0x5fc00), 0x00000, "1 + (-1) = 0");
    ASSERT_EQ(tf32_add(0x20000, 0x60400), 0x60000, "2 + (-4) = -2");
    
    TEST_CASE("Infinity cases");
    ASSERT_EQ(tf32_add(0x3fc00, 0x1fc00), 0x3fc00, "+Inf + 1 = +Inf");
    ASSERT_EQ(tf32_add(0x3fc00, 0x7fc00), 0x3FE00, "+Inf + (-Inf) = NaN");
    
    TEST_CASE("Denormalized numbers");
    ASSERT_EQ(tf32_add(0x000c0, 0x40080), 0x00440, "denorm + denorm");
}

void test_tf32_mul() {
    TEST_SECTION("tf32_mul - TF32 Multiplication");
    
    TEST_CASE("Basic multiplication");
    ASSERT_EQ(tf32_mul(0x1fc00, 0x1fc00), 0x1fc00, "1.0 * 1.0 = 1.0");
    ASSERT_EQ(tf32_mul(0x20000, 0x20000), 0x20400, "2.0 * 2.0 = 4.0");
    
    TEST_CASE("Zero cases");
    ASSERT_EQ(tf32_mul(0x00000, 0x1fc00), 0x00000, "0 * 1 = 0");
    ASSERT_EQ(tf32_mul(0x1fc00, 0x00000), 0x00000, "1 * 0 = 0");
    
    TEST_CASE("Sign combinations");
    ASSERT_EQ(tf32_mul(0x1fc00, 0x5fc00), 0x5fc00, "1 * (-1) = -1");
    ASSERT_EQ(tf32_mul(0x5fc00, 0x5fc00), 0x1fc00, "(-1) * (-1) = 1");
    
    TEST_CASE("Infinity cases");
    ASSERT_EQ(tf32_mul(0x3fc00, 0x1fc00), 0x3fc00, "+Inf * 1 = +Inf");
    ASSERT_EQ(tf32_mul(0x3fc00, 0x00000), 0x3FE00, "+Inf * 0 = NaN");
    ASSERT_EQ(tf32_mul(0x7fc00, 0x7fc00), 0x3fc00, "-Inf * -Inf = +Inf");
    
    TEST_CASE("Underflow");
    ASSERT_EQ(tf32_mul(0x00040, 0x00040), 0x00000, "denorm * denorm -> 0");
    
    TEST_CASE("Normal multiplication");
    printf("  0x42714 * 0x24f2a = 0x%05x\n", tf32_mul(0x42714, 0x24f2a));
}

void test_tf32_div() {
    TEST_SECTION("tf32_div - TF32 Division (Newton-Raphson)");
    
    TEST_CASE("Basic division");
    ASSERT_EQ(tf32_div(0x1fc00, 0x1fc00), 0x1fc00, "1.0 / 1.0 = 1.0");
    ASSERT_EQ(tf32_div(0x20400, 0x20000), 0x20000, "4.0 / 2.0 = 2.0");
    
    TEST_CASE("Division by zero");
    ASSERT_EQ(tf32_div(0x1fc00, 0x00000), 0x3fc00, "1 / 0 = +Inf");
    ASSERT_EQ(tf32_div(0x5fc00, 0x00000), 0x7fc00, "-1 / 0 = -Inf");
    ASSERT_EQ(tf32_div(0x00000, 0x00000), 0x3FE00, "0 / 0 = NaN");
    
    TEST_CASE("Infinity cases");
    ASSERT_EQ(tf32_div(0x3fc00, 0x3fc00), 0x3FE00, "+Inf / +Inf = NaN");
    ASSERT_EQ(tf32_div(0x1fc00, 0x3fc00), 0x00000, "1 / +Inf = 0");
    ASSERT_EQ(tf32_div(0x3fc00, 0x1fc00), 0x3fc00, "+Inf / 1 = +Inf");
    
    TEST_CASE("Newton-Raphson accuracy");
    printf("  0x210a0 / 0x20248 = 0x%05x (%.6f)\n", 
           tf32_div(0x210a0, 0x20248), 
           tf322double(tf32_div(0x210a0, 0x20248)));
    printf("  0x2261a / 0x60a80 = 0x%05x (%.6f)\n", 
           tf32_div(0x2261a, 0x60a80), 
           tf322double(tf32_div(0x2261a, 0x60a80)));
    
    TEST_CASE("Sign combinations");
    printf("  2 / -2 = 0x%05x (expect negative)\n", tf32_div(0x20000, 0x60000));
    printf("  -4 / -2 = 0x%05x (expect positive)\n", tf32_div(0x60400, 0x60000));
}

void test_tf32_compare() {
    TEST_SECTION("tf32_compare - TF32 Comparison");
    
    TEST_CASE("Equal values");
    ASSERT_INT_EQ(tf32_compare(0x1fc00, 0x1fc00), 0, "1.0 == 1.0");
    ASSERT_INT_EQ(tf32_compare(0x00000, 0x40000), 0, "+0 == -0");
    
    TEST_CASE("Less than");
    ASSERT_INT_EQ(tf32_compare(0x1fc00, 0x20000), -1, "1.0 < 2.0");
    ASSERT_INT_EQ(tf32_compare(0x5fc00, 0x1fc00), -1, "-1.0 < 1.0");
    
    TEST_CASE("Greater than");
    ASSERT_INT_EQ(tf32_compare(0x20000, 0x1fc00), 1, "2.0 > 1.0");
    ASSERT_INT_EQ(tf32_compare(0x1fc00, 0x5fc00), 1, "1.0 > -1.0");
    
    TEST_CASE("NaN comparisons");
    ASSERT_INT_EQ(tf32_compare(0x3FE00, 0x1fc00), -2, "NaN vs 1.0");
    ASSERT_INT_EQ(tf32_compare(0x1fc00, 0x3FE00), -2, "1.0 vs NaN");
    ASSERT_INT_EQ(tf32_compare(0x3FE00, 0x3FE00), -2, "NaN vs NaN");
    
    TEST_CASE("Infinity comparisons");
    ASSERT_INT_EQ(tf32_compare(0x3fc00, 0x1fc00), 1, "+Inf > 1.0");
    ASSERT_INT_EQ(tf32_compare(0x7fc00, 0x1fc00), -1, "-Inf < 1.0");
    ASSERT_INT_EQ(tf32_compare(0x7fc00, 0x3fc00), -1, "-Inf < +Inf");
    
    TEST_CASE("Negative comparisons");
    ASSERT_INT_EQ(tf32_compare(0x60000, 0x60400), 1, "-2.0 > -4.0");
}

void test_edge_cases() {
    TEST_SECTION("Edge Cases & Stress Tests");
    
    TEST_CASE("Round-to-even verification");
    printf("  int2tf32(3001) = 0x%05x (expect 0x229dc)\n", int2tf32(3001));
    
    TEST_CASE("Denormalized number handling");
    tf32 d1 = 0x00001;
    tf32 d2 = 0x00002;
    printf("  denorm(0x00001) + denorm(0x00002) = 0x%05x\n", tf32_add(d1, d2));
    printf("  denorm * denorm = 0x%05x (expect underflow to 0)\n", 
           tf32_mul(0x00100, 0x00100));
    
    TEST_CASE("Overflow scenarios");
    tf32 large = 0x3fbff;  // Close to infinity
    printf("  Large * Large = 0x%05x (expect overflow to Inf)\n", 
           tf32_mul(large, large));
    
    TEST_CASE("Precision limits");
    printf("  1.0 + very_small = 0x%05x (expect 1.0)\n", 
           tf32_add(0x1fc00, 0x00001));
    
    TEST_CASE("Bit pattern validation");
    printf("  put_tf32(0x3FE00): ");
    put_tf32(0x3FE00);
    printf(" (NaN pattern)\n");
    
    printf("  put_tf32(0x3FC00): ");
    put_tf32(0x3FC00);
    printf(" (+Inf pattern)\n");
}

void test_consistency() {
    TEST_SECTION("Consistency & Roundtrip Tests");
    
    TEST_CASE("int -> tf32 -> int roundtrip");
    int test_ints[] = {0, 1, -1, 37, -13, 1024, -1024};
    for (int i = 0; i < 7; i++) {
        int original = test_ints[i];
        tf32 converted = int2tf32(original);
        int roundtrip = tf322int(converted);
        if (original == roundtrip) {
            printf("  ✓ %d -> 0x%05x -> %d\n", original, converted, roundtrip);
        } else {
            printf("  ✗ %d -> 0x%05x -> %d (MISMATCH)\n", 
                   original, converted, roundtrip);
        }
    }
    
    TEST_CASE("Arithmetic identities");
    tf32 x = 0x210a0;  // 37
    printf("  x = 0x%05x\n", x);
    printf("  x + 0 = 0x%05x (expect x)\n", tf32_add(x, 0x00000));
    printf("  x * 1 = 0x%05x (expect x)\n", tf32_mul(x, 0x1fc00));
    printf("  x / 1 = 0x%05x (expect x)\n", tf32_div(x, 0x1fc00));
    printf("  x - x = 0x%05x (expect 0)\n", tf32_add(x, x ^ 0x40000));
}

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║         TF32 Edge Case Verification Test Suite                ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    test_int2tf32();
    test_tf322int();
    test_double2tf32();
    test_tf322double();
    test_tf32_add();
    test_tf32_mul();
    test_tf32_div();
    test_tf32_compare();
    test_edge_cases();
    test_consistency();
    
    printf("\n");
    printf("================================================================\n");
    printf("  Test Suite Complete\n");
    printf("================================================================\n\n");
    
    return 0;
}