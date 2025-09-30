#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include "tf32.h"   // typedef tf32; tf32_div 선언

// ===== 보조 매크로 (필요 시 tf32.h에 이미 있다면 #ifndef 가드로 충돌 방지) =====
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

// ===== 스펙 기반 변환기: TF32 → double (Oracle) =====
static double tf32_spec_to_double(tf32 x) {
    unsigned s = (x & TF32_SIGN_MASK) != 0u;
    unsigned e = (x & TF32_EXP_MASK)  >> TF32_EXP_SHIFT;
    unsigned f = (x & TF32_FRAC_MASK);

    double sign = s ? -1.0 : +1.0;
    if (e == 0xFFu) { if (f != 0u) return NAN; return s ? -INFINITY : +INFINITY; }
    if (e == 0u) { if (f == 0u) return copysign(0.0, s ? -1.0 : +1.0);
                   return sign * ((double)f / 1024.0) * ldexp(1.0, 1 - TF32_EXP_BIAS); }
    return sign * (1.0 + ((double)f / 1024.0)) * ldexp(1.0, (int)e - TF32_EXP_BIAS);
}

// ===== 스펙 기반 변환기: double → TF32 (Oracle, RTE + subnormal) =====
static tf32 double_to_tf32_ref(double x) {
    if (isnan(x)) return PACK(0,255,1);
    if (isinf(x)) return signbit(x) ? PACK(1,255,0) : PACK(0,255,0);
    if (x == 0.0) return PACK(0,0,0); // +0로 통일

    int sign = signbit(x) ? 1 : 0;
    double ax = fabs(x);

    int e2;               // ax = m * 2^e2, 0.5 ≤ m < 1
    double m = frexp(ax, &e2);

    // TF32는 1.xxx 기반 → mant = m*2 ∈ [1,2), exp_unbiased = e2-1
    double mant = m * 2.0;
    int exp_unbiased = e2 - 1;
    long long exp_tf32 = (long long)exp_unbiased + TF32_EXP_BIAS;

    if (exp_tf32 >= 0xFF) return PACK(sign,255,0); // overflow → INF

    // 11비트(1+10) mantissa 생성 + RTE
    double mant11 = mant * 1024.0;                 // [1024, 2048)
    unsigned long long keep = (unsigned long long)floor(mant11);
    double fracp = mant11 - (double)keep;
    if (fracp > 0.5) keep++;
    else if (fracp == 0.5 && (keep & 1ull)) keep++;

    // 자리올림
    if (keep >= 2048ull) {
        keep >>= 1;
        exp_tf32++;
        if (exp_tf32 >= 0xFF) return PACK(sign,255,0);
    }

    // 언더플로 → subnormal로 하강 + RTE
    if (exp_tf32 <= 0) {
        int sh = 1 - (int)exp_tf32; // exp 1→0 만들기
        if (sh >= 32) return PACK(sign,0,0);
        unsigned mant11u = (unsigned)keep;
        unsigned lost = (sh ? (mant11u & ((1u << sh) - 1u)) : 0u);
        unsigned kept = mant11u >> sh;
        unsigned halfway = (sh ? (1u << (sh - 1)) : 0u);
        if (sh && (lost > halfway || (lost == halfway && (kept & 1u)))) kept++;
        unsigned frac = kept & TF32_FRAC_MASK;     // subnormal: implicit 1 없음
        if (frac == 0) return PACK(sign,0,0);
        return PACK(sign,0,frac);
    }

    unsigned frac10 = (unsigned)(keep & TF32_FRAC_MASK);
    return PACK(sign,(unsigned)exp_tf32,frac10);
}

// ===== 기대값(Oracle): 나눗셈 =====
static tf32 oracle_div(tf32 a, tf32 b) {
    double da = tf32_spec_to_double(a);
    double db = tf32_spec_to_double(b);

    // NaN 우선
    if (isnan(da) || isnan(db)) return PACK(0,255,1);
    // 0/0 = NaN
    if (da == 0.0 && db == 0.0) return PACK(0,255,1);
    // ∞/∞ = NaN
    if (isinf(da) && isinf(db)) return PACK(0,255,1);
    // ∞/finite = ±∞
    if (isinf(da) && !isinf(db)) return double_to_tf32_ref(da / db);
    // finite/∞ = 0 (부호 포함)
    if (!isinf(da) && isinf(db)) return double_to_tf32_ref(da / db);
    // finite/0 = ±∞
    if (db == 0.0) return PACK(signbit(da) ^ signbit(db), 255, 0);

    double q = da / db;
    return double_to_tf32_ref(q);
}

// ===== 테스트 케이스 =====
typedef struct { tf32 a, b; const char* why; } Case;

int main(void){
    Case cases[] = {
        // Zeros
        { PACK(0,0,0), PACK(0,127,0),        "0 / 1 = +0" },
        { PACK(1,0,0), PACK(0,127,0),        "-0 / 1 = -0 (비트상)" },

        // 0으로 나누기 (finite/0 → ±INF)
        { PACK(0,127,0), PACK(0,0,0),        "1 / 0 = +INF" },
        { PACK(1,127,0), PACK(0,0,0),        "-1 / 0 = -INF" },
        { PACK(0,0,0),   PACK(0,0,0),        "0 / 0 = NaN" },

        // 간단한 정규 케이스
        { PACK(0,128,0), PACK(0,127,0),      "2.0 / 1.0 = 2.0" },
        { PACK(0,127,512), PACK(0,127,0),    "1.5 / 1.0 = 1.5" },
        { PACK(0,127,0), PACK(0,127,512),    "1.0 / 1.5 ≈ 0.666..." },

        // 부호 규칙
        { PACK(1,127,0), PACK(0,127,0),      "-1.0 / 1.0 = -1.0" },
        { PACK(1,127,512), PACK(1,127,512),  "-1.5 / -1.5 = +1.0" },

        // 지수 차이 큰 경우 (정렬 및 라운딩 관찰)
        { PACK(0,142,0), PACK(0,127,0),      "2^15 / 1 = 2^15" },
        { PACK(0,142,0), PACK(0,128,0),      "2^15 / 2 = 2^14" },
        { PACK(0,142,0), PACK(0,142,0),      "2^15 / 2^15 = 1.0" },

        // 서브노말과의 조합
        { PACK(0,0,1),   PACK(0,127,0),      "min subnormal / 1 = min subnormal" },
        { PACK(0,127,0), PACK(0,0,1),        "1 / tiny → very large → +INF or 큰 정규" },

        // 언더플로 (아주 작은 결과)
        { PACK(0,127,0), PACK(0,142,0),      "1 / 2^15 → 매우 작음 (subnormal/0)" },

        // 오버플로 (아주 큰 결과)
        { PACK(0,254,1023), PACK(0,127,1),   "max / (1 + 2^-10) ≈ max (경계)" },
        { PACK(0,254,1023), PACK(0,0,1),     "max / tiny → +INF" },

        // 특수값: INF / FINITE, FINITE / INF, INF / 0, 등
        { PACK(0,255,0), PACK(0,127,0),      "+INF / 1.0 = +INF" },
        { PACK(1,255,0), PACK(0,127,0),      "-INF / 1.0 = -INF" },
        { PACK(0,127,0), PACK(0,255,0),      "1.0 / +INF = +0" },
        { PACK(0,255,0), PACK(0,255,0),      "+INF / +INF = NaN" },
        { PACK(0,255,1), PACK(0,127,0),      "NaN / 1.0 = NaN" },
        { PACK(0,127,0), PACK(0,255,1),      "1.0 / NaN = NaN" },
    };

    int n = (int)(sizeof(cases)/sizeof(cases[0]));
    int pass = 0;

    for (int i=0;i<n;i++){
        tf32 got = tf32_div(cases[i].a, cases[i].b);
        tf32 exp = oracle_div(cases[i].a, cases[i].b);

        // NaN 동치 허용 (payload 다를 수 있음)
        int ok = (got == exp) ||
                 (isnan(tf32_spec_to_double(got)) && isnan(tf32_spec_to_double(exp)));

        printf("A=0x%05X  B=0x%05X  -> got=0x%05X  expect=0x%05X  [%s]%s\n",
               cases[i].a, cases[i].b, got, exp, cases[i].why, ok ? "  ✓" : "  ✗");
        if (ok) pass++;
    }

    printf("\nPassed %d / %d\n", pass, n);
    return 0;
}
