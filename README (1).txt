폴더 구조

아래처럼 프로젝트 루트에 파일이 있다고 가정합니다.
pa1/
├─ tf32.h
├─ tf32.c                // 당신의 구현 (int2tf32, tf322int, double2tf32, tf322double, tf32_add, tf32_mul, tf32_div, tf32_compare ...)
├─ test_int2tf32.c
├─ test_tf322int.c
├─ test_double2tf32.c
├─ test_tf322double.c
├─ test_tf32_add.c
├─ test_tf32_mul.c
├─ test_tf32_div.c
└─ test_tf32_compare.c
중요: 테스트 파일에서는 tf32.h만 #include하고, tf32.c는 빌드 단계에서 함께 컴파일하세요.
#include "tf32.c"는 피하세요(중복 정의 문제).

빌드 & 실행 (gcc/clang)
아래 명령을 프로젝트 루트에서 실행하세요.
# 1) int → tf32
gcc -std=c11 -O2 -Wall -Wextra tf32.c test_int2tf32.c -o test_int2tf32
./test_int2tf32

# 2) tf32 → int
gcc -std=c11 -O2 -Wall -Wextra tf32.c test_tf322int.c -o test_tf322int
./test_tf322int

# 3) double → tf32   (수학 라이브러리 필요)
gcc -std=c11 -O2 -Wall -Wextra tf32.c test_double2tf32.c -lm -o test_double2tf32
./test_double2tf32

# 4) tf32 → double   (수학 라이브러리 필요)
gcc -std=c11 -O2 -Wall -Wextra tf32.c test_tf322double.c -lm -o test_tf322double
./test_tf322double

# 5) tf32_add
gcc -std=c11 -O2 -Wall -Wextra tf32.c test_tf32_add.c -lm -o test_tf32_add
./test_tf32_add

# 6) tf32_mul
gcc -std=c11 -O2 -Wall -Wextra tf32.c test_tf32_mul.c -lm -o test_tf32_mul
./test_tf32_mul

# 7) tf32_div
gcc -std=c11 -O2 -Wall -Wextra tf32.c test_tf32_div.c -lm -o test_tf32_div
./test_tf32_div

# 8) tf32_compare
gcc -std=c11 -O2 -Wall -Wextra tf32.c test_tf32_compare.c -lm -o test_tf32_compare
./test_tf32_compare
