// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"

#include "tinyformat.h"

CFeeRate::CFeeRate(const CAmount& nFeePaid, size_t nSize)
{
    if (nSize > 0)
        nSatoshisPerB = nFeePaid/nSize;
    else
        nSatoshisPerB = 0;
}

CAmount CFeeRate::GetFee(size_t nSize) const
{
    CAmount nFee = nSatoshisPerB*nSize;

    if (nFee == 0 && nSatoshisPerB > 0)
        nFee = nSatoshisPerB;

    return nFee;
}

std::string CFeeRate::ToString() const
{
    return strprintf("%d.%06d φ/B", nSatoshisPerB / COIN, nSatoshisPerB % COIN);
}
