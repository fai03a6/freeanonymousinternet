// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "base58.h"
#include "script/script.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;

static const string strSecret1     ("6uGFQ4DSW7zh1viHZi6iiVT17CncvoaV4MHvGvJKPDaLCdymj87");
static const string strSecret2     ("6vVo7sPkeLTwVdAntrv4Gbnsyr75H8ChD3P5iyHziwaqe8mCYR5");
static const string strSecret1C    ("T3gJYmBuZXsdd65E7NQF88ZmUP2MaUanqnZg9GFS94W7kND4Ebjq");
static const string strSecret2C    ("T986ZKRRdnuuXLeDZuKBRrZW1ujotAncU9WTrFU1n7vMgRW75ZtF");
static const CBitcoinAddress addr1 ("LiUo6Zn39joYJBzPUhssbDwAywhjFcoHE3");
static const CBitcoinAddress addr2 ("LZJvLSP5SGKcFS13MHgdrVhpFUbEMB5XVC");
static const CBitcoinAddress addr1C("Lh2G82Bi33RNuzz4UfSMZbh54jnWHVnmw8");
static const CBitcoinAddress addr2C("LWegHWHB5rmaF5rgWYt1YN3StapRdnGJfU");


static const string strAddressBad("Lbi6bpMhSwp2CXkivEeUK9wzyQEFzHDfSr");


#ifdef KEY_TESTS_DUMPINFO
void dumpKeyInfo(uint256 privkey)
{
}
#endif


BOOST_AUTO_TEST_SUITE(key_tests)

BOOST_AUTO_TEST_CASE(key_test1)
{
}

BOOST_AUTO_TEST_SUITE_END()
