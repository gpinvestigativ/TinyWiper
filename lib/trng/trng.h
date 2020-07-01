#include <stdint.h>

void trng_init();
void trng_deinit();
void trng512(uint32_t *udata);
uint32_t trng_word();