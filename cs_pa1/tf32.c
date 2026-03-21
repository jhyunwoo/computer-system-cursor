#include <stdio.h>
#include "tf32.h"

/*
    Do NOT include any C libraries or header files
    except those above

    use the defines in tf32.h
    if neccessary, you can add some macros below
*/

// TF32 형식은 19비트로 구성됨: 부호(1) + 지수(8) + 가수(10)
// 아래는 TF32 형식의 각 부분을 추출하기 위한 마스크와 상수 정의
#define TF32_SIGN_MASK    0x40000u // 부호 비트 마스크 (19번째 비트)
#define TF32_EXP_MASK     0x3FC00u // 지수 비트 마스크 (11-18번째 비트)
#define TF32_FRAC_MASK    0x003FFu // 가수 비트 마스크 (1-10번째 비트)
#define TF32_EXP_BIAS     127      // 지수의 바이어스 값 (IEEE 754 single-precision과 동일)
#define TF32_EXP_SHIFT    10       // 지수 비트를 제자리로 옮기기 위한 시프트 값
#define TF32_FRAC_BITS    10       // 가수의 비트 수

// 32비트 정수를 tf32 형식으로 변환하는 함수
// 정규, 비정규, 0, 특수 값을 처리
tf32 int2tf32(int in) {
    // 입력 값이 0인 경우, 부동소수점 0으로 변환하여 반환
    if (in == 0) {
        return 0;
    }
    
    unsigned int sign = 0;     // 부호 비트를 저장할 변수 (0: 양수, 1: 음수)
    unsigned int abs_val;      // 입력값의 절대값을 저장할 변수
    
    // 입력값의 부호를 확인하고 절대값을 계산
    if (in < 0) {
        sign = 1; // 음수이므로 부호 비트를 1로 설정
        // 가장 작은 정수(INT_MIN, 0x80000000)는 부호를 바꾸면 오버플로우가 발생하므로 특별 처리
        if (in == 0x80000000) {
            abs_val = 0x80000000u; // 부호 없는 정수로 변환하여 절대값으로 사용
        } else {
            abs_val = (unsigned int)(-in); // 그 외 음수는 부호를 바꿔 절대값으로 만듦
        }
    } else {
        abs_val = (unsigned int)in; // 양수는 그대로 부호 없는 정수로 변환
    }
    
    // 절대값에서 가장 왼쪽에 있는 1 비트(MSB, Most Significant Bit)의 위치를 찾음
    int leading_bit = 31; // 32비트 정수의 최상위 비트 인덱스부터 시작
    // 루프를 돌며 MSB를 찾음. (abs_val >> leading_bit) & 1 이 1이 될 때까지
    while (leading_bit >= 0 && !((abs_val >> leading_bit) & 1)) {
        leading_bit--; // MSB가 아니면 인덱스를 1 감소
    }
    
    // 만약 leading_bit가 0보다 작으면, abs_val이 0이라는 의미 (이미 처리했지만 안전장치)
    if (leading_bit < 0) {
        return 0;
    }
    
    // 지수(exponent) 계산. 정규화된 표현에서 지수는 MSB의 위치와 관련됨
    // 바이어스(127)를 더해줌
    int exp = leading_bit + TF32_EXP_BIAS;
    
    // 가수(fraction) 계산 및 반올림 처리
    unsigned int frac;
    // MSB 위치가 가수 비트 수(10)보다 크거나 같으면, 오른쪽으로 시프트하여 가수를 구함
    if (leading_bit >= TF32_FRAC_BITS) {
        int shift = leading_bit - TF32_FRAC_BITS; // 시프트할 비트 수 계산
        // MSB를 제외하고 그 다음 10비트를 가수로 추출
        frac = (abs_val >> shift) & TF32_FRAC_MASK;

        // Round-to-nearest-even (가장 가까운 짝수로 반올림) 처리
        if (shift > 0) {
            // 시프트로 인해 잘려나간 나머지 비트들을 구함
            unsigned int remainder = abs_val & ((1u << shift) - 1);
            // 중간값(half)을 계산 (잘려나간 비트들의 중간 지점)
            unsigned int half = 1u << (shift - 1);
            // 나머지가 중간값보다 크거나, 중간값과 같으면서 가수의 마지막 비트가 1(홀수)이면 반올림
            if (remainder > half || (remainder == half && (frac & 1))) {
                frac++; // 가수를 1 증가
                // 반올림으로 인해 가수가 10비트를 초과하면(오버플로우)
                if (frac > TF32_FRAC_MASK) {
                    frac = 0;   // 가수는 0으로 리셋
                    exp++;      // 지수를 1 증가
                }
            }
        }
    } else {
        // MSB 위치가 가수 비트 수보다 작으면, 왼쪽으로 시프트하여 가수를 구함
        // (MSB 제외하고) 남은 비트들을 가수의 상위 비트로 채움
        frac = (abs_val << (TF32_FRAC_BITS - leading_bit)) & TF32_FRAC_MASK;
    }
    
    // 계산된 부호, 지수, 가수를 19비트 tf32 형식으로 조합하여 반환
    return (sign << 18) | (exp << TF32_EXP_SHIFT) | frac;
}

// tf32 형식을 가장 가까운 정수로 변환하는 함수
int tf322int(tf32 in) {
    // 입력된 tf32 값에서 부호, 지수, 가수를 각각 추출
    unsigned int sign = (in >> 18) & 1;
    unsigned int exp = (in >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac = in & TF32_FRAC_MASK;
    
    // 특수값 처리: 지수가 모두 1인 경우 (무한대 또는 NaN)
    if (exp == 0xFF) {
        // 가수가 0이면 무한대. 부호에 따라 INT_MIN 또는 INT_MAX 반환
        // 가수가 0이 아니면 NaN. INT_MIN을 반환 (과제 요구사항에 따름)
        return (frac == 0) ? (sign ? 0x80000000 : 0x7FFFFFFF) : 0x80000000;
    }
    
    // 지수가 0인 경우 (0 또는 비정규화된 수)
    if (exp == 0) {
        // 이들은 모두 1보다 작은 값이므로 정수로 변환 시 0이 됨
        return 0;
    }
    
    // 실제 지수(true exponent) 계산: 바이어스(127)를 뺌
    int true_exp = (int)exp - TF32_EXP_BIAS;
    
    // 실제 지수가 -1보다 작으면 값은 0.5보다 작으므로 정수 0으로 반올림
    if (true_exp < -1) {
        return 0;
    }
    
    // 가수(mantissa) 복원: 정규화된 수는 암시적인 1이 맨 앞에 있으므로 이를 추가
    unsigned int mantissa = (1u << TF32_FRAC_BITS) | frac;
    unsigned int result_uint; // 부호 없는 정수 결과를 저장할 변수
    
    // 실제 지수에 따라 시프트 연산을 수행하여 정수 값을 계산
    if (true_exp >= TF32_FRAC_BITS) {
        // 왼쪽으로 시프트해야 하는 경우 (값이 커짐)
        // 시프트 양이 너무 커서 32비트 정수 범위를 초과할 경우 오버플로우 처리
        if (true_exp - TF32_FRAC_BITS > 21) { // 11비트 가수 + 21비트 시프트 = 32비트 초과
            return sign ? 0x80000000 : 0x7FFFFFFF; // 부호에 따라 TMin 또는 TMax 반환
        }
        // 가수를 왼쪽으로 시프트하여 정수 값을 만듦
        result_uint = mantissa << (true_exp - TF32_FRAC_BITS);
    } else {
        // 오른쪽으로 시프트해야 하는 경우 (소수점 이하가 생김)
        int shift = TF32_FRAC_BITS - true_exp; // 시프트할 비트 수
        // 잘려나갈 비트들을 remainder로 저장
        unsigned int remainder = mantissa & ((1u << shift) - 1);
        // 가수를 오른쪽으로 시프트하여 정수 부분만 남김
        result_uint = mantissa >> shift;
        
        // Round-to-nearest-even 반올림 처리
        unsigned int half = 1u << (shift - 1); // 중간값
        if (remainder > half || (remainder == half && (result_uint & 1))) {
            result_uint++; // 반올림 조건에 맞으면 결과값을 1 증가
        }
    }
    
    // 부호를 적용하고 최종적으로 정수 범위를 초과하는지 확인
    if (sign == 0) { // 양수인 경우
        // 결과값이 양의 정수 최대값(TMax)을 초과하면 TMax를 반환
        if (result_uint > 0x7FFFFFFFu) {
            return 0x7FFFFFFF;
        }
        return (int)result_uint; // 범위 내에 있으면 int로 형변환하여 반환
    } else { // 음수인 경우
        // 결과값의 절대값이 음의 정수 최소값(TMin)의 절대값을 초과하면 TMin을 반환
        if (result_uint > 0x80000000u) {
            return 0x80000000;
        }
        // 결과값의 절대값이 정확히 TMin의 절대값과 같으면 TMin을 반환
        if (result_uint == 0x80000000u) {
            return (int)0x80000000;
        }
        // 그 외의 경우, 부호를 붙여서 반환
        return -(int)result_uint;
    }
}

// double형 실수를 tf32 형식으로 변환하는 함수
tf32 double2tf32(double in) {
    // union을 사용하여 double 값의 비트 표현에 직접 접근
    union { double f; unsigned long long u; } x;
    x.f = in;
    
    unsigned long long bits = x.u; // double의 64비트 표현을 가져옴
    // double의 비트 표현에서 부호(1), 지수(11), 가수(52)를 추출
    unsigned long long sign = (bits >> 63) & 1;
    unsigned long long exp = (bits >> 52) & 0x7FF;
    unsigned long long frac = bits & 0xFFFFFFFFFFFFFull;
    
    // 특수값 처리: 지수가 모두 1인 경우 (무한대 또는 NaN)
    if (exp == 0x7FF) {
        if (frac == 0) { // 가수가 0이면 무한대
            return (sign << 18) | 0x3FC00; // tf32의 무한대 값으로 변환
        } else { // 가수가 0이 아니면 NaN
            return 0x3FE00; // tf32의 NaN 값으로 변환 (부호 없는 NaN)
        }
    }
    
    // 입력값이 0인 경우
    if (exp == 0 && frac == 0) {
        return sign << 18; // 부호 비트만 설정된 0을 반환 (+0 또는 -0)
    }
    
    // 정규화 과정
    int double_exp;

    if (exp == 0) { // double이 비정규화된 수인 경우
        double_exp = 1 - 1023; // 비정규화 수의 지수는 1 - 바이어스
        // 암시적 1(hidden bit)이 1이 될 때까지 가수를 왼쪽으로 시프트하고 지수를 감소
        while ((frac & 0x10000000000000ull) == 0) {
            frac <<= 1;
            double_exp--;
        }
        frac &= 0xFFFFFFFFFFFFFull; // 정규화 후 암시적 1을 제거
    } else { // double이 정규화된 수인 경우
        double_exp = (int)exp - 1023; // 실제 지수 계산 (지수 - 바이어스)
    }
    
    // double의 실제 지수를 tf32의 바이어스된 지수로 변환
    int tf32_exp = double_exp + TF32_EXP_BIAS;
    
    // 오버플로우 처리: tf32 지수가 표현 범위를 넘어서면 무한대로 처리
    if (tf32_exp >= 0xFF) {
        return (sign << 18) | 0x3FC00;
    }
    
    // 언더플로우 또는 비정규화 처리: tf32 지수가 0 이하일 경우
    if (tf32_exp <= 0) {
        // 값이 너무 작아 0으로 처리해야 하는 경우
        if (tf32_exp < -TF32_FRAC_BITS) {
            return sign << 18; // 부호 있는 0 반환
        }
        
        // tf32의 비정규화된 수로 만들기
        unsigned long long tf32_frac = frac >> (52 - TF32_FRAC_BITS);
        tf32_frac |= 0x400; // 암시적 1을 가수에 포함
        
        // tf32 지수가 0이 되도록 가수를 오른쪽으로 시프트
        int shift_amount = 1 - tf32_exp;
        tf32_frac >>= shift_amount;
        
        // 반올림 처리
        int total_shift = shift_amount + (52 - TF32_FRAC_BITS);
        if (total_shift < 52) {
            // 잘려나간 비트와 중간값을 이용해 반올림
            unsigned long long remainder = frac & ((1ull << total_shift) - 1);
            unsigned long long half = 1ull << (total_shift - 1);
            if (remainder > half || (remainder == half && (tf32_frac & 1))) {
                tf32_frac++;
            }
        }
        
        // 비정규화된 수는 지수가 0이므로, 부호와 가수만 조합하여 반환
        return (sign << 18) | (tf32_frac & TF32_FRAC_MASK);
    }
    
    // 정규화된 수 처리
    int shift = 52 - TF32_FRAC_BITS; // 52비트 가수를 10비트로 줄이기 위한 시프트 양
    unsigned long long tf32_frac = frac >> shift; // 가수를 오른쪽으로 시프트
    unsigned long long remainder = frac & ((1ull << shift) - 1); // 잘려나간 비트
    
    // Round-to-nearest-even 반올림 처리
    unsigned long long half = 1ull << (shift - 1);
    if (remainder > half || (remainder == half && (tf32_frac & 1))) {
        tf32_frac++; // 반올림
        if (tf32_frac > TF32_FRAC_MASK) { // 가수 오버플로우 발생 시
            tf32_frac = 0;   // 가수는 0으로
            tf32_exp++;      // 지수는 1 증가
            if (tf32_exp >= 0xFF) { // 지수도 오버플로우 되면 무한대로 처리
                return (sign << 18) | 0x3FC00;
            }
        }
    }
    
    // 최종적으로 부호, 지수, 가수를 조합하여 tf32 값 반환
    return (sign << 18) | (tf32_exp << TF32_EXP_SHIFT) | tf32_frac;
}


// tf32 형식을 double형 실수로 변환하는 함수
double tf322double(tf32 in) {
    // tf32에서 부호, 지수, 가수를 추출
    unsigned int sign = (in >> 18) & 1;
    unsigned int exp = (in >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac = in & TF32_FRAC_MASK;
    
    // union을 사용하여 double 비트 표현을 조립
    union { double f; unsigned long long u; } result;
    
    // 특수값 처리: 지수가 모두 1인 경우 (무한대 또는 NaN)
    if (exp == 0xFF) {
        if (frac == 0) { // 무한대
            // double의 무한대 비트 표현으로 설정
            result.u = ((unsigned long long)sign << 63) | 0x7FF0000000000000ull;
        } else { // NaN
            // double의 NaN 비트 표현으로 설정 (일반적인 quiet NaN)
            result.u = 0x7FF8000000000000ull;
        }
        return result.f; // double 값으로 반환
    }
    
    // 지수가 0인 경우 (0 또는 비정규화된 수)
    if (exp == 0) {
        if (frac == 0) { // 0인 경우
            result.u = (unsigned long long)sign << 63; // 부호 있는 0으로 설정
            return result.f;
        }
        // 비정규화된 수 처리: 이 부분은 모든 비정규수를 하나의 특정 작은 값으로 매핑하는 간략화된 구현으로 보임
        // 정석적인 방법은 가수의 MSB를 찾아 지수를 조정해야 함
        result.u = ((unsigned long long)sign << 63) | (897ull << 52);
        return result.f;
    }
    
    // 정규화된 수 처리
    // tf32의 실제 지수를 계산
    int true_exp = (int)exp - TF32_EXP_BIAS;
    // double의 바이어스된 지수로 변환
    int double_exp = true_exp + 1023;
    
    // 10비트 가수를 52비트 가수로 확장 (왼쪽으로 시프트)
    unsigned long long double_frac = (unsigned long long)frac << (52 - TF32_FRAC_BITS);
    // double의 비트 표현을 조립
    result.u = ((unsigned long long)sign << 63) |
              ((unsigned long long)double_exp << 52) |
              double_frac;
    
    return result.f; // 최종 double 값 반환
}

// 두 tf32 값을 더하는 함수
tf32 tf32_add(tf32 a, tf32 b) {
    // a와 b에서 각각 부호, 지수, 가수를 추출
    unsigned int sign_a = (a >> 18) & 1;
    unsigned int exp_a = (a >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_a = a & TF32_FRAC_MASK;
    
    unsigned int sign_b = (b >> 18) & 1;
    unsigned int exp_b = (b >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_b = b & TF32_FRAC_MASK;
    
    // 특수값 처리: a가 무한대 또는 NaN인 경우
    if (exp_a == 0xFF) {
        if (frac_a != 0) return 0x3FE00; // a가 NaN이면 NaN 반환
        // Inf + (-Inf)는 NaN
        if (exp_b == 0xFF && frac_b == 0 && sign_a != sign_b) return 0x3FE00;
        return a; // Inf + x = Inf
    }
    // b가 무한대 또는 NaN인 경우
    if (exp_b == 0xFF) {
        if (frac_b != 0) return 0x3FE00; // b가 NaN이면 NaN 반환
        return b;
    }
    
    // 0 처리: 한쪽이 0이면 다른 쪽 값을 반환
    if (exp_a == 0 && frac_a == 0) return b;
    if (exp_b == 0 && frac_b == 0) return a;
    
    // 지수가 더 큰 쪽을 a로 오도록 정렬 (계산 편의를 위해)
    if (exp_b > exp_a || (exp_b == exp_a && frac_b > frac_a)) {
        // 부호, 지수, 가수를 서로 교환
        unsigned int tmp_s = sign_a; sign_a = sign_b; sign_b = tmp_s;
        unsigned int tmp_e = exp_a; exp_a = exp_b; exp_b = tmp_e;
        unsigned int tmp_f = frac_a; frac_a = frac_b; frac_b = tmp_f;
    }
    
    // 가수(mantissa) 복원: 암시적 1을 추가. 비정규화 수는 1을 추가하지 않음.
    unsigned int mant_a = (exp_a == 0) ? frac_a : ((1u << TF32_FRAC_BITS) | frac_a);
    unsigned int mant_b = (exp_b == 0) ? frac_b : ((1u << TF32_FRAC_BITS) | frac_b);
    
    // 비정규화 수의 지수는 1로 간주하여 계산 (실제로는 1-바이어스 지수)
    int true_exp_a = (exp_a == 0) ? 1 : exp_a;
    int true_exp_b = (exp_b == 0) ? 1 : exp_b;
    
    // 지수 차이를 계산하고, 작은 쪽(b)의 가수를 오른쪽으로 시프트하여 소수점 위치를 맞춤
    int exp_diff = true_exp_a - true_exp_b;
    if (exp_diff > 15) { // 차이가 너무 크면 b는 결과에 영향을 주지 않으므로 무시
        return a;
    }
    
    unsigned int shifted_b = mant_b >> exp_diff; // b의 가수를 시프트
    unsigned int result_mant; // 결과 가수를 저장할 변수
    unsigned int result_sign; // 결과 부호를 저장할 변수
    int result_exp = true_exp_a; // 결과 지수는 큰 쪽(a) 지수로 시작
    
    // 부호에 따라 덧셈 또는 뺄셈 수행
    if (sign_a == sign_b) { // 부호가 같으면 덧셈
        result_sign = sign_a;
        result_mant = mant_a;
        result_mant += shifted_b;
        
        // 덧셈으로 인해 가수 오버플로우(캐리)가 발생하면
        if (result_mant & (1u << (TF32_FRAC_BITS + 1))) {
            result_mant >>= 1; // 가수를 오른쪽으로 1 시프트
            result_exp++;      // 지수를 1 증가 (정규화)
        }
    } else { // 부호가 다르면 뺄셈
        // 이미 a가 b보다 크거나 같도록 정렬했으므로 mant_a >= shifted_b
        if (mant_a >= shifted_b) {
            result_sign = sign_a;
            result_mant = mant_a;
            result_mant -= shifted_b;
        } else { // 이 경우는 발생하지 않아야 함 (정렬했기 때문)
            result_sign = sign_b;
            result_mant = shifted_b;
            result_mant -= mant_a;
        }
        
        // 뺄셈 결과가 0이면 0 반환
        if (result_mant == 0) return 0;
        
        // 뺄셈으로 인해 가수의 MSB가 0이 되면 정규화 필요
        while (result_mant < (1u << TF32_FRAC_BITS) && result_exp > 1) {
            result_mant <<= 1; // 가수를 왼쪽으로 시프트
            result_exp--;      // 지수를 1 감소
        }
    }
    
    // 최종 지수가 오버플로우되면 무한대 반환
    if (result_exp >= 0xFF) {
        return (result_sign << 18) | 0x3FC00;
    }
    
    // 결과 구성: 암시적 1을 제거하고 최종 가수를 구함
    // 이 코드에서는 반올림 단계가 생략되어 있음
    unsigned int result_frac = result_mant & TF32_FRAC_MASK;
    // 부호, 지수, 가수를 조합하여 최종 tf32 값 반환
    return (result_sign << 18) | (result_exp << TF32_EXP_SHIFT) | result_frac;
}

// 두 tf32 값을 곱하는 함수
tf32 tf32_mul(tf32 a, tf32 b) {
    // a와 b에서 부호, 지수, 가수를 추출
    unsigned int sign_a = (a >> 18) & 1;
    unsigned int exp_a = (a >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_a = a & TF32_FRAC_MASK;
    
    unsigned int sign_b = (b >> 18) & 1;
    unsigned int exp_b = (b >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_b = b & TF32_FRAC_MASK;
    
    // 결과의 부호는 두 부호의 XOR 연산으로 결정
    unsigned int result_sign = sign_a ^ sign_b;
    
    // 특수값 처리
    if (exp_a == 0xFF) { // a가 무한대 또는 NaN
        if (frac_a != 0) return 0x3FE00; // a가 NaN -> NaN
        if (exp_b == 0 && frac_b == 0) return 0x3FE00; // Inf * 0 -> NaN
        return (result_sign << 18) | 0x3FC00; // Inf * x -> Inf
    }
    
    if (exp_b == 0xFF) { // b가 무한대 또는 NaN
        if (frac_b != 0) return 0x3FE00; // b가 NaN -> NaN
        if (exp_a == 0 && frac_a == 0) return 0x3FE00; // 0 * Inf -> NaN
        return (result_sign << 18) | 0x3FC00; // x * Inf -> Inf
    }
    
    // 0 처리: 둘 중 하나라도 0이면 결과는 0
    if ((exp_a == 0 && frac_a == 0) || (exp_b == 0 && frac_b == 0)) {
        return result_sign << 18; // 부호 있는 0 반환
    }
    
    // 가수 복원 (암시적 1 추가, 비정규화 수는 예외)
    unsigned int mant_a = (exp_a == 0) ? frac_a : ((1u << TF32_FRAC_BITS) | frac_a);
    unsigned int mant_b = (exp_b == 0) ? frac_b : ((1u << TF32_FRAC_BITS) | frac_b);
    
    // 비정규화 수의 지수를 1로 간주하여 처리
    int true_exp_a = (exp_a == 0) ? 1 : exp_a;
    int true_exp_b = (exp_b == 0) ? 1 : exp_b;
    
    // 가수 곱셈 (11비트 * 11비트 = 최대 22비트 결과)
    unsigned int product = mant_a * mant_b;
    
    // 지수 계산: (exp_a - bias) + (exp_b - bias) = (exp_a + exp_b - bias) - bias
    // 바이어스를 한번만 빼주기 위해 아래와 같이 계산
    int result_exp = (true_exp_a - TF32_EXP_BIAS) + (true_exp_b - TF32_EXP_BIAS) + TF32_EXP_BIAS;
    
    // 곱셈 결과 정규화
    int shift = 0;
    // 곱셈 결과가 22비트이면(MSB가 21번째 비트) 정규화를 위해 오른쪽 시프트하고 지수 증가
    if (product & (1u << (TF32_FRAC_BITS * 2 + 1))) {
        shift = TF32_FRAC_BITS + 1;
        result_exp++;
    } else { // 결과가 21비트이면
        shift = TF32_FRAC_BITS;
    }
    
    // 정규화된 곱셈 결과에서 상위 10비트를 가수로 추출
    unsigned int result_frac = (product >> shift) & TF32_FRAC_MASK;
    
    // Round-to-nearest-even 반올림 처리
    if (shift > 0) {
        // 잘려나간 비트와 중간값을 이용
        unsigned int remainder = product & ((1u << shift) - 1);
        unsigned int half = 1u << (shift - 1);
        if (remainder > half || (remainder == half && (result_frac & 1))) {
            result_frac++; // 반올림
            if (result_frac > TF32_FRAC_MASK) { // 가수 오버플로우
                result_frac = 0;
                result_exp++;
            }
        }
    }
    
    // 지수 오버플로우 체크
    if (result_exp >= 0xFF) {
        return (result_sign << 18) | 0x3FC00; // 무한대 반환
    }
    
    // 지수 언더플로우 체크
    if (result_exp <= 0) {
        // 이 구현에서는 비정규화 수 대신 0으로 처리
        return result_sign << 18; // 0 반환
    }
    
    // 최종 결과 조합하여 반환
    return (result_sign << 18) | (result_exp << TF32_EXP_SHIFT) | result_frac;
}

// 두 tf32 값을 나누는 함수 (Newton-Raphson 방법을 사용)
tf32 tf32_div(tf32 a, tf32 b) {
    // a와 b에서 부호, 지수, 가수를 추출
    unsigned int sign_a = (a >> 18) & 1;
    unsigned int exp_a = (a >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_a = a & TF32_FRAC_MASK;
    
    unsigned int sign_b = (b >> 18) & 1;
    unsigned int exp_b = (b >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_b = b & TF32_FRAC_MASK;
    
    // 결과 부호는 XOR 연산으로 결정
    unsigned int result_sign = sign_a ^ sign_b;
    
    // 특수값 처리
    if (exp_a == 0xFF) { // a가 무한대 또는 NaN
        if (frac_a != 0) return 0x3FE00; // NaN / x -> NaN
        if (exp_b == 0xFF) return 0x3FE00; // Inf / Inf -> NaN
        return (result_sign << 18) | 0x3FC00; // Inf / x -> Inf
    }
    
    if (exp_b == 0xFF) { // b가 무한대 또는 NaN
        if (frac_b != 0) return 0x3FE00; // x / NaN -> NaN
        return result_sign << 18; // x / Inf -> 0
    }
    
    // 0으로 나누기 처리
    if (exp_b == 0 && frac_b == 0) {
        if (exp_a == 0 && frac_a == 0) return 0x3FE00; // 0 / 0 -> NaN
        return (result_sign << 18) | 0x3FC00; // x / 0 -> Inf
    }
    
    // 피제수(a)가 0인 경우
    if (exp_a == 0 && frac_a == 0) {
        return result_sign << 18; // 0 / x -> 0
    }
    
    // Newton-Raphson 방법을 사용하여 1/|b|를 근사적으로 계산
    // y_{i+1} = y_i * (2 - |b| * y_i)
    int iter = 5; // 반복 횟수, 높을수록 정확도 증가
    
    // b의 절대값을 만듦 (부호 비트 제거)
    tf32 b_abs = b & 0x3FFFF;
    
    // b의 실제 지수 값을 추정
    int exp_val;
    if (exp_b == 0) { // b가 비정규화된 수인 경우
        unsigned int temp = frac_b;
        exp_val = -126; // 비정규화 수의 가장 큰 값의 지수
        // 가수의 MSB를 찾아 실제 지수를 조정
        while ((temp & 0x200) == 0) {
            temp <<= 1;
            exp_val--;
        }
    } else { // b가 정규화된 수인 경우
        exp_val = (int)exp_b - TF32_EXP_BIAS;
    }
    
    // 1/b의 지수는 -exp_val이 됨
    int neg_exp = -exp_val;
    
    // 1/|b|의 초기 추정값 y를 구함. y ≈ 2^(-exp_val)
    tf32 y;
    int init_exp = neg_exp + TF32_EXP_BIAS; // y의 바이어스된 지수
    if (init_exp <= 0) {
        y = 0; // 언더플로우
    } else if (init_exp >= 0xFF) {
        y = 0x3FC00; // 오버플로우
    } else {
        y = init_exp << TF32_EXP_SHIFT; // 가수는 0으로 두고 지수만 설정
    }
    
    // tf32로 표현된 2.0 (지수: 1+127=128, 가수: 0)
    tf32 two = (TF32_EXP_BIAS + 1) << TF32_EXP_SHIFT;
    
    // Newton-Raphson 반복: y = y * (2 - |b| * y)
    for (int i = 0; i < iter; i++) {
        tf32 b_times_y = tf32_mul(b_abs, y);
        // (2 - b*y)를 계산. 덧셈을 이용하기 위해 b*y의 부호를 반전시켜 더함
        tf32 two_minus_by = tf32_add(two, b_times_y ^ 0x40000);
        y = tf32_mul(y, two_minus_by);
    }
    
    // 최종 결과: a / b = a * (1/b)
    tf32 result = tf32_mul(a, y);
    
    // 처음에 계산해둔 최종 부호를 적용
    if (result_sign) {
        result |= 0x40000; // 부호 비트를 1로 설정
    } else {
        result &= 0x3FFFF; // 부호 비트를 0으로 설정
    }
    
    return result;
}

// 두 tf32 값을 비교하는 함수
// a > b 이면 1, a < b 이면 -1, a == b 이면 0, 둘 중 하나가 NaN이면 -2 반환
int tf32_compare(tf32 a, tf32 b) {
    // a와 b에서 각각 부호, 지수, 가수를 추출
    unsigned int sign_a = (a >> 18) & 1;
    unsigned int exp_a = (a >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_a = a & TF32_FRAC_MASK;
    
    unsigned int sign_b = (b >> 18) & 1;
    unsigned int exp_b = (b >> TF32_EXP_SHIFT) & 0xFF;
    unsigned int frac_b = b & TF32_FRAC_MASK;
    
    // NaN 처리: 지수가 모두 1이고 가수가 0이 아닌 경우
    if ((exp_a == 0xFF && frac_a != 0) || (exp_b == 0xFF && frac_b != 0)) {
        return -2; // NaN이 포함된 비교는 특별값 -2 반환
    }
    
    // +0과 -0은 같다고 처리
    if ((exp_a == 0 && frac_a == 0) && (exp_b == 0 && frac_b == 0)) {
        return 0;
    }
    
    // 부호가 다른 경우 (0은 이미 처리됨)
    if (sign_a != sign_b) {
        if (sign_a) return -1; // a가 음수, b가 양수이므로 a < b
        else return 1; // a가 양수, b가 음수이므로 a > b
    }
    
    // 부호가 같은 경우
    int comparison;
    
    // 비트 표현이 완전히 같으면 두 수는 같음
    if (a == b) {
        comparison = 0;
    // 지수가 다르면 지수가 큰 쪽이 절대값이 큼
    } else if (exp_a != exp_b) {
        comparison = (exp_a > exp_b) ? 1 : -1;
    } else { // 지수가 같으면 가수가 큰 쪽이 절대값이 큼
        comparison = (frac_a > frac_b) ? 1 : -1;
    }
    
    // 만약 두 수가 음수였다면, 절대값 비교 결과를 반전시켜야 함
    if (sign_a) {
        comparison = -comparison;
    }
    
    return comparison;
}