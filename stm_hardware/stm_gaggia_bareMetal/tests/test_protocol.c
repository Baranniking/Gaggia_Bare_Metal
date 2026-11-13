/* PC test for Core/Src/protocol.c. Includes the .c to reach static functions. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../Core/Src/protocol.c"
#include "gaggia_proto.h"



int main(void)
{
      uint16_t crc = proto_crc16((const uint8_t *)"123456789", 9, 0xFFFF);
    printf("crc = 0x%04X\n", crc);
    return 0
}
