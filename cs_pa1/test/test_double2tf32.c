#include <stdio.h>
#include <math.h>
#include "tf32.h"   // typedef tf32, double2tf32 선언

// (필요시) 테스트 파일에서만 최소 매크로 보강
#ifndef TF32_SIGN_SHIFT
#define TF32_SIGN_SHIFT 18
#endif
#ifndef TF32_EXP_SHIFT
#define TF32_EXP_SHIFT  10
#endif
#ifndef TF32_FRAC_MASK
#define TF32_FRAC_MASK  0x3FFu
#endif
#ifndef TF32_SIGN_MASK
#define TF32_SIGN_MASK  0x40000u
#endif

// PACK: (sign, exp(8), frac(10)) -> 19-bit TF32
#define PACK(s,e,f) ((((s)&1u)<<TF32_SIGN_SHIFT) | (((e)&0xFFu)<<TF32_EXP_SHIFT) | ((f)&TF32_FRAC_MASK))

typedef struct { double in; unsigned tf_expect; const char* why; } Case;

int main(void){
    // NOTE: 기대값은 TF32 포맷(부호1/지수8(bias127)/가수10) 기준의 19-bit 정수(hex)
    Case cases[] = {
        /* ===== Zeros ===== */
        { +0.0,                         PACK(0,0,0),        "+0.0 → +0" },
        { -0.0,                         PACK(1,0,0),        "-0.0 → -0 (비트만 -0)" },

        /* ===== Exact integers (정규) ===== */
        { 1.0,                          0x1FC00,            "1.0  → exp=127, frac=0" },
        { 2.0,                          0x20000,            "2.0  → exp=128, frac=0" },
        { 3.0,                          0x20200,            "3.0  → 1.5×2^1 → frac=512" },
        { 4.0,                          0x20400,            "4.0  → exp=129, frac=0" },

        /* ===== Half/tie-to-even (정확히 이진 0.5) ===== */
        { 1.5,                          0x1FE00,            "1.5  → 1 + 0.5 → frac=512" },
        { 2.5,                          0x20100,            "2.5  → 1.25×2^1 → frac=256" },
        { 3.5,                          0x20300,            "3.5  → 1.75×2^1 → frac=768" },
        { -1.5,                         0x5FE00,            "-1.5 → sign=1, exp=127, frac=512" },

        /* ===== 몇 가지 양/음수 정규값 ===== */
        { 9.0,                          0x20880,            "9.0  → (1.125)×2^3 → frac=0.125*1024=128" },
        { 10.0,                         0x20900,            "10.0 → (1.25 )×2^3 → frac=256" },
        { -3.0,                         0x60200,            "-3.0 → sign=1, exp=128, frac=512" },

        /* ===== 큰 값 (정규) ===== */
        { 1024.0,                       0x22400,            "2^10 → exp=137, frac=0" },
        { 2048.0,                       0x22800,            "2^11 → exp=138, frac=0" },

        /* ===== 오버플로 → ±INF =====
           TF32에서 exp=255는 INF/NaN. 2^128의 TF32 지수는 128+127=255 → +INF / -INF  */
        { +pow(2.0, 128.0),             0x3FC00,            "+2^128 → +INF" },
        { -pow(2.0, 128.0),             0x7FC00,            "-2^128 → -INF" },

        /* ===== 너무 작은 값 (더블 서브노말/아주 작은 수) → 0으로 수렴 ===== */
        { 1e-300,                       PACK(0,0,0),        "아주 작은 양수 → TF32에서 0" },
        { -1e-300,                      PACK(1,0,0),        "아주 작은 음수 → TF32에서 -0" },

        /* ===== NaN / INF from <math.h> ===== */
        { +INFINITY,                    0x3FC00,            "+INF → 0x3FC00" },
        { -INFINITY,                    0x7FC00,            "-INF → 0x7FC00" },
        { +NAN,                         0x3FC01,            "NaN  → 0x3FC01 (exp=255, frac≠0)" },
    };

    int n = (int)(sizeof(cases)/sizeof(cases[0]));
    int ok = 0;
    for(int i=0;i<n;i++){
        tf32 got = double2tf32(cases[i].in);
        printf("in=%12.6e -> got=0x%05X  expect=0x%05X  : %s\n",
               cases[i].in, got, cases[i].tf_expect, cases[i].why);
        if (got == cases[i].tf_expect) ok++;
    }
    printf("\nPassed %d / %d\n", ok, n);
    return 0;
}
