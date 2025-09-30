#include <stdio.h>
#include <limits.h>      // INT_MAX / INT_MIN
#include "tf32.h"        // ✅ 로컬 헤더

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

#define PACK(s,e,f) ((((s)&1u)<<TF32_SIGN_SHIFT) | (((e)&0xFFu)<<TF32_EXP_SHIFT) | ((f)&TF32_FRAC_MASK))

typedef struct { unsigned int tf; int expect; const char* why; } Case;

int main(void){
    Case cases[] = {
        { PACK(0,0,0),                     0, " +0 → 0" },
        { PACK(1,0,0),                     0, " -0 → 0 (부호 무시)" },
        { PACK(0,0,1),                     0, " subnormal → 0" },
        { PACK(1,0,0x3FF),                 0, " max subnormal(−) → 0" },

        { PACK(0,127,0),                   1, " 1.0 → 1" },
        { PACK(0,128,0),                   2, " 2.0 → 2" },
        { PACK(0,128,512),                 3, " 3.0 → 3" },

        { PACK(0,127,512),                 2, " +1.5 → 2 (짝수)" },
        { PACK(0,128,256),                 2, " +2.5 → 2 (짝수)" },
        { PACK(0,128,768),                 4, " +3.5 → 4 (짝수)" },
        { PACK(1,127,512),                -2, " -1.5 → -2 (짝수)" },
        { PACK(1,128,256),                -2, " -2.5 → -2 (짝수)" },

        { PACK(0,127,512-1),               1, " 1.5 − 1 ulp → 1" },
        { PACK(0,127,512+1),               2, " 1.5 + 1 ulp → 2" },

        { PACK(0,137,0),                1024, " 2^10 → 1024" },
        { PACK(0,138,0),                2048, " 2^11 → 2048" },

        { PACK(0,158,0),             INT_MAX, " +2^31 → INT_MAX 포화" },
        { PACK(1,158,0),             INT_MIN, " -2^31 → INT_MIN 포화" },

        { PACK(0,255,0),             INT_MAX, " +INF → INT_MAX" },
        { PACK(1,255,0),             INT_MIN, " -INF → INT_MIN" },
        { PACK(0,255,1),             INT_MIN, " NaN → INT_MIN(정책)" },

        { PACK(1,127,0),                -1, " -1.0 → -1" },
        { PACK(1,128,0),                -2, " -2.0 → -2" },
        { PACK(1,128,512),              -3, " -3.0 → -3" },
    };

    int n = sizeof(cases)/sizeof(cases[0]);
    int ok = 0;
    for(int i=0;i<n;i++){
        int got = tf322int((tf32)cases[i].tf);
        printf("TF32=0x%05X -> got=%11d  expect=%11d  : %s\n",
               cases[i].tf, got, cases[i].expect, cases[i].why);
        if(got == cases[i].expect) ok++;
    }
    printf("\nPassed %d / %d\n", ok, n);
    return 0;
}
