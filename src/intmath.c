#include <pebble.h>
#include "intmath.h"

// Integer math procedures

// Integer division with rounding
int32_t divide(int32_t n, int32_t d)
{
  return ((n < 0) ^ (d < 0)) ? ((n - d/2) / d) : ((n + d/2) / d);
}

// Fast integer square-root with rounding
uint32_t intsqrt(uint32_t input)
{
    if (input == 0) return 0;
  
    uint32_t op  = input;
    uint32_t result = 0;
    uint32_t one = 1uL << 30; 

    // Find the highest power of four <= than the input.
    while (one > op) one >>= 2;
  
    // Find the root
    while (one != 0)
    {
        if (op >= result + one)
        {
            op = op - (result + one);
            result = result +  2 * one;
        }
        result >>= 1;
        one >>= 2;
    }

    // Round result
    if (op > result) result++;

    return result;
}