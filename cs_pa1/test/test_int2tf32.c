#include <stdio.h>
#include <limits.h>
#include "tf32.h"   // tf32 typedef, int2tf32 선언 포함

// TF32 포맷에서 유용한 매크로 (필요시 tf32.h에 옮겨도 됨)
#ifndef TF32_SIGN_SHIFT
#define TF32_SIGN_SHIFT 18
#endif
#ifndef TF32_EXP_SHIFT
#define TF32_EXP_SHIFT  10
#endif
#ifndef TF32_FRAC_MASK
#define TF32_FRAC_MASK  0x3FFu
#endif

typedef struct { int in; unsigned int expect; const char* why; } Case;

int main(void) {
    Case cases[] = {
        /* ===== Zero & Small integers ===== */
        { 0,    0x00000, "0 → 0x00000" },
        { 1,    0x1FC00, "1 → 0x1FC00" },
        { 2,    0x20000, "2 → 0x20000" },
        { 3,    0x20200, "3 → 0x20200" },
        { 4,    0x20400, "4 → 0x20400" },
        { 5,    0x20500, "5 → 0x20500" },
        { 6,    0x20600, "6 → 0x20600" },
        { 7,    0x20700, "7 → 0x20700" },
        { 8,    0x20800, "8 → 0x20800" },
        { 9,    0x20880, "9 → 0x20880" },
        { 10,   0x20900, "10 → 0x20900" },

        /* ===== Around 2^10 (shift==0) ===== */
        { 1023, 0x223FE, "1023 → 0x223FE" },
        { 1024, 0x22400, "1024 → 0x22400" },
        { 1025, 0x22401, "1025 → 0x22401" },

        /* ===== Tie-to-even around 2^11 ===== */
        { 2047, 0x227FF, "2047 → 0x227FF" },
        { 2048, 0x22800, "2048 → 0x22800" },
        { 2049, 0x22800, "2049 tie-even → 0x22800" },
        { 2051, 0x22802, "2051 tie-odd  → 0x22802" },

        /* ===== Tie-to-even around 2^12 ===== */
        { 4095, 0x22C00, "4095 → round up → 0x22C00" },
        { 4096, 0x22C00, "4096 → 0x22C00" },
        { 4097, 0x22C00, "4097 tie-even → 0x22C00" },
        { 4099, 0x22C01, "4099 tie-odd  → 0x22C01" },

        /* ===== Larger magnitudes ===== */
        { 1048576, 0x24C00, "2^20 → 0x24C00" },
        { 1048577, 0x24C00, "2^20+1 → same as 2^20" },

        /* ===== Random positive/negative ===== */
        { 123456789, 0x2675C, "123456789 → 0x2675C" },
        { -123456789,0x6675C, "-123456789 → 0x6675C" },
        { -1,   0x5FC00, "-1 → 0x5FC00" },
        { -2,   0x60000, "-2 → 0x60000" },
        { -3,   0x60200, "-3 → 0x60200" },
        { -1024,0x62400, "-1024 → 0x62400" },
        { -2048,0x62800, "-2048 → 0x62800" },

        /* ===== Extremes ===== */
        { INT_MAX, 0x27800, "INT_MAX(2147483647) → 0x27800" },
        { INT_MIN, 0x67800, "INT_MIN(-2147483648) → 0x67800" },
    };

    int n = sizeof(cases)/sizeof(cases[0]);
    int ok = 0;
    for (int i=0;i<n;i++) {
        unsigned int got = int2tf32(cases[i].in);
        printf("in=%12d -> got=0x%05X  expect=0x%05X  : %s\n",
               cases[i].in, got, cases[i].expect, cases[i].why);
        if (got == cases[i].expect) ok++;
    }
    printf("\nPassed %d / %d\n", ok, n);
    return 0;
}
