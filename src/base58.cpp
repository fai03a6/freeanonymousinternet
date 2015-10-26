// Copyright (c) 2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"

#include "hash.h"
#include "uint256.h"
#include "util.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/assign/list_of.hpp>
#include "utilstrencodings.h"
#include <boost/foreach.hpp>

using namespace std;

/** All alphanumeric characters except for "0", "I", "O", and "l" */
static const char* pszBase32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static const char* pszBase32Vague = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567abcdefghijklmnopqrstuvwxyz0189";
static const char* pszBase32Clear = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567ABCDEFGHIJKLMNOPQRSTUVWXYZOLBG";

bool DecodeBase32(const char* psz, std::vector<unsigned char>& vch)
{
    // Skip leading spaces.
    while (*psz && isspace(*psz))
        psz++;
    
    const char* v = strchr(pszBase32Vague, *psz);
    if (v == NULL)
        return false;
    int c = v - pszBase32Vague;
    const char* ch = strchr(pszBase32, *(pszBase32Clear + c));
    int buffer = ch - pszBase32;
    int bufferLen = 5;
            
    while (*psz && !isspace(*psz)) {
        while (bufferLen < 8) {
            if (!*++psz)
                break;
            buffer <<= 5;
            const char* v = strchr(pszBase32Vague, *psz);
            if (v == NULL)
                return false;
            c = v - pszBase32Vague;
            const char* ch = strchr(pszBase32, *(pszBase32Clear + c));
            buffer += ch - pszBase32;
            bufferLen += 5;
               
        }
        if (!*psz)
            break;
        bufferLen -= 8;
        vch.push_back(buffer >> bufferLen);
        buffer -= buffer >> bufferLen << bufferLen;
    } // Skip trailing spaces.
    while (isspace(*psz))
        psz++;
    if (*psz != 0)
        return false;
    return true;
}

std::string ToStandardB32String(const std::string str)
{
    const char* psz = str.c_str();
    std::string strOut;
    while (*psz) {
        if (isspace(*psz)) {
            psz++;
            continue;
    }
        const char* v = strchr(pszBase32Vague, *psz);
        if (v == NULL) {
            psz++;
            continue;
        }
        int c = v - pszBase32Vague;
        strOut.push_back(*(pszBase32Clear + c));
        psz++;
    }
    return strOut;
}

std::string EncodeBase32(const unsigned char* pbegin, const unsigned char* pend)
{
    std::string str;
    int buffer = *pbegin;
    int head = buffer;
    int bufferLen = 8;
    while (pbegin != pend) {
        if (bufferLen < 5) {
            if (++pbegin != pend) {
                buffer <<= 8;
                buffer += *pbegin;
                bufferLen += 8;
            } else if (bufferLen != 0) {
                buffer <<= 5 - bufferLen;
                buffer += head >> (3 + bufferLen);
                bufferLen = 5;
            }
        }
        if (bufferLen != 0) {
            bufferLen -= 5;
            str += pszBase32[buffer >> bufferLen];
            buffer -= buffer >> bufferLen << bufferLen;
        }
    }
    return str;
}

std::string EncodeBase32(const int i)
{
    std::string str = "";
    int t = i;
    if (t == 0)
        str += pszBase32[0];
    while (t > 0) {
        str += pszBase32[ t % 32];
        t = t / 32;
}
    return std::string(str.rbegin(), str.rend());
}

int DecodeBase32ToInt(const char* psz)
{
    while (*psz && isspace(*psz))
        psz++;
    const char* v = strchr(pszBase32Vague, *psz);
    int c = v - pszBase32Vague;
    const char* ch = strchr(pszBase32, *(pszBase32Clear + c));
    int r = ch - pszBase32;
    while (*psz && !isspace(*psz)) {
        if (!*++psz)
            break;
        if ((int64_t) (r * 32) > INT_MAX || r * 32 < r) // overflow
            return -1;
        r = r * 32;
        const char* v = strchr(pszBase32Vague, *psz);
        if (v == NULL)
            return false;
        c = v - pszBase32Vague;
        const char* ch = strchr(pszBase32, *(pszBase32Clear + c));
        r += ch - pszBase32;
    } // Skip trailing spaces.
    while (isspace(*psz))
        psz++;
    if (*psz != 0)
        return -1;
    return r;
}

int DecodeBase32ToInt(const std::string& str)
{
    return DecodeBase32ToInt(str.c_str());
}

std::string EncodeBase32(const std::vector<unsigned char>& vch)
{
    return EncodeBase32(&vch[0], &vch[0] + vch.size());
}

bool DecodeBase32(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase32(str.c_str(), vchRet);
}

std::string EncodeBase32Check(const std::vector<unsigned char>& vchIn)
{
    // add 4-byte hash check to the end
    std::vector<unsigned char> vch(vchIn);
    uint256 hash = Hash(vch.begin(), vch.end());
    vch.insert(vch.end(), (unsigned char*) &hash, (unsigned char*) &hash + 4);
    return EncodeBase32(vch);
}

bool DecodeBase32Check(const char* psz, std::vector<unsigned char>& vchRet)
{
    if (!DecodeBase32(psz, vchRet) ||
        (vchRet.size() < 4)) {
        vchRet.clear();
        return false;
    }
    
    // re-calculate the checksum, insure it matches the included 4-byte checksum
    uint256 hash = Hash(vchRet.begin(), vchRet.end() - 4);
    if (memcmp(&hash, &vchRet.end()[-4], 4) != 0) {
        vchRet.clear();
        return false;
    }
    vchRet.resize(vchRet.size() - 4);
    return true;
}

bool DecodeBase32Check(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase32Check(str.c_str(), vchRet);
}

int CompareVch(const std::vector<unsigned char>& vch1, const std::vector<unsigned char>& vch2)
{
    unsigned int s1 = vch1.size();
    unsigned int s2 = vch2.size();
    for (unsigned int i = 0; i < min(s1, s2); i++) {
        if (vch1.at(i) < vch2.at(i))
            return -1;
        if (vch1.at(i) > vch2.at(i))
            return 1;
}
    if (s1 < s2)
        return -1;
    else if (s1 > s2)
        return 1;
    else
        return 0;
}

int CompareBase32(const std::string& s1, const std::string& s2)
{
    std::vector<unsigned char> vch1;
    std::vector<unsigned char> vch2;
    if (!DecodeBase32(s1, vch1))
        return -1;
    if (!DecodeBase32(s2, vch2))
        return 1;
    return CompareVch(vch1, vch2);

}

CBase32Data::CBase32Data()
{
    vchVersion.clear();
    vchData.clear();
}

void CBase32Data::SetData(const std::vector<unsigned char>& vchVersionIn, const void* pdata, size_t nSize)
{
    vchVersion = vchVersionIn;
    vchData.resize(nSize);
    if (!vchData.empty())
        memcpy(&vchData[0], pdata, nSize);
}

void CBase32Data::SetData(const std::vector<unsigned char>& vchVersionIn, const unsigned char* pbegin, const unsigned char* pend)
{
    SetData(vchVersionIn, (void*) pbegin, pend - pbegin);
}

bool CBase32Data::SetString(const char* psz, unsigned int nVersionBytes)
{
    std::vector<unsigned char> vchTemp;
    bool rc32 = DecodeBase32Check(psz, vchTemp);
    if ((!rc32) || (vchTemp.size() < nVersionBytes)) {
        vchData.clear();
        vchVersion.clear();
        return false;
    }
    vchVersion.assign(vchTemp.end() - nVersionBytes, vchTemp.end());
    vchData.resize(vchTemp.size() - nVersionBytes);
    if (!vchData.empty())
        memcpy(&vchData[0], &vchTemp[0], vchData.size());
    OPENSSL_cleanse(&vchTemp[0], vchData.size());
    return true;
}

bool CBase32Data::SetString(const std::string& str)
{
    return SetString(str.c_str());
}

std::string CBase32Data::ToString() const
{
    std::vector<unsigned char> vch = vchVersion;
    vch.insert(vch.begin(), vchData.begin(), vchData.end());
    return EncodeBase32Check(vch);
}


int CBase32Data::CompareTo(const CBase32Data& b32) const
{
    if (vchVersion < b32.vchVersion)
        return -1;
    if (vchVersion > b32.vchVersion)
        return 1;
    if (vchData < b32.vchData)
        return -1;
    if (vchData > b32.vchData)
        return 1;
    return 0;
}


namespace
{

class CBitcoinAddressVisitor : public boost::static_visitor<bool>
{
private:
    CBitcoinAddress* addr;

public:
    CBitcoinAddressVisitor(CBitcoinAddress* addrIn) : addr(addrIn) {}

    bool operator()(const CPubKey& id) const { return addr->Set(id); }
    bool operator()(const CKeyID& id) const { return addr->Set(id); }
    bool operator()(const CScript& script) const { return addr->Set(script); }
    bool operator()(const CScriptID& id) const { return addr->Set(id); }
    bool operator()(const CNoDestination& no) const { return false; }
};

} // anon namespace

bool CBitcoinAddress::Set(const CKeyID& id)
{
    if(!id.IsValid())
        return false;
    std::string str;
    std::vector<unsigned char> tmp =id.GetData();
    std::vector<unsigned char>::const_iterator pc= tmp.begin();
    while (pc < tmp.end()){
            str += *pc++;
    }
    const char* sch = (const char*)str.c_str();;
    if(id.IsEvenY())
        SetData(Params().Base32Prefix(CChainParams::PUBKEY_ADDRESS_2), sch, 32);
    else
        SetData(Params().Base32Prefix(CChainParams::PUBKEY_ADDRESS_3), sch, 32);  
    return true;
}

bool CBitcoinAddress::Set(const CPubKey& id)
{
    if(!id.IsValid())
        return false;
    SetData(Params().Base32Prefix((CChainParams::Base32Type) * id.begin()), id.begin() + 1, id.size() - 1);
    return true;
}

bool CBitcoinAddress::Set(const CScriptID& id)
{
    SetData(Params().Base32Prefix(CChainParams::SCRIPTHASH_ADDRESS), &id, 20);
    return true;
}

bool CBitcoinAddress::Set(const CScript& script)
{
    if (script.size() == 0)
        return false;
    SetData(Params().Base32Prefix(CChainParams::SCRIPT_ADDRESS), (void*) &script[0], script.size());
    return true;
}

bool CBitcoinAddress::Set(const CTxDestination& dest)
{
    return boost::apply_visitor(CBitcoinAddressVisitor(this), dest);
}

bool CBitcoinAddress::IsValid() const
{
    return IsValid(Params());
}

bool CBitcoinAddress::IsValid(const CChainParams& params) const
{
    bool fCorrectSize = (vchVersion == params.Base32Prefix(CChainParams::PUBKEY_ADDRESS_2) ||
            vchVersion == params.Base32Prefix(CChainParams::PUBKEY_ADDRESS_3)) ? vchData.size() == 32 : vchData.size() == 20;
    bool fKnownVersion = vchVersion == params.Base32Prefix(CChainParams::PUBKEY_ADDRESS_2) ||
            vchVersion == params.Base32Prefix(CChainParams::PUBKEY_ADDRESS_3) ||
            vchVersion == params.Base32Prefix(CChainParams::SCRIPTHASH_ADDRESS);
    bool fCorrectScript = vchVersion == params.Base32Prefix(CChainParams::SCRIPT_ADDRESS);
    return (fCorrectSize && fKnownVersion) || fCorrectScript;
}

std::string CBitcoinAddress::ToString() const
{
    return IsValid() ? CBase32Data::ToString() : "";
}

CTxDestination CBitcoinAddress::Get() const
{
    if (!IsValid())
        return CNoDestination();
    if (vchVersion == Params().Base32Prefix(CChainParams::SCRIPT_ADDRESS)) {
        std::vector<unsigned char> s;

        BOOST_FOREACH(const unsigned char & ch, vchData)
        {
            s.push_back(ch);
        }
        return CScript(s.begin(), s.end());
    }
    if (vchVersion == Params().Base32Prefix(CChainParams::PUBKEY_ADDRESS_2) || vchVersion == Params().Base32Prefix(CChainParams::PUBKEY_ADDRESS_3)) {
        std::vector<unsigned char> id(vchData.begin(), vchData.end());
        id.insert(id.begin(), vchVersion.begin(), vchVersion.end());
        return CKeyID(id);
    } else if (vchVersion == Params().Base32Prefix(CChainParams::SCRIPTHASH_ADDRESS)) {
        uint160 id;
        memcpy(&id, &vchData[0], 20);
        return CScriptID(id);
    } else
        return CNoDestination();
}

bool CBitcoinAddress::GetKeyID(CKeyID& keyID) const
{
    unsigned char chHeader = 0;
    if (vchVersion == Params().Base32Prefix(CChainParams::PUBKEY_ADDRESS_2))
        chHeader = 2;
    else if (vchVersion == Params().Base32Prefix(CChainParams::PUBKEY_ADDRESS_3))
        chHeader = 3;
    if (!IsValid() || chHeader == 0)
        return false;
    std::vector<unsigned char> id(vchData.begin(), vchData.end());
    id.insert(id.begin(), vchVersion.begin(), vchVersion.end());
    keyID = CKeyID(id);
    return true;
}

bool CBitcoinAddress::IsScript() const
{
    return IsValid() && vchVersion == Params().Base32Prefix(CChainParams::SCRIPTHASH_ADDRESS);
}

void CBitcoinSecret::SetKey(const CKey& vchSecret)
{
    assert(vchSecret.IsValid());
        SetData(Params().Base32Prefix(CChainParams::SECRET_KEY), vchSecret.begin(), vchSecret.size());
}

CKey CBitcoinSecret::GetKey()
{
    CKey ret;
    assert(vchData.size() == 32);
    ret.Set(vchData.begin(), vchData.begin() + 32, true); // always get compressed key
    return ret;
}

bool CBitcoinSecret::IsValid() const
{
    bool fExpectedFormat = vchData.size() == 32;
    bool fCorrectVersion = vchVersion == Params().Base32Prefix(CChainParams::SECRET_KEY);
    return fExpectedFormat && fCorrectVersion;
}

bool CBitcoinSecret::SetString(const char* pszSecret)
{
    return CBase32Data::SetString(pszSecret) && IsValid();
}

bool CBitcoinSecret::SetString(const std::string& strSecret)
{
    return SetString(strSecret.c_str());
}

bool StringToScriptPubKey(const string& str, CScript& script)
{
    CBitcoinAddress address = CBitcoinAddress(str);
    if (!address.IsValid()) {
        return false;
    }
    script = GetScriptForDestination(address.Get());
    return true;
}

bool ScriptPubKeyToString(const CScript& script, string& str)
{
    txnouttype type;
    vector<CTxDestination> addresses;
    unsigned int wRequired;
    ExtractDestinations(script, type, addresses, wRequired);
    if (addresses.size() == 0)
        return false;
    switch((int)type)
    {
        case TX_PUBKEY:
        case TX_SCRIPTHASH:
            str = CBitcoinAddress(addresses[0]).ToString();
            break;
        case TX_SCRIPT:
        case TX_MULTISIG:
            str = CBitcoinAddress(script).ToString();
            break;
        default:
            return false;
    }
    return true;
}

bool B32Equal(const std::string str1, const std::string str2)
{
    return ToStandardB32String(str1) == ToStandardB32String(str2);
}
