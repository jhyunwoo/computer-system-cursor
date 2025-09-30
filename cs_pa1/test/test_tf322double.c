#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include "tf32.h"   // typedef tf32; double tf322double(tf32) 선언

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
#ifndef TF32_EXP_MASK
#define TF32_EXP_MASK   0x3FC00u
#endif
#ifndef TF32_SIGN_MASK
#define TF32_SIGN_MASK  0x40000u
#endif
#ifndef TF32_EXP_BIAS
#define TF32_EXP_BIAS   127
#endif

// PACK: (sign, exp(8), frac(10)) -> 19-bit TF32
#define PACK(s,e,f) ((((s)&1u)<<TF32_SIGN_SHIFT) | (((e)&0xFFu)<<TF32_EXP_SHIFT) | ((f)&TF32_FRAC_MASK))

// 스펙 기반 기대값 계산기 (구현과 독립)
static double tf32_spec_to_double(tf32 x) {
    unsigned s = (x & TF32_SIGN_MASK) != 0u;
    unsigned e = (x & TF32_EXP_MASK)  >> TF32_EXP_SHIFT;
    unsigned f = (x & TF32_FRAC_MASK);

    double sign = s ? -1.0 : +1.0;

    if (e == 0xFFu) {                       // Inf or NaN
        if (f != 0u) return NAN;            // NaN
        return s ? -INFINITY : +INFINITY;   // ±Inf
    }
    if (e == 0u) {                          // Zero or Subnormal
        if (f == 0u) return copysign(0.0, s ? -1.0 : +1.0); // ±0
        // Subnormal: value = sign * (frac / 2^10) * 2^(1 - 127)
        return sign * ( (double)f / 1024.0 ) * ldexp(1.0, 1 - TF32_EXP_BIAS);
    }
    // Normal: value = sign * (1 + frac/2^10) * 2^(e - 127)
    return sign * (1.0 + ((double)f / 1024.0)) * ldexp(1.0, (int)e - TF32_EXP_BIAS);
}

static bool same_sign_zero(double a, double b) {
    // +0.0 / -0.0 구분
    return (signbit(a) == signbit(b)) && (a == 0.0) && (b == 0.0);
}

static bool doubles_close(double got, double expect) {
    if (isnan(expect))   return isnan(got);
    if (isinf(expect))   return isinf(got) && (signbit(got) == signbit(expect));
    if (got == 0.0 && expect == 0.0) return same_sign_zero(got, expect);
    // 상대 오차 기준(엄격): 1e-12
    double diff = fabs(got - expect);
    double scale = fmax(fabs(expect), 1.0);
    return diff <= 1e-12 * scale;
}

typedef struct { tf32 in; const char* why; } Case;

int main(void){
    Case cases[] = {
        /* ===== Zeros ===== */
        { PACK(0,0,0),                  "+0.0" },
        { PACK(1,0,0),                  "-0.0" },

        /* ===== Subnormals =====
           값 = sign * (frac / 2^10) * 2^(1-127) = sign * frac * 2^(-136) */
        { PACK(0,0,1),                  "smallest +subnormal (≈ 2^-136)" },
        { PACK(1,0,1),                  "smallest -subnormal (≈ -2^-136)" },
        { PACK(0,0,0x3FF),              "largest +subnormal (1023 * 2^-136)" },
        { PACK(1,0,0x3FF),              "largest -subnormal (-1023 * 2^-136)" },

        /* ===== Normals (exact / easy) ===== */
        { PACK(0,127,0),                "+1.0" },
        { PACK(1,127,0),                "-1.0" },
        { PACK(0,127,512),              "+1.5" },
        { PACK(0,128,0),                "+2.0" },
        { PACK(0,128,512),              "+3.0" },
        { PACK(1,128,512),              "-3.0" },
        { PACK(0,142,0),                "+32768 (=2^15)" },

        /* ===== Big normals (still finite in double) ===== */
        { PACK(0,254,1023),             "max TF32 normal: (1+1023/1024)*2^(127)" },
        { PACK(1,254,1023),             "min TF32 normal (most negative finite)" },

        /* ===== Infinities / NaN ===== */
        { PACK(0,255,0),                "+INF" },
        { PACK(1,255,0),                "-INF" },
        { PACK(0,255,1),                "NaN (quiet)" },
        { PACK(1,255,512),              "NaN (sign=-, payload)" },
    };

    int n = (int)(sizeof(cases)/sizeof(cases[0]));
    int pass = 0;

    for (int i=0;i<n;i++){
        double expect = tf32_spec_to_double(cases[i].in);
        double got    = tf322double(cases[i].in);

        int ok = doubles_close(got, expect);
        printf("TF32=0x%05X  -> got=% .17e  expect=% .17e  [%s]%s\n",
               cases[i].in, got, expect, cases[i].why, ok ? "  ✓" : "  ✗");
        if (ok) pass++;
    }
    printf("\nPassed %d / %d\n", pass, n);
    return 0;
}
