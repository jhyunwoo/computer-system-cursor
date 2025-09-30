#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "tf32.h"   // typedef tf32; int tf32_compare(tf32 a, tf32 b);

// ===== 테스트 파일에서만 최소 매크로 보강 (헤더에 있으면 #ifndef로 충돌 방지) =====
#ifndef TF32_SIGN_SHIFT
#define TF32_SIGN_SHIFT 18
#endif
#ifndef TF32_EXP_SHIFT
#define TF32_EXP_SHIFT  10
#endif
#ifndef TF32_SIGN_MASK
#define TF32_SIGN_MASK  0x40000u
#endif
#ifndef TF32_EXP_MASK
#define TF32_EXP_MASK   0x3FC00u
#endif
#ifndef TF32_FRAC_MASK
#define TF32_FRAC_MASK  0x3FFu
#endif
#ifndef TF32_EXP_BIAS
#define TF32_EXP_BIAS   127
#endif
#ifndef TF32_IMPLICIT_BIT
#define TF32_IMPLICIT_BIT (1u<<10)
#endif

// 19-bit TF32 패커
#define PACK(s,e,f) ((((s)&1u)<<TF32_SIGN_SHIFT) | (((e)&0xFFu)<<TF32_EXP_SHIFT) | ((f)&TF32_FRAC_MASK))

// ===== NaN 비교 정책: 둘 중 하나라도 NaN이면 무엇을 반환할지 설정 =====
#ifndef CMP_NAN_RET
#define CMP_NAN_RET 0   // 기본: NaN 비교는 0(비교불가=동등 취급). 필요시 -1 또는 +1로 변경 가능.
#endif

// ===== 스펙 기반 TF32 → double (비교용) =====
static double tf32_to_double_spec(tf32 x) {
    unsigned s = (x & TF32_SIGN_MASK) != 0u;
    unsigned e = (x & TF32_EXP_MASK)  >> TF32_EXP_SHIFT;
    unsigned f = (x & TF32_FRAC_MASK);

    double sign = s ? -1.0 : +1.0;

    if (e == 0xFFu) {                  // Inf / NaN
        if (f != 0u) return NAN;
        return s ? -INFINITY : +INFINITY;
    }
    if (e == 0u) {                     // Zero / Subnormal
        if (f == 0u) return copysign(0.0, s ? -1.0 : +1.0);
        // subnormal: value = sign * (f/2^10) * 2^(1-127)
        return sign * ((double)f / 1024.0) * ldexp(1.0, 1 - TF32_EXP_BIAS);
    }
    // normal
    return sign * (1.0 + ((double)f / 1024.0)) * ldexp(1.0, (int)e - TF32_EXP_BIAS);
}

// ===== Oracle 비교 함수 =====
// 반환: -1 (a<b), 0 (a==b), 1 (a>b)
// 정책:
//   * NaN 포함 시 CMP_NAN_RET
//   * +0 == -0 (동등)
//   * 그 외는 double 비교 결과를 따름
static int oracle_compare(tf32 a, tf32 b) {
    double da = tf32_to_double_spec(a);
    double db = tf32_to_double_spec(b);

    if (isnan(da) || isnan(db)) {
        return CMP_NAN_RET;
    }

    // +0, -0 동등 처리
    if (da == 0.0 && db == 0.0) {
        return 0;
    }

    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

// ===== 테스트 케이스 =====
typedef struct { tf32 a, b; int expect; const char* why; } Case;

int main(void){
    Case cases[] = {
        // Zeros (동등)
        { PACK(0,0,0),        PACK(0,0,0),        0,  "+0 vs +0 → equal" },
        { PACK(0,0,0),        PACK(1,0,0),        0,  "+0 vs -0 → equal" },

        // Subnormal vs zero
        { PACK(0,0,1),        PACK(0,0,0),        1,  "tiny+ vs +0 → a>b" },
        { PACK(1,0,1),        PACK(0,0,0),       -1,  "tiny- vs +0 → a<b" },

        // Simple normals
        { PACK(0,127,0),      PACK(0,128,0),     -1,  "1.0  vs 2.0 → a<b" },
        { PACK(0,128,0),      PACK(0,127,0),      1,  "2.0  vs 1.0 → a>b" },
        { PACK(0,127,512),    PACK(0,127,512),    0,  "1.5  vs 1.5 → equal" },
        { PACK(1,127,0),      PACK(1,127,512),    1,  "-1.0 vs -1.5 → a>b (덜 음수)" },
        { PACK(1,127,512),    PACK(1,127,0),     -1,  "-1.5 vs -1.0 → a<b" },

        // Close numbers (가수만 1 ULP 차이)
        { PACK(0,127,100),    PACK(0,127,101),   -1,  "1.097... vs 1.098... → a<b" },
        { PACK(0,127,101),    PACK(0,127,100),    1,  "1.098... vs 1.097... → a>b" },

        // Power-of-two boundaries
        { PACK(0,142,0),      PACK(0,141,1023),   1,  "2^15 vs (2^15 - ulp) → a>b" },
        { PACK(1,142,0),      PACK(1,141,1023),  -1,  "-2^15 vs -(2^15 - ulp) → a<b" },

        // Infinities
        { PACK(0,255,0),      PACK(0,254,1023),   1,  "+INF vs max finite → a>b" },
        { PACK(1,255,0),      PACK(0,254,1023),  -1,  "-INF vs max finite → a<b" },
        { PACK(0,255,0),      PACK(1,255,0),      1,  "+INF vs -INF → a>b" },

        // NaN policy (CMP_NAN_RET 사용)
        { PACK(0,255,1),      PACK(0,127,0),   CMP_NAN_RET, "NaN vs 1.0 → policy" },
        { PACK(0,127,0),      PACK(1,255,512), CMP_NAN_RET, "1.0 vs NaN → policy" },
        { PACK(0,255,1),      PACK(1,255,512), CMP_NAN_RET, "NaN vs NaN → policy" },
    };

    // 기대값 보정: NaN 정책을 Oracle에도 반영
    int n = (int)(sizeof(cases)/sizeof(cases[0]));
    int pass = 0;

    for (int i=0;i<n;i++){
        int got = tf32_compare(cases[i].a, cases[i].b);
        int exp = cases[i].expect;

        // Oracle로 재계산해서 교차 검증 (정수 기대가 아닌 정책 케이스 제외)
        // (주석 해제시 참고) int exp_oracle = oracle_compare(cases[i].a, cases[i].b);

        int ok = (got == exp);

        printf("A=0x%05X  B=0x%05X  -> got=%2d  expect=%2d  [%s]%s\n",
               cases[i].a, cases[i].b, got, exp, cases[i].why, ok ? "  ✓" : "  ✗");

        if (ok) pass++;
    }

    printf("\nPassed %d / %d  (NaN policy: CMP_NAN_RET=%d)\n", pass, n, (int)CMP_NAN_RET);
    return 0;
}
