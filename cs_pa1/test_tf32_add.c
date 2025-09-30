#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include "tf32.h"   // typedef tf32; tf32_add 선언

// ====== 보조 매크로 (필요 시 tf32.h에 이미 있다면 중복 제거) ======
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

// ====== 스펙 기반 변환기: TF32 → double (Oracle) ======
static double tf32_spec_to_double(tf32 x) {
    unsigned s = (x & TF32_SIGN_MASK) != 0u;
    unsigned e = (x & TF32_EXP_MASK)  >> TF32_EXP_SHIFT;
    unsigned f = (x & TF32_FRAC_MASK);

    double sign = s ? -1.0 : +1.0;

    if (e == 0xFFu) {                 // Inf / NaN
        if (f != 0u) return NAN;
        return s ? -INFINITY : +INFINITY;
    }
    if (e == 0u) {                    // Zero / Subnormal
        if (f == 0u) return copysign(0.0, s ? -1.0 : +1.0);
        // subnormal: value = sign * (f / 2^10) * 2^(1-127)
        return sign * ((double)f / 1024.0) * ldexp(1.0, 1 - TF32_EXP_BIAS);
    }
    // normal: value = sign * (1 + f/2^10) * 2^(e-127)
    return sign * (1.0 + ((double)f / 1024.0)) * ldexp(1.0, (int)e - TF32_EXP_BIAS);
}

// ====== 스펙 기반 변환기: double → TF32 (Oracle, round-to-even, subnormal 처리) ======
static tf32 double_to_tf32_ref(double x) {
    // NaN / Inf
    if (isnan(x))      return PACK(0,255,1); // quiet NaN
    if (isinf(x))      return signbit(x) ? PACK(1,255,0) : PACK(0,255,0);

    // ±0
    if (x == 0.0) {
        // +0로 통일(네 tf32_add가 0 리턴 시 +0로 패킹하는 정책에 맞춤)
        return PACK(0,0,0);
    }

    int sign = signbit(x) ? 1 : 0;
    double ax = fabs(x);

    // frexp: ax = m * 2^e2, 0.5 ≤ m < 1
    int e2;
    double m = frexp(ax, &e2);  // m in [0.5,1), ax = m * 2^e2

    // TF32는 1.xxx 형태가 기준 → mantissa = m*2 in [1,2), exponent = e2-1
    double mant = m * 2.0;
    int exp_unbiased = e2 - 1;

    long long exp_tf32 = (long long)exp_unbiased + TF32_EXP_BIAS;

    // 너무 크면 ±INF
    if (exp_tf32 >= 0xFF) return PACK(sign, 255, 0);

    // 11비트(1+10) mantissa 생성 + RTE
    // mant ∈ [1,2) → mant11_raw = mant * 2^10 ∈ [1024, 2048)
    double mant11_d = mant * 1024.0;
    unsigned long long keep = (unsigned long long)floor(mant11_d);
    double frac_part = mant11_d - (double)keep;

    // tie-to-even: frac_part == 0.5 ?
    if (frac_part > 0.5) {
        keep += 1ull;
    } else if (frac_part == 0.5) {
        if (keep & 1ull) keep += 1ull;
    }
    // 자리올림으로 2048이 되면 재정규화
    if (keep >= 2048ull) {
        keep >>= 1;
        exp_tf32 += 1;
        if (exp_tf32 >= 0xFF) return PACK(sign, 255, 0);
    }

    // 서브노말 처리 (exp_tf32 <= 0 → exp=0로 떨어뜨리기 + RTE)
    if (exp_tf32 <= 0) {
        // 목표: exp=0이 되게 mant11(=keep) 을 더 오른쪽으로 이동
        int sh = 1 - (int)exp_tf32; // 최소 1
        if (sh >= 32) return PACK(sign,0,0); // 너무 작음 → 0
        unsigned mant11 = (unsigned)keep;
        unsigned lost = (sh ? (mant11 & ((1u << sh) - 1u)) : 0u);
        unsigned kept = mant11 >> sh;
        unsigned halfway = (sh ? (1u << (sh - 1)) : 0u);

        if (sh && (lost > halfway || (lost == halfway && (kept & 1u)))) kept += 1u;

        unsigned frac = kept & TF32_FRAC_MASK; // implicit 1 없음
        if (frac == 0) return PACK(sign,0,0);
        return PACK(sign,0,frac);
    }

    // 정상 패킹: implicit 1 제거 후 frac10 저장
    unsigned frac10 = (unsigned)(keep & TF32_FRAC_MASK);
    return PACK(sign,(unsigned)exp_tf32,frac10);
}

// ====== 기대값(Oracle): (a,b) → TF32 ======
static tf32 oracle_add(tf32 a, tf32 b) {
    double da = tf32_spec_to_double(a);
    double db = tf32_spec_to_double(b);

    // 특수값 우선: NaN 규칙
    if (isnan(da) || isnan(db)) return PACK(0,255,1);
    if (isinf(da) && isinf(db)) {
        // +inf + -inf = NaN
        if ((da > 0 && db < 0) || (da < 0 && db > 0)) return PACK(0,255,1);
    }

    double sum = da + db;
    return double_to_tf32_ref(sum);
}

// ====== 테스트 케이스 ======
typedef struct { tf32 a, b; const char* why; } Case;

int main(void){
    Case cases[] = {
        // Zeros
        { PACK(0,0,0), PACK(0,0,0), "0 + 0 = +0" },
        { PACK(0,0,0), PACK(1,0,0), "+0 + -0 → +0 정책" },

        // Simple normals
        { PACK(0,127,0), PACK(0,127,0), "1.0 + 1.0 = 2.0" },
        { PACK(0,127,512), PACK(0,127,512), "1.5 + 1.5 = 3.0" },
        { PACK(0,128,0), PACK(1,127,0), "2.0 + (-1.0) = 1.0" },

        // Opposite signs, equal magnitude → cancellation to +0
        { PACK(0,138,0), PACK(1,138,0), "+2048 + (-2048) → +0" },

        // Alignment with big exponent gap (sticky propagation check)
        { PACK(0,254,1023), PACK(0,127,0), "max TF32 + 1.0 ≈ max TF32 (작은 항은 sticky로만 반영)" },

        // Tie-to-even: craft small addends to test rounding
        { PACK(0,142,0), PACK(0,127,0), "32768 + 1 → 32769 근처 (RTE)" },
        { PACK(0,128,256), PACK(0,128,256), "2.5 + 2.5 = 5.0 (RTE 정상)" },

        // Underflow to subnormal (1.0 + (-0.9990234375) ≈ ~1/1024)
        { PACK(0,127,0), PACK(1,127,1023), "1.0 + (- (1 - 1/1024)) → subnormal or 0" },

        // Overflow to infinity
        { PACK(0,254,1023), PACK(0,254,1023), "max + max → +INF" },

        // Infinities & NaNs
        { PACK(0,255,0), PACK(0,127,0), "+INF + 1.0 = +INF" },
        { PACK(1,255,0), PACK(0,127,0), "-INF + 1.0 = -INF" },
        { PACK(0,255,0), PACK(1,255,0), "+INF + -INF = NaN" },
        { PACK(0,255,1), PACK(0,127,0), "NaN + 1.0 = NaN" },
    };

    int n = (int)(sizeof(cases)/sizeof(cases[0]));
    int pass = 0;

    for (int i=0;i<n;i++){
        tf32 got = tf32_add(cases[i].a, cases[i].b);
        tf32 exp = oracle_add(cases[i].a, cases[i].b);

        int ok = (got == exp) ||
                 (isnan(tf32_spec_to_double(got)) && isnan(tf32_spec_to_double(exp))); // NaN 동등성 처리

        printf("A=0x%05X  B=0x%05X  -> got=0x%05X  expect=0x%05X  [%s]%s\n",
               cases[i].a, cases[i].b, got, exp, cases[i].why, ok ? "  ✓" : "  ✗");

        if (ok) pass++;
    }

    printf("\nPassed %d / %d\n", pass, n);
    return 0;
}
