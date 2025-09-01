#include "protocol_utils.h"

uint16_t u16_calculateCRC(const uint8_t *buffer, uint8_t length){
    uint16_t crc = 0xFFFF;
    uint8_t shiftctr;
    const uint8_t * pblock = buffer;

    while(length > 0) {
        length--;
        crc ^= (uint16_t)*pblock++;
        shiftctr = 8;
        do {
            if(crc & 0x0001){
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
                crc >>= 1;
            
            shiftctr--;
        }while(shiftctr > 0);
    }
    return crc;
}