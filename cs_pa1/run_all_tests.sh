#!/bin/bash

# 색상 정의 (선택적)
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "======================================"
echo "  모든 테스트 프로그램 실행"
echo "======================================"
echo ""

# 테스트 프로그램 목록
TESTS=(
    "test_int2tf32"
    "test_tf322int"
    "test_double2tf32"
    "test_tf322double"
    "test_tf32_add"
    "test_tf32_mul"
    "test_tf32_div"
    "test_tf32_compare"
    "test_spec"
    "test_debug"
)

# 결과 카운터
TOTAL=0
PASSED=0
FAILED=0

# 각 테스트 실행
for test in "${TESTS[@]}"
do
    if [ -x "./$test" ]; then
        echo -e "${BLUE}[실행 중]${NC} $test"
        echo "--------------------------------------"
        
        # 테스트 실행 및 결과 캡처
        if ./"$test"; then
            echo -e "${GREEN}[성공]${NC} $test 통과"
            ((PASSED++))
        else
            echo -e "${RED}[실패]${NC} $test 실패"
            ((FAILED++))
        fi
        ((TOTAL++))
        echo ""
    else
        echo -e "${RED}[경고]${NC} $test 파일을 찾을 수 없거나 실행할 수 없습니다."
        echo ""
    fi
done

# 최종 결과 출력
echo "======================================"
echo "  테스트 결과 요약"
echo "======================================"
echo -e "전체: $TOTAL"
echo -e "${GREEN}성공: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}실패: $FAILED${NC}"
else
    echo -e "실패: $FAILED"
fi
echo "======================================"

# 모든 테스트가 성공하면 exit 0, 아니면 exit 1
if [ $FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi


