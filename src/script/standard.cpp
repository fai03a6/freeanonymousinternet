// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script/standard.h"

#include "pubkey.h"
#include "script/script.h"
#include "util.h"
#include "utilstrencodings.h"
#include "script.h"

#include <boost/foreach.hpp>
#include <vector>

using namespace std;

typedef vector<unsigned char> valtype;

unsigned nMaxDatacarrierBytes = MAX_OP_RETURN_RELAY;

CScriptID::CScriptID(const CScript& in) : uint160(Hash160(in.begin(), in.end())) {}

const char* GetTxnOutputType(txnouttype t)
{
    switch (t)
    {
        case TX_NONSTANDARD: return "nonstandard";
        case TX_PUBKEY: return "pubkey";
        case TX_SCRIPTHASH: return "scripthash";
        case TX_SCRIPT: return "script";
        case TX_MULTISIG: return "multisig";
        case TX_NULL_DATA: return "nulldata";
    }
    return NULL;
}

/**
 * Return public keys or hashes from scriptPubKey, for 'standard' transaction types.
 */
bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, vector<vector<unsigned char> >& vSolutionsRet)
{
    // Templates
    if (scriptPubKey.empty()){
        typeRet = TX_NULL_DATA;
        return true;
    }
        
    static multimap<txnouttype, CScript> mTemplates;
    if (mTemplates.empty())
    {
        // Standard tx, sender provides pubkey, receiver adds signature
        mTemplates.insert(make_pair(TX_PUBKEY, CScript() << OP_PUBKEY << OP_CHECKSIG));


        // Sender provides N pubkeys, receivers provides M signatures
        mTemplates.insert(make_pair(TX_MULTISIG, CScript() << OP_SMALLINTEGER << OP_PUBKEYS << OP_SMALLINTEGER << OP_CHECKMULTISIG));

        // Empty, provably prunable, data-carrying output
        if (GetBoolArg("-datacarrier", true))
            mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN << OP_SMALLDATA));
        mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN));
    }

    // Shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
    if (scriptPubKey.IsPayToScriptHash())
    {
        typeRet = TX_SCRIPTHASH;
        vector<unsigned char> hashBytes(scriptPubKey.begin()+2, scriptPubKey.begin()+22);
        vSolutionsRet.push_back(hashBytes);
        return true;
    }

    // Scan templates
    const CScript& script1 = scriptPubKey;
    BOOST_FOREACH(const PAIRTYPE(txnouttype, CScript)& tplate, mTemplates)
    {
        const CScript& script2 = tplate.second;
        vSolutionsRet.clear();

        opcodetype opcode1, opcode2;
        vector<unsigned char> vch1, vch2;

        // Compare
        CScript::const_iterator pc1 = script1.begin();
        CScript::const_iterator pc2 = script2.begin();
        unsigned int wTotal = 0;
        while (true)
        {
            if (pc1 == script1.end() && pc2 == script2.end())
            {
                // Found a match
                typeRet = tplate.first;
                if (typeRet == TX_MULTISIG)
                {
                    // Additional checks for TX_MULTISIG:
                    unsigned int n = CScriptNum(vSolutionsRet.back(),false).getint();
                    unsigned int wRequired = CScriptNum(vSolutionsRet.front(), false).getint();
                    if(wTotal < wRequired)
                        return false;
                    if(n != (vSolutionsRet.size() - 2) / 2)
                        return false;
                }
                return true;
            }
            if (!script1.GetOp(pc1, opcode1, vch1))
                break;
            if (!script2.GetOp(pc2, opcode2, vch2))
                break;
            // Template matching opcodes:
            if (opcode2 == OP_PUBKEYS)
            {
                bool finvalid = false;
                vector<vector<unsigned char> > vPubKeysRet;
                while (true)
                {
                    if (vch1.size() > 8 && vch1.size() <= 65) 
                    {
                        vSolutionsRet.push_back(vch1);
                        if (std::find(vPubKeysRet.begin(), vPubKeysRet.end(), vch1) != vPubKeysRet.end())
                        {
                            finvalid = true;
                            break;
                        }
                        vPubKeysRet.push_back(vch1);
                        if (!script1.GetOp(pc1, opcode1, vch1)) 
                        {
                            finvalid = true;
                            break;
                        }
                        int wPubkey = vch1.size() > 0 ? CScriptNum(vch1, false).getint() : CScriptNum((int) opcode1 - (int) (OP_1 - 1)).getint();
                        if ((wTotal + wPubkey) < wTotal) 
                        {
                            finvalid = true;
                            break;
                        } else
                            wTotal += wPubkey;
                        vSolutionsRet.push_back(vch1);
                        if (!script1.GetOp(pc1, opcode1, vch1))
                            finvalid = true;
                    } else {
                        break;
                    }
                }
                if(finvalid)
                        break;
                if (!script2.GetOp(pc2, opcode2, vch2))
                    break;
                // Normal situation is to fall through
                // to other if/else statements
            }

            if (opcode2 == OP_PUBKEY)
            {
                if (vch1.size() > 65)
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_PUBKEYHASH)
                break;
            else if (opcode2 == OP_SMALLINTEGER)
            {   // Single-byte small integer pushed onto vSolutions
                if (opcode1 == OP_0 ||
                    (opcode1 >= OP_1 && opcode1 <= OP_16))
                {
                    char n = (char)CScript::DecodeOP_N(opcode1);
                    vSolutionsRet.push_back(valtype(1, n));
                }
                else if (opcode1 < OP_PUSHDATA1)
                {
                    vSolutionsRet.push_back(vch1);
                }
                else
                    break;
            }
            else if (opcode2 == OP_SMALLDATA)
            {
                // small pushdata, <= nMaxDatacarrierBytes
                if (vch1.size() > nMaxDatacarrierBytes)
                    break;
            }
            else if (opcode1 != opcode2 || vch1 != vch2)
            {
                // Others must match exactly
                break;
            }
        }
    }

    vSolutionsRet.clear();
    typeRet = TX_NONSTANDARD;
    return false;
}

int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vSolutions)
{
    switch (t)
    {
        case TX_NONSTANDARD:
        case TX_NULL_DATA:
            return -1;
        case TX_PUBKEY:
            return 1;
        case TX_MULTISIG:
            if (vSolutions.size() < 1 || vSolutions[0].size() < 1)
                return -1;
            //multisig sigcount can't be evaluated
            return 1;
        case TX_SCRIPTHASH:
            return 1; // doesn't include args needed by the script
        case TX_SCRIPT:
            return 0;
    }
    return -1;
}

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType)
{
    vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;


    return whichType != TX_NONSTANDARD;
}

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet)
{
    vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY)
    {
        CPubKey pubKey(vSolutions[0]);
        if (!pubKey.IsValid())
            return false;

        addressRet = pubKey.GetID();
        return true;
    }
    else if (whichType == TX_SCRIPTHASH)
    {
        addressRet = CScriptID(uint160(vSolutions[0]));
        return true;
    }
    // Multisig txns have more than one address...
    return false;
}

bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, vector<CTxDestination>& addressRet, unsigned int& wRequiredRet)
{
    addressRet.clear();
    typeRet = TX_NONSTANDARD;
    vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, typeRet, vSolutions))
        return false;
    if (typeRet == TX_SCRIPT)
    {
        if (vSolutions.size() != 1)
            return false;
        CScript sc(vSolutions.front().begin(),vSolutions.front().end());
        if (!Solver(sc, typeRet, vSolutions))
            return false; 
    }
    if (typeRet == TX_NULL_DATA)
    {
        // This is data, not addresses
        return false;
    }

    if (typeRet == TX_MULTISIG)
    {
        wRequiredRet = CScriptNum(vSolutions.front(),false).getint();
        for (unsigned int i = 1; i < vSolutions.size()-1; i++)
        {
            CPubKey pubKey(vSolutions[i]);
            if (!pubKey.IsValid())
                continue;

            CTxDestination address = pubKey.GetID();
            addressRet.push_back(address);
        }

        if (addressRet.empty())
            return false;
    }
    else
    {
        wRequiredRet = 1;
        CTxDestination address;
        if (!ExtractDestination(scriptPubKey, address))
            return false;
        addressRet.push_back(address);
    }

    return true;
}

namespace
{
class CScriptVisitor : public boost::static_visitor<bool>
{
    private:
        CScript *script;
        int fType;
    public:
    CScriptVisitor(CScript *scriptin, int fTypein = 0) { script = scriptin; fType = fTypein;}

        bool operator()(const CNoDestination &dest) const {
            script->clear();
            return false;
        }

        bool operator()(const CKeyID &keyID) const {
            *script << keyID.ToByteVector();
            if(fType == 1)
                *script << OP_CHECKSIG;
            return true;
        }
        
        bool operator()(const CScriptID &scriptID) const {
            script->clear();
            *script << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
            return true;
        }
        
        bool operator()(const CScript &scriptIn) const {
            script->clear();
            *script += scriptIn;
            return true;
        }
    };
}

CScript GetScriptForDestination(const CTxDestination& dest)
{
    CScript script;
    boost::apply_visitor(CScriptVisitor(&script,1), dest);
    
    return script;
}

CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys)
{
    CScript script;

    script << CScript::EncodeOP_N(nRequired);
    BOOST_FOREACH(const CPubKey& key, keys)
    script << ToByteVector(key);
    script << CScript::EncodeOP_N(keys.size()) << OP_CHECKMULTISIG;
    return script;
}

CScript GetScriptForMultisigByWeight(const unsigned int wRequired, const std::vector<CTxDestination>& dests, const std::vector<unsigned int>& weights) 
{
    CScript script;
    script += GetScriptNum(wRequired);
    if (dests.size() != weights.size())
        return false;
    int i = 0;
    BOOST_FOREACH(const CTxDestination& dest, dests) {
        boost::apply_visitor(CScriptVisitor(&script), dest);
        script += GetScriptNum(weights[i]);
        i++;
    }
    script += GetScriptNum(i);
    script << OP_CHECKMULTISIG;
    return script;
}

CScript GetScriptNum(const unsigned int n)
{
       CScript script;
    if(n <=16)
        script <<CScript::EncodeOP_N(n);
    else
    {
        const CScriptNum is(n);
        script << is;
    }
    return script;
}
