#include <assert.h>

#include "tmp102_driver.h"

int main(void)
{
    assert(tmp102_decode_celsius(0x1900) == 25);
    assert(tmp102_decode_celsius(0x0000) == 0);
    assert(tmp102_decode_celsius(0xe700) == -25);
    assert(tmp102_decode_celsius(0x5000) == 80);

    return 0;
}
