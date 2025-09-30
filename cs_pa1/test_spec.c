#include <stdio.h>
#include "tf32.h"

// Helper macros
#define PASS "\033[32m✓\033[0m"
#define FAIL "\033[31m✗\033[0m"
#define INFO "\033[36mℹ\033[0m"

// TF32 special values
#define POS_INF   0x3fc00
#define NEG_INF   0x7fc00
#define POS_ZERO  0x00000
#define NEG_ZERO  0x40000
#define POS_NAN   0x3FE00
#define NEG_NAN   0x7FE00
#define POS_NORM  0x1fc00  // 1.0
#define NEG_NORM  0x5fc00  // -1.0

int test_count = 0;
int pass_count = 0;
int fail_count = 0;

void test_result(int condition, const char* desc, const char* expected, const char* got) {
    test_count++;
    if (condition) {
        printf("  %s %s\n", PASS, desc);
        pass_count++;
    } else {
        printf("  %s %s\n", FAIL, desc);
        printf("     Expected: %s, Got: %s\n", expected, got);
        fail_count++;
    }
}

void print_section(const char* title) {
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("═══════════════════════════════════════════════════════════════\n");
}

void test_tf322int_spec() {
    print_section("tf322int - Specification Verification");
    
    printf("\n[1] Positive Infinity → TMax\n");
    int r1 = tf322int(POS_INF);
    char buf1[50], exp1[50];
    sprintf(buf1, "%d (0x%x)", r1, r1);
    sprintf(exp1, "%d (0x%x)", 0x7FFFFFFF, 0x7FFFFFFF);
    test_result(r1 == 0x7FFFFFFF, "tf322int(+Inf) = TMax", exp1, buf1);
    
    printf("\n[2] Negative Infinity → TMin\n");
    int r2 = tf322int(NEG_INF);
    char buf2[50], exp2[50];
    sprintf(buf2, "%d (0x%x)", r2, (unsigned int)r2);
    sprintf(exp2, "%d (0x%x)", (int)0x80000000, 0x80000000);
    test_result(r2 == (int)0x80000000, "tf322int(-Inf) = TMin", exp2, buf2);
    
    printf("\n[3] >TMax → TMax\n");
    tf32 large = 0x27800;  // 2^31
    int r3 = tf322int(large);
    char buf3[50], exp3[50];
    sprintf(buf3, "%d", r3);
    sprintf(exp3, "%d (TMax)", 0x7FFFFFFF);
    test_result(r3 == 0x7FFFFFFF, "tf322int(2^31) = TMax", exp3, buf3);
    
    printf("\n[4] <TMin → TMin\n");
    tf32 small = 0x67900;  // Very negative
    int r4 = tf322int(small);
    char buf4[50], exp4[50];
    sprintf(buf4, "%d", r4);
    sprintf(exp4, "%d (TMin)", (int)0x80000000);
    test_result(r4 == (int)0x80000000, "tf322int(very negative) = TMin", exp4, buf4);
    
    printf("\n[5] ±NaN → TMin\n");
    int r5a = tf322int(POS_NAN);
    int r5b = tf322int(NEG_NAN);
    char buf5[50], exp5[50];
    sprintf(buf5, "%d, %d", r5a, r5b);
    sprintf(exp5, "%d", (int)0x80000000);
    test_result(r5a == (int)0x80000000 && r5b == (int)0x80000000, 
                "tf322int(±NaN) = TMin", exp5, buf5);
}

void test_tf32_add_spec() {
    print_section("tf32_add - Specification Verification");
    
    printf("\n[1] +Inf + +Inf = +Inf\n");
    tf32 r1 = tf32_add(POS_INF, POS_INF);
    test_result(r1 == POS_INF, "tf32_add(+Inf, +Inf)", "0x3fc00", "");
    
    printf("\n[2] +Inf + -Inf = NaN\n");
    tf32 r2 = tf32_add(POS_INF, NEG_INF);
    unsigned int exp = (r2 >> 10) & 0xFF;
    unsigned int frac = r2 & 0x3FF;
    test_result(exp == 0xFF && frac != 0, "tf32_add(+Inf, -Inf)", "NaN", "");
    
    printf("\n[3] +Inf + Normal Value = +Inf\n");
    tf32 r3 = tf32_add(POS_INF, POS_NORM);
    test_result(r3 == POS_INF, "tf32_add(+Inf, 1.0)", "0x3fc00", "");
    
    printf("\n[4] -Inf + -Inf = -Inf\n");
    tf32 r4 = tf32_add(NEG_INF, NEG_INF);
    test_result(r4 == NEG_INF, "tf32_add(-Inf, -Inf)", "0x7fc00", "");
    
    printf("\n[5] -Inf + Normal Value = -Inf\n");
    tf32 r5 = tf32_add(NEG_INF, POS_NORM);
    test_result(r5 == NEG_INF, "tf32_add(-Inf, 1.0)", "0x7fc00", "");
    
    printf("\n[6] NaN + NaN = NaN\n");
    tf32 r6 = tf32_add(POS_NAN, POS_NAN);
    exp = (r6 >> 10) & 0xFF;
    frac = r6 & 0x3FF;
    test_result(exp == 0xFF && frac != 0, "tf32_add(NaN, NaN)", "NaN", "");
}

void test_tf32_mul_spec() {
    print_section("tf32_mul - Specification Verification");
    
    printf("\n[1] +Inf * +Inf = +Inf\n");
    tf32 r1 = tf32_mul(POS_INF, POS_INF);
    test_result(r1 == POS_INF, "tf32_mul(+Inf, +Inf)", "0x3fc00", "");
    
    printf("\n[2] +Inf * -Inf = -Inf\n");
    tf32 r2 = tf32_mul(POS_INF, NEG_INF);
    test_result(r2 == NEG_INF, "tf32_mul(+Inf, -Inf)", "0x7fc00", "");
    
    printf("\n[3] +Inf * Positive Normal Value = +Inf\n");
    tf32 r3 = tf32_mul(POS_INF, POS_NORM);
    test_result(r3 == POS_INF, "tf32_mul(+Inf, 1.0)", "0x3fc00", "");
    
    printf("\n[4] +Inf * Negative Normal Value = -Inf\n");
    tf32 r4 = tf32_mul(POS_INF, NEG_NORM);
    test_result(r4 == NEG_INF, "tf32_mul(+Inf, -1.0)", "0x7fc00", "");
    
    printf("\n[5] -Inf * -Inf = +Inf\n");
    tf32 r5 = tf32_mul(NEG_INF, NEG_INF);
    test_result(r5 == POS_INF, "tf32_mul(-Inf, -Inf)", "0x3fc00", "");
    
    printf("\n[6] -Inf * Positive Normal Value = -Inf\n");
    tf32 r6 = tf32_mul(NEG_INF, POS_NORM);
    test_result(r6 == NEG_INF, "tf32_mul(-Inf, 1.0)", "0x7fc00", "");
    
    printf("\n[7] -Inf * Negative Normal Value = +Inf\n");
    tf32 r7 = tf32_mul(NEG_INF, NEG_NORM);
    test_result(r7 == POS_INF, "tf32_mul(-Inf, -1.0)", "0x3fc00", "");
    
    printf("\n[8] +Inf * 0 = NaN (sign may vary)\n");
    tf32 r8 = tf32_mul(POS_INF, POS_ZERO);
    unsigned int exp8 = (r8 >> 10) & 0xFF;
    unsigned int frac8 = r8 & 0x3FF;
    test_result(exp8 == 0xFF && frac8 != 0, "tf32_mul(+Inf, 0)", "NaN", "");
    
    printf("\n[9] -Inf * 0 = NaN (sign may vary)\n");
    tf32 r9 = tf32_mul(NEG_INF, POS_ZERO);
    unsigned int exp9 = (r9 >> 10) & 0xFF;
    unsigned int frac9 = r9 & 0x3FF;
    test_result(exp9 == 0xFF && frac9 != 0, "tf32_mul(-Inf, 0)", "NaN", "");
    
    printf("\n[10] NaN * NaN = NaN\n");
    tf32 r10 = tf32_mul(POS_NAN, POS_NAN);
    unsigned int exp10 = (r10 >> 10) & 0xFF;
    unsigned int frac10 = r10 & 0x3FF;
    test_result(exp10 == 0xFF && frac10 != 0, "tf32_mul(NaN, NaN)", "NaN", "");
}

void test_tf32_div_spec() {
    print_section("tf32_div - Specification Verification");
    
    printf("\n[1] +Inf / +Inf = NaN\n");
    tf32 r1 = tf32_div(POS_INF, POS_INF);
    unsigned int exp1 = (r1 >> 10) & 0xFF;
    unsigned int frac1 = r1 & 0x3FF;
    test_result(exp1 == 0xFF && frac1 != 0, "tf32_div(+Inf, +Inf)", "NaN", "");
    
    printf("\n[2] +Inf / -Inf = NaN\n");
    tf32 r2 = tf32_div(POS_INF, NEG_INF);
    unsigned int exp2 = (r2 >> 10) & 0xFF;
    unsigned int frac2 = r2 & 0x3FF;
    test_result(exp2 == 0xFF && frac2 != 0, "tf32_div(+Inf, -Inf)", "NaN", "");
    
    printf("\n[3] +Inf / 0 = +Inf\n");
    tf32 r3 = tf32_div(POS_INF, POS_ZERO);
    test_result(r3 == POS_INF, "tf32_div(+Inf, 0)", "0x3fc00", "");
    
    printf("\n[4] +Inf / Positive Normal = +Inf\n");
    tf32 r4 = tf32_div(POS_INF, POS_NORM);
    test_result(r4 == POS_INF, "tf32_div(+Inf, 1.0)", "0x3fc00", "");
    
    printf("\n[5] +Inf / Negative Normal = -Inf\n");
    tf32 r5 = tf32_div(POS_INF, NEG_NORM);
    test_result(r5 == NEG_INF, "tf32_div(+Inf, -1.0)", "0x7fc00", "");
    
    printf("\n[6] -Inf / +Inf = NaN\n");
    tf32 r6 = tf32_div(NEG_INF, POS_INF);
    unsigned int exp6 = (r6 >> 10) & 0xFF;
    unsigned int frac6 = r6 & 0x3FF;
    test_result(exp6 == 0xFF && frac6 != 0, "tf32_div(-Inf, +Inf)", "NaN", "");
    
    printf("\n[7] -Inf / -Inf = NaN\n");
    tf32 r7 = tf32_div(NEG_INF, NEG_INF);
    unsigned int exp7 = (r7 >> 10) & 0xFF;
    unsigned int frac7 = r7 & 0x3FF;
    test_result(exp7 == 0xFF && frac7 != 0, "tf32_div(-Inf, -Inf)", "NaN", "");
    
    printf("\n[8] -Inf / 0 = -Inf\n");
    tf32 r8 = tf32_div(NEG_INF, POS_ZERO);
    test_result(r8 == NEG_INF, "tf32_div(-Inf, 0)", "0x7fc00", "");
    
    printf("\n[9] -Inf / Positive Normal = -Inf\n");
    tf32 r9 = tf32_div(NEG_INF, POS_NORM);
    test_result(r9 == NEG_INF, "tf32_div(-Inf, 1.0)", "0x7fc00", "");
    
    printf("\n[10] -Inf / Negative Normal = +Inf\n");
    tf32 r10 = tf32_div(NEG_INF, NEG_NORM);
    test_result(r10 == POS_INF, "tf32_div(-Inf, -1.0)", "0x3fc00", "");
    
    printf("\n[11] 0 / 0 = NaN\n");
    tf32 r11 = tf32_div(POS_ZERO, POS_ZERO);
    unsigned int exp11 = (r11 >> 10) & 0xFF;
    unsigned int frac11 = r11 & 0x3FF;
    test_result(exp11 == 0xFF && frac11 != 0, "tf32_div(0, 0)", "NaN", "");
    
    printf("\n[12] 0 / Positive Normal = +0\n");
    tf32 r12 = tf32_div(POS_ZERO, POS_NORM);
    test_result(r12 == POS_ZERO, "tf32_div(0, 1.0)", "0x00000", "");
    
    printf("\n[13] 0 / Negative Normal = -0\n");
    tf32 r13 = tf32_div(POS_ZERO, NEG_NORM);
    test_result(r13 == NEG_ZERO, "tf32_div(0, -1.0)", "0x40000", "");
    
    printf("\n[14] Positive Normal / 0 = +Inf\n");
    tf32 r14 = tf32_div(POS_NORM, POS_ZERO);
    test_result(r14 == POS_INF, "tf32_div(1.0, 0)", "0x3fc00", "");
    
    printf("\n[15] Positive Normal / ±Inf = ±0\n");
    tf32 r15a = tf32_div(POS_NORM, POS_INF);
    tf32 r15b = tf32_div(POS_NORM, NEG_INF);
    test_result(r15a == POS_ZERO && r15b == NEG_ZERO, 
                "tf32_div(1.0, ±Inf)", "±0", "");
    
    printf("\n[16] Negative Normal / 0 = -Inf\n");
    tf32 r16 = tf32_div(NEG_NORM, POS_ZERO);
    test_result(r16 == NEG_INF, "tf32_div(-1.0, 0)", "0x7fc00", "");
    
    printf("\n[17] Negative Normal / ±Inf = ∓0\n");
    tf32 r17a = tf32_div(NEG_NORM, POS_INF);
    tf32 r17b = tf32_div(NEG_NORM, NEG_INF);
    test_result(r17a == NEG_ZERO && r17b == POS_ZERO, 
                "tf32_div(-1.0, ±Inf)", "∓0", "");
    
    printf("\n[18] NaN / NaN = NaN\n");
    tf32 r18 = tf32_div(POS_NAN, POS_NAN);
    unsigned int exp18 = (r18 >> 10) & 0xFF;
    unsigned int frac18 = r18 & 0x3FF;
    test_result(exp18 == 0xFF && frac18 != 0, "tf32_div(NaN, NaN)", "NaN", "");
}

void test_tf32_compare_spec() {
    print_section("tf32_compare - Specification Verification");
    
    printf("\n[1] +Inf == +Inf → 0\n");
    int r1 = tf32_compare(POS_INF, POS_INF);
    test_result(r1 == 0, "tf32_compare(+Inf, +Inf)", "0", "");
    
    printf("\n[2] +Inf > -Inf → 1\n");
    int r2 = tf32_compare(POS_INF, NEG_INF);
    test_result(r2 == 1, "tf32_compare(+Inf, -Inf)", "1", "");
    
    printf("\n[3] +Inf > Normal Value → 1\n");
    int r3 = tf32_compare(POS_INF, POS_NORM);
    test_result(r3 == 1, "tf32_compare(+Inf, 1.0)", "1", "");
    
    printf("\n[4] -Inf == -Inf → 0\n");
    int r4 = tf32_compare(NEG_INF, NEG_INF);
    test_result(r4 == 0, "tf32_compare(-Inf, -Inf)", "0", "");
    
    printf("\n[5] -Inf < Normal Value → -1\n");
    int r5 = tf32_compare(NEG_INF, POS_NORM);
    test_result(r5 == -1, "tf32_compare(-Inf, 1.0)", "-1", "");
    
    printf("\n[6] +0 == -0 → 0\n");
    int r6 = tf32_compare(POS_ZERO, NEG_ZERO);
    test_result(r6 == 0, "tf32_compare(+0, -0)", "0", "");
    
    printf("\n[7] NaN vs NaN → -2\n");
    int r7 = tf32_compare(POS_NAN, POS_NAN);
    test_result(r7 == -2, "tf32_compare(NaN, NaN)", "-2", "");
}

void print_summary() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                    Test Summary                               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  Total Tests:  %d\n", test_count);
    printf("  Passed:       %s %d\n", PASS, pass_count);
    printf("  Failed:       %s %d\n", fail_count > 0 ? FAIL : PASS, fail_count);
    printf("\n");
    
    if (fail_count == 0) {
        printf("  🎉 All specification requirements verified!\n");
    } else {
        printf("  ⚠️  Some tests failed. Please review the implementation.\n");
    }
    printf("\n");
}

int main() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║        TF32 Specification Compliance Verification            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    test_tf322int_spec();
    test_tf32_add_spec();
    test_tf32_mul_spec();
    test_tf32_div_spec();
    test_tf32_compare_spec();
    
    print_summary();
    
    return fail_count > 0 ? 1 : 0;
}
