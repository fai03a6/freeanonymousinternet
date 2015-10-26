#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <openssl/sha.h>
#include <iostream>
#include <uint256.h>
#include <hash.h>
#include "mhash.h"
#include "utilstrencodings.h"
#include <math.h> 
#if defined(USE_SSE2) && !defined(USE_SSE2_ALWAYS)
#ifdef _MSC_VER
// MSVC 64bit is unable to use inline asm
#include <intrin.h>
#else
// GCC Linux or i686-w64-mingw32
#include <cpuid.h>
#endif
#endif

/*How mixed Hash works
 * First, get a normal sha256d hash of the block header
 * rolling for nBlockHeight times, occuping memory.
 * each roll,xor with a random number, and self mix add.cache to array.
 * Then roll for another nBlockHeight times
 * each roll, add with a random memory cache ,then xor with a random number, and self mix add.
 * Finally, attach to the block header hash, and calc a sha256d hash.
 * This design enables ultra fast computing with huge rams, with changing rolling times, so as to disable mining machine.
 * 
 */
void mixHash(uint256* input, const unsigned int height) {
    uint256 mHashRnd[8];
    getRandom(mHashRnd);
    uint256 roller;
    roller = *input;
    int r;
    uint256 *mixer;
    unsigned int rounds=(unsigned int)int(16*sqrt((double)height));
    mixer = new uint256[rounds];
    for (unsigned int i = 0; i < rounds; i++) {
        roller ^= mHashRnd[i & 7];
        mixAdd(&roller);
        mixer[i] = roller;

    }
    for (unsigned int i = 0; i < rounds; i++) {
        r = (unsigned int) (roller.GetLow64() & 0xffffffff) % rounds;
        roller += mixer[r];
        roller ^= mHashRnd[i & 7];
        mixAdd(&roller);
    }
    *input = Hash(BEGIN(roller), END(roller), BEGIN(*input), END(*input));
    delete[] mixer;
}

void getRandom(uint256* mHashRnd) {
    mHashRnd[0].SetHex("0x95faaa7b5ccde209b9cdebd1de254102795a295a883354449555f5a4e8f7493e");
    mHashRnd[1].SetHex("0xd89d9124591df8a0e45fe616535627a61302f599c882f4aa622c2a9223661937");
    mHashRnd[2].SetHex("0xc9a7798889ffe05159c37b769a1b914ddd5e94610463d029acf288f37649c772");
    mHashRnd[3].SetHex("0x0c362798f117ebc70aff6afc7d49b3982132e3244702fc417b22fcfcbb0cd230");
    mHashRnd[4].SetHex("0xf648c988f4f5e6e2f9d6b292a622c8cfe5016acd63dcafcf2f26002743b90d35");
    mHashRnd[5].SetHex("0x059140f332690e0c6d37160d4444056a3f4135614bc93f2243f8c6118a69edd1");
    mHashRnd[6].SetHex("0x59b7c70950352b733d074b5bad162c9d3b732f1acbcf29e4b2c47eab5a5615c5");
    mHashRnd[7].SetHex("0x61dcfeae08c1d458ee5e6a65bb26c6419f64a15c169a94a9de34a11ab3753b39");

}

void mixAdd(uint256* roller) {
    unsigned int shift = roller->GetLow64()&0xff;
    uint256 lchunk = *roller>>shift;
    uint256 r1 = *roller << (unsigned int) (256 - shift);
    r1 |= lchunk;
    *roller += r1;
}



