#include "tmp102_driver.h"

int main(void)
{
    uint16_t raw = tmp102_read_raw();
    int temp_c = tmp102_decode_celsius(raw);

    (void)raw;
    (void)temp_c;

    while (1)
    {
    }
    
}