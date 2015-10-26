// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u,%d.%06d)", hash.ToString().substr(0,10), n,nValue/COIN, nValue % COIN);
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CAmount nValueIn,CScript scriptSigIn)
{
    prevout = COutPoint(hashPrevTx, nOut,nValueIn);
    scriptSig = scriptSigIn;
    
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0,24));
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn,string strContentIn,uint32_t nLockTimeIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
    strContent=strContentIn;
    nLockTime=nLockTimeIn;
}

uint256 CTxOut::GetHash() const
{
    return SerializeHash(*this);
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%06d, scriptPubKey=%s,strContent=%s, nLockTime=%u)", nValue / COIN, nValue % COIN, scriptPubKey.ToString().substr(0,30),HexStr(strContent.length()>100? strContent.substr(0,100):strContent),nLockTime);
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : nVersion(tx.nVersion), vin(tx.vin), vout(tx.vout) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this);
}

void CTransaction::UpdateHash() const
{
    *const_cast<uint256*>(&hash) = SerializeHash(*this);
}

CTransaction::CTransaction() : hash(0), nVersion(CTransaction::CURRENT_VERSION), vin(), vout(){ }

CTransaction::CTransaction(const CMutableTransaction &tx) : nVersion(tx.nVersion), vin(tx.vin), vout(tx.vout) {
    UpdateHash();
}

CTransaction& CTransaction::operator=(const CTransaction &tx) {
    *const_cast<int*>(&nVersion) = tx.nVersion;
    *const_cast<std::vector<CTxIn>*>(&vin) = tx.vin;
    *const_cast<std::vector<CTxOut>*>(&vout) = tx.vout;
    *const_cast<uint256*>(&hash) = tx.hash;
    return *this;
}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    for (std::vector<CTxOut>::const_iterator it(vout.begin()); it != vout.end(); ++it)
    {
        nValueOut += it->nValue;
        if (!MoneyRange(it->nValue) || !MoneyRange(nValueOut))
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range");
    }
    return nValueOut;
}
CAmount CTransaction::GetValueIn() const
{
    CAmount nValueIn = 0;
    for (std::vector<CTxIn>::const_iterator it(vin.begin()); it != vin.end(); ++it)
    {
        nValueIn += it->prevout.nValue;
        if (!MoneyRange(it->prevout.nValue) || !MoneyRange(nValueIn))
            throw std::runtime_error("CTransaction::GetValueIn() : value out of range");
    }
    return nValueIn;
}
CAmount CTransaction::GetFee() const
{
    return GetValueIn()-GetValueOut();
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        vin.size(),
        vout.size());
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}
double  CTransaction::GetFeeRate() const{
    return (double)GetFee()/GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION);
}
