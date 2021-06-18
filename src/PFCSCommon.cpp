#include "PFCSCommon.h"

void intToStr(char* dest, uint32_t value, uint32_t n){
    sprintf(dest, "%0*d", n, value);
}