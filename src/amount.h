// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_AMOUNT_H
#define BITCOIN_AMOUNT_H

#include "serialize.h"

#include <stdlib.h>
#include <string>

typedef int64_t CAmount;

static const CAmount COIN = 1000000;
static const CAmount CENT = 10000;

/** No amount larger than this (in satoshi) is valid */
static const CAmount MAX_MONEY = 210000000000 * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

/** Type-safe wrapper class to for fee rates
 * (how much to pay based on transaction size)
 */
class CFeeRate
{
private:
    CAmount nSatoshisPerB; // unit is satoshis-per-byte
public:
    CFeeRate() : nSatoshisPerB(0) { }
    explicit CFeeRate(const CAmount& _nSatoshisPerB): nSatoshisPerB(_nSatoshisPerB) { }
    CFeeRate(const CAmount& nFeePaid, size_t nSize);
    CFeeRate(const CFeeRate& other) { nSatoshisPerB = other.nSatoshisPerB; }

    CAmount GetFee(size_t size) const; // unit returned is satoshis
    CAmount GetFeePerK() const { return GetFee(1000); } // satoshis-per-1000-bytes

    friend bool operator<(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerB < b.nSatoshisPerB; }
    friend bool operator>(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerB > b.nSatoshisPerB; }
    friend bool operator==(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerB == b.nSatoshisPerB; }
    friend bool operator<=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerB <= b.nSatoshisPerB; }
    friend bool operator>=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerB >= b.nSatoshisPerB; }
    std::string ToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nSatoshisPerB);
    }
};

#endif //  BITCOIN_AMOUNT_H
