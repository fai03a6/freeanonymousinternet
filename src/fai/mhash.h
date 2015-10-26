#ifndef MHASH_H
#define MHASH_H
#include <stdlib.h>
#include <stdint.h>
#include "uint256.h"
void mixHash(uint256* input,const unsigned int height);
void mixAdd(uint256* roller);
void getRandom(uint256* mHashRnd);

#endif
