#include <stdio.h>
#include "tf32.h"

/*
    Do NOT include any C libraries or header files
    except those above

    use the defines in tf32.h
    if neccessary, you can add some macros below
*/

// TF32 형식: 19비트 (1 부호 + 8 지수 + 10 가수)
#define TF32_SIGN_MASK    0x40000u
#define TF32_EXP_MASK     0x3FC00u
#define TF32_FRAC_MASK    0x003FFu
#define TF32_EXP_BIAS     127
#define TF32_EXP_SHIFT    10
#define TF32_FRAC_BITS    10

tf32 int2tf32(int in) {
    if (in == 0) {
        return 0;
    }
    
    unsigned int sign = 0;
    unsigned int abs_val;
    
    // 부호 처리
    if (in < 0) {
        sign = 1;
        if (in == 0x80000000) {  // INT_MIN
            abs_val = 0x80000000u;
        } else {
            abs_val = (unsigned int)(-in);
        }
    } else {
        abs_val = (unsigned int)in;
    }
    
    // 최상위 비트 찾기
    int leading_bit = 31;
    while (leading_bit >= 0 && !((abs_val >> leading_bit) & 1)) {
        leading_bit--;
    }
    
    if (leading_bit < 0) {
        return 0;
    }
    
    // 지수 계산
    int exp = leading_bit + TF32_EXP_BIAS;
    
    // 가수 추출
    unsigned int frac;
    if (leading_bit >= TF32_FRAC_BITS) {
        int shift = leading_bit - TF32_FRAC_BITS;
        frac = (abs_val >> shift) & TF32_FRAC_MASK;
        
        // Round to even
        if (shift > 0) {
            unsigned int remainder = abs_val & ((1u << shift) - 1);
            unsigned int half = 1u << (shift - 1);
            if (remainder > half || (remainder == half && (frac & 1))) {
                frac++;
                if (frac > TF32_FRAC_MASK) {
                    frac = 0;
                    exp++;
                }
            }
        }
    } else {
        frac = (abs_val << (TF32_FRAC_BITS - leading_bit)) & TF32_FRAC_MASK;
    }
    
    return (sign << 18) | (exp << TF32_EXP_SHIFT) | frac;
}

int tf322int(tf32 in) {
    unsigned int sign = (in >> 18) & 1;
    unsigned int exp = (in >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac = in & TF32_FRAC_MASK;
    
    // 특수값
    if (exp == 0xFF) {
        return (frac == 0) ? (sign ? 0x80000000 : 0x7FFFFFFF) : 0x80000000;
    }
    
    if (exp == 0) {
        return 0;  // ±0 또는 비정규화 수 (모두 0으로 변환)
    }
    
    // 정규화된 수
    int true_exp = (int)exp - TF32_EXP_BIAS;
    
    if (true_exp < 0) {
        return 0;
    }
    
    unsigned int mantissa = (1u << TF32_FRAC_BITS) | frac;
    unsigned int result_uint;
    
    if (true_exp >= TF32_FRAC_BITS) {
        if (true_exp - TF32_FRAC_BITS > 21) {
            return sign ? 0x80000000 : 0x7FFFFFFF;
        }
        result_uint = mantissa << (true_exp - TF32_FRAC_BITS);
    } else {
        int shift = TF32_FRAC_BITS - true_exp;
        unsigned int remainder = mantissa & ((1u << shift) - 1);
        result_uint = mantissa >> shift;
        
        unsigned int half = 1u << (shift - 1);
        if (remainder > half || (remainder == half && (result_uint & 1))) {
            result_uint++;
        }
    }
    
    // 부호 적용 및 범위 체크
    if (sign == 0) {
        // 양수: INT_MAX 초과 체크
        if (result_uint > 0x7FFFFFFFu) {
            return 0x7FFFFFFF;  // TMax
        }
        return (int)result_uint;
    } else {
        // 음수: INT_MIN 체크
        if (result_uint > 0x80000000u) {
            return 0x80000000;  // TMin
        }
        if (result_uint == 0x80000000u) {
            return (int)0x80000000;  // INT_MIN
        }
        return -(int)result_uint;
    }
}

tf32 double2tf32(double in) {
    union { double f; unsigned long long u; } x;
    x.f = in;
    
    unsigned long long bits = x.u;
    unsigned long long sign = (bits >> 63) & 1;
    unsigned long long exp = (bits >> 52) & 0x7FF;
    unsigned long long frac = bits & 0xFFFFFFFFFFFFFull;
    
    // 특수값
    if (exp == 0x7FF) {
        if (frac == 0) {
            return (sign << 18) | 0x3FC00;  // ±Inf
        } else {
            return 0x3FE00;  // NaN (quiet NaN: exp=0xFF, frac=0x200)
        }
    }
    
    if (exp == 0 && frac == 0) {
        return sign << 18;  // ±0
    }
    
    // 정규화
    int double_exp;
    if (exp == 0) {
        // 비정규화된 수
        double_exp = 1 - 1023;
        while ((frac & 0x10000000000000ull) == 0) {
            frac <<= 1;
            double_exp--;
        }
        frac &= 0xFFFFFFFFFFFFFull;
    } else {
        double_exp = (int)exp - 1023;
    }
    
    // TF32 지수로 변환
    int tf32_exp = double_exp + TF32_EXP_BIAS;
    
    // 오버플로우
    if (tf32_exp >= 0xFF) {
        return (sign << 18) | 0x3FC00;
    }
    
    // 언더플로우 - 비정규화
    if (tf32_exp <= 0) {
        if (tf32_exp < -TF32_FRAC_BITS) {
            return sign << 18;  // 0으로 언더플로우
        }
        
        // 비정규화된 수 생성
        unsigned long long tf32_frac = frac >> (52 - TF32_FRAC_BITS);
        tf32_frac |= 0x400;  // 암시적 1 추가
        
        int shift_amount = 1 - tf32_exp;
        tf32_frac >>= shift_amount;
        
        // Round to even
        int total_shift = shift_amount + (52 - TF32_FRAC_BITS);
        if (total_shift < 52) {
            unsigned long long remainder = frac & ((1ull << total_shift) - 1);
            unsigned long long half = 1ull << (total_shift - 1);
            if (remainder > half || (remainder == half && (tf32_frac & 1))) {
                tf32_frac++;
            }
        }
        
        return (sign << 18) | (tf32_frac & TF32_FRAC_MASK);
    }
    
    // 정규화된 수
    int shift = 52 - TF32_FRAC_BITS;
    unsigned long long tf32_frac = frac >> shift;
    unsigned long long remainder = frac & ((1ull << shift) - 1);
    
    // Round to even
    unsigned long long half = 1ull << (shift - 1);
    if (remainder > half || (remainder == half && (tf32_frac & 1))) {
        tf32_frac++;
        if (tf32_frac > TF32_FRAC_MASK) {
            tf32_frac = 0;
            tf32_exp++;
            if (tf32_exp >= 0xFF) {
                return (sign << 18) | 0x3FC00;
            }
        }
    }
    
    return (sign << 18) | (tf32_exp << TF32_EXP_SHIFT) | tf32_frac;
}

// 내부 계산용: 정확한 비정규화 수 변환
static double tf322double_accurate(tf32 in) {
    unsigned int sign = (in >> 18) & 1;
    unsigned int exp = (in >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac = in & TF32_FRAC_MASK;
    
    union { double f; unsigned long long u; } result;
    
    if (exp == 0xFF) {
        if (frac == 0) {
            result.u = ((unsigned long long)sign << 63) | 0x7FF0000000000000ull;
        } else {
            result.u = 0x7FF8000000000000ull;
        }
        return result.f;
    }
    
    if (exp == 0) {
        if (frac == 0) {
            result.u = (unsigned long long)sign << 63;
            return result.f;
        }
        
        // 비정규화 수 - 정확한 변환
        int shift_count = 0;
        unsigned int temp = frac;
        while ((temp & 0x400) == 0) {
            temp <<= 1;
            shift_count++;
        }
        
        int double_exp = (-126 - shift_count) + 1023;
        unsigned long long double_frac = ((unsigned long long)(temp & 0x3FF)) << (52 - 10);
        
        result.u = ((unsigned long long)sign << 63) | 
                  ((unsigned long long)double_exp << 52) | 
                  double_frac;
        return result.f;
    }
    
    int true_exp = (int)exp - TF32_EXP_BIAS;
    int double_exp = true_exp + 1023;
    
    unsigned long long double_frac = (unsigned long long)frac << (52 - TF32_FRAC_BITS);
    
    result.u = ((unsigned long long)sign << 63) | 
              ((unsigned long long)double_exp << 52) | 
              double_frac;
    
    return result.f;
}

// 외부용: 과제 스펙에 맞는 변환
double tf322double(tf32 in) {
    unsigned int sign = (in >> 18) & 1;
    unsigned int exp = (in >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac = in & TF32_FRAC_MASK;
    
    union { double f; unsigned long long u; } result;
    
    if (exp == 0xFF) {
        if (frac == 0) {
            result.u = ((unsigned long long)sign << 63) | 0x7FF0000000000000ull;
        } else {
            result.u = 0x7FF8000000000000ull;
        }
        return result.f;
    }
    
    if (exp == 0) {
        if (frac == 0) {
            result.u = (unsigned long long)sign << 63;
            return result.f;
        }
        
        // 비정규화 수 - 과제 스펙: 2^-126으로 매핑
        result.u = ((unsigned long long)sign << 63) | (897ull << 52);
        return result.f;
    }
    
    int true_exp = (int)exp - TF32_EXP_BIAS;
    int double_exp = true_exp + 1023;
    
    unsigned long long double_frac = (unsigned long long)frac << (52 - TF32_FRAC_BITS);
    
    result.u = ((unsigned long long)sign << 63) | 
              ((unsigned long long)double_exp << 52) | 
              double_frac;
    
    return result.f;
}

tf32 tf32_add(tf32 a, tf32 b) {
    unsigned int sign_a = (a >> 18) & 1;
    unsigned int exp_a = (a >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_a = a & TF32_FRAC_MASK;
    
    unsigned int sign_b = (b >> 18) & 1;
    unsigned int exp_b = (b >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_b = b & TF32_FRAC_MASK;
    
    // 특수값 처리
    if (exp_a == 0xFF) {
        if (frac_a != 0) return 0x3FE00;  // NaN
        if (exp_b == 0xFF && frac_b == 0 && sign_a != sign_b) return 0x3FE00;  // Inf - Inf
        return a;  // Inf + x = Inf
    }
    
    if (exp_b == 0xFF) {
        if (frac_b != 0) return 0x3FE00;
        return b;
    }
    
    // 0 처리
    if (exp_a == 0 && frac_a == 0) return b;
    if (exp_b == 0 && frac_b == 0) return a;
    
    // a와 b를 정렬 (exp_a >= exp_b)
    if (exp_b > exp_a || (exp_b == exp_a && frac_b > frac_a)) {
        unsigned int tmp_s = sign_a; sign_a = sign_b; sign_b = tmp_s;
        unsigned int tmp_e = exp_a; exp_a = exp_b; exp_b = tmp_e;
        unsigned int tmp_f = frac_a; frac_a = frac_b; frac_b = tmp_f;
    }
    
    // 가수 정규화 (암시적 1 추가, 단 비정규화 수는 제외)
    unsigned int mant_a = (exp_a == 0) ? frac_a : ((1u << TF32_FRAC_BITS) | frac_a);
    unsigned int mant_b = (exp_b == 0) ? frac_b : ((1u << TF32_FRAC_BITS) | frac_b);
    
    // 비정규화 수 처리
    int true_exp_a = (exp_a == 0) ? 1 : exp_a;
    int true_exp_b = (exp_b == 0) ? 1 : exp_b;
    
    // 지수 차이만큼 b를 시프트
    int exp_diff = true_exp_a - true_exp_b;
    if (exp_diff > 15) {
        return a;  // b가 너무 작아서 무시
    }
    
    unsigned int shifted_b = mant_b >> exp_diff;
    unsigned int result_mant;
    unsigned int result_sign;
    int result_exp = true_exp_a;
    
    // 부호에 따라 덧셈 또는 뺄셈
    if (sign_a == sign_b) {
        result_sign = sign_a;
        result_mant = mant_a;
        result_mant += shifted_b;
        
        // 오버플로우 처리
        if (result_mant & (1u << (TF32_FRAC_BITS + 1))) {
            result_mant >>= 1;
            result_exp++;
        }
    } else {
        if (mant_a >= shifted_b) {
            result_sign = sign_a;
            result_mant = mant_a;
            result_mant -= shifted_b;
        } else {
            result_sign = sign_b;
            result_mant = shifted_b;
            result_mant -= mant_a;
        }
        
        // 정규화
        if (result_mant == 0) return 0;
        
        while (result_mant < (1u << TF32_FRAC_BITS) && result_exp > 1) {
            result_mant <<= 1;
            result_exp--;
        }
    }
    
    // 오버플로우 체크
    if (result_exp >= 0xFF) {
        return (result_sign << 18) | 0x3FC00;  // Inf
    }
    
    // 결과 구성
    unsigned int result_frac = result_mant & TF32_FRAC_MASK;
    return (result_sign << 18) | (result_exp << TF32_EXP_SHIFT) | result_frac;
}

tf32 tf32_mul(tf32 a, tf32 b) {
    unsigned int sign_a = (a >> 18) & 1;
    unsigned int exp_a = (a >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_a = a & TF32_FRAC_MASK;
    
    unsigned int sign_b = (b >> 18) & 1;
    unsigned int exp_b = (b >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_b = b & TF32_FRAC_MASK;
    
    unsigned int result_sign = sign_a ^ sign_b;
    
    // 특수값
    if (exp_a == 0xFF) {
        if (frac_a != 0) return 0x3FE00;  // NaN
        if (exp_b == 0 && frac_b == 0) return 0x3FE00;  // Inf * 0
        return (result_sign << 18) | 0x3FC00;  // Inf * x = Inf
    }
    
    if (exp_b == 0xFF) {
        if (frac_b != 0) return 0x3FE00;
        if (exp_a == 0 && frac_a == 0) return 0x3FE00;
        return (result_sign << 18) | 0x3FC00;
    }
    
    // 0 처리
    if ((exp_a == 0 && frac_a == 0) || (exp_b == 0 && frac_b == 0)) {
        return result_sign << 18;
    }
    
    // 가수 준비 (암시적 1 추가, 비정규화 수 제외)
    unsigned int mant_a = (exp_a == 0) ? frac_a : ((1u << TF32_FRAC_BITS) | frac_a);
    unsigned int mant_b = (exp_b == 0) ? frac_b : ((1u << TF32_FRAC_BITS) | frac_b);
    
    // 비정규화 수의 지수 처리
    int true_exp_a = (exp_a == 0) ? 1 : exp_a;
    int true_exp_b = (exp_b == 0) ? 1 : exp_b;
    
    // 가수 곱셈 (11비트 * 11비트 = 최대 22비트)
    unsigned int product = mant_a * mant_b;
    
    // 지수 계산
    int result_exp = (true_exp_a - TF32_EXP_BIAS) + (true_exp_b - TF32_EXP_BIAS) + TF32_EXP_BIAS;
    
    // 정규화
    int shift = 0;
    if (product & (1u << (TF32_FRAC_BITS * 2 + 1))) {
        shift = TF32_FRAC_BITS + 1;
        result_exp++;
    } else {
        shift = TF32_FRAC_BITS;
    }
    
    unsigned int result_frac = (product >> shift) & TF32_FRAC_MASK;
    
    // Round to even
    if (shift > 0) {
        unsigned int remainder = product & ((1u << shift) - 1);
        unsigned int half = 1u << (shift - 1);
        if (remainder > half || (remainder == half && (result_frac & 1))) {
            result_frac++;
            if (result_frac > TF32_FRAC_MASK) {
                result_frac = 0;
                result_exp++;
            }
        }
    }
    
    // 오버플로우 체크
    if (result_exp >= 0xFF) {
        return (result_sign << 18) | 0x3FC00;  // Inf
    }
    
    // 언더플로우 체크
    if (result_exp <= 0) {
        return result_sign << 18;  // 0
    }
    
    return (result_sign << 18) | (result_exp << TF32_EXP_SHIFT) | result_frac;
}

tf32 tf32_div(tf32 a, tf32 b) {
    unsigned int sign_a = (a >> 18) & 1;
    unsigned int exp_a = (a >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_a = a & TF32_FRAC_MASK;
    
    unsigned int sign_b = (b >> 18) & 1;
    unsigned int exp_b = (b >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_b = b & TF32_FRAC_MASK;
    
    unsigned int result_sign = sign_a ^ sign_b;
    
    // 특수값
    if (exp_a == 0xFF) {
        if (frac_a != 0) return 0x3FE00;  // NaN
        if (exp_b == 0xFF) return 0x3FE00;  // Inf / Inf
        return (result_sign << 18) | 0x3FC00;  // Inf / x = Inf
    }
    
    if (exp_b == 0xFF) {
        if (frac_b != 0) return 0x3FE00;
        return result_sign << 18;  // x / Inf = 0
    }
    
    // 0으로 나누기
    if (exp_b == 0 && frac_b == 0) {
        if (exp_a == 0 && frac_a == 0) return 0x3FE00;  // 0 / 0 = NaN
        return (result_sign << 18) | 0x3FC00;  // x / 0 = Inf
    }
    
    // a가 0
    if (exp_a == 0 && frac_a == 0) {
        return result_sign << 18;
    }
    
    // Newton-Raphson으로 1/|b| 계산 (부호 무시)
    int iter = 5;
    
    // b의 절대값 (부호 비트 제거)
    tf32 b_abs = b & 0x3FFFF;
    
    // b의 실제 지수
    int exp_val;
    if (exp_b == 0) {
        // 비정규화된 수
        unsigned int temp = frac_b;
        exp_val = -126;
        while ((temp & 0x200) == 0) {
            temp <<= 1;
            exp_val--;
        }
    } else {
        exp_val = (int)exp_b - TF32_EXP_BIAS;
    }
    
    int neg_exp = -exp_val;
    
    // 초기값: y = 2^(-exp_val)
    tf32 y;
    int init_exp = neg_exp + TF32_EXP_BIAS;
    if (init_exp <= 0) {
        y = 0;
    } else if (init_exp >= 0xFF) {
        y = 0x3FC00;
    } else {
        y = init_exp << TF32_EXP_SHIFT;
    }
    
    tf32 two = (TF32_EXP_BIAS + 1) << TF32_EXP_SHIFT;  // 2.0
    
    // y = y * (2 - |b| * y), Newton-Raphson 반복
    for (int i = 0; i < iter; i++) {
        tf32 b_times_y = tf32_mul(b_abs, y);
        tf32 two_minus_by = tf32_add(two, b_times_y ^ 0x40000);  // 부호 반전
        y = tf32_mul(y, two_minus_by);
    }
    
    // a * (1/|b|), 그 다음 부호 적용
    tf32 result = tf32_mul(a, y);
    
    // 최종 부호 적용
    if (result_sign) {
        result |= 0x40000;  // 부호 비트 설정
    } else {
        result &= 0x3FFFF;  // 부호 비트 제거
    }
    
    return result;
}

int tf32_compare(tf32 a, tf32 b) {
    unsigned int sign_a = (a >> 18) & 1;
    unsigned int exp_a = (a >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_a = a & TF32_FRAC_MASK;
    
    unsigned int sign_b = (b >> 18) & 1;
    unsigned int exp_b = (b >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_b = b & TF32_FRAC_MASK;
    
    // NaN 처리
    if ((exp_a == 0xFF && frac_a != 0) || (exp_b == 0xFF && frac_b != 0)) {
        return -2;
    }
    
    // ±0 비교
    if ((exp_a == 0 && frac_a == 0) && (exp_b == 0 && frac_b == 0)) {
        return 0;
    }
    
    // 부호가 다른 경우
    if (sign_a != sign_b) {
        if (sign_a) return -1;  // a가 음수
        else return 1;  // a가 양수
    }
    
    // 같은 부호
    int comparison;
    
    if (a == b) {
        comparison = 0;
    } else if (exp_a != exp_b) {
        comparison = (exp_a > exp_b) ? 1 : -1;
    } else {
        comparison = (frac_a > frac_b) ? 1 : -1;
    }
    
    // 음수면 비교 결과 반전
    if (sign_a) {
        comparison = -comparison;
    }
    
    return comparison;
}