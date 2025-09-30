#include <stdio.h>
#include "tf32.h"

int main() {
    // Test case: 0x20000 + 0x60200
    tf32 a = 0x20000;
    tf32 b = 0x60200;
    
    printf("a = 0x%05x\n", a);
    printf("b = 0x%05x\n", b);
    
    double val_a = tf322double(a);
    double val_b = tf322double(b);
    
    printf("tf322double(a) = %f\n", val_a);
    printf("tf322double(b) = %f\n", val_b);
    printf("Sum = %f\n", val_a + val_b);
    
    tf32 result = tf32_add(a, b);
    printf("tf32_add(a, b) = 0x%05x\n", result);
    printf("tf322double(result) = %f\n", tf322double(result));
    
    printf("\nExpected: 0x1f800\n");
    printf("tf322double(0x1f800) = %f\n", tf322double(0x1f800));
    
    return 0;
}
