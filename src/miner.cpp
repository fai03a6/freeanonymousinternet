// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#include "fai/mhash.h"
#include "amount.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "hash.h"
#include "main.h"
#include "net.h"
#include "pow.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "base58.h"
#include "utilstrencodings.h"
#include "core_io.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// The COrphan class keeps track of these 'temporary orphans' while
// CreateBlock is figuring out which transactions to include.
//

static uint64_t nPoolMiningResult=0;
static bool fPoolMiningFinished=false;
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256> setDependsOn;
    CFeeRate feeRate;
    double dPriority;

    COrphan(const CTransaction* ptxIn) : ptx(ptxIn), feeRate(0), dPriority(0)
    {
    }
};

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

// We want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, CFeeRate, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }

    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

void UpdateTime(CBlockHeader* pblock, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    // Updating time can change work required on testnet:
    if (Params().AllowMinDifficultyBlocks())
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
}

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn,const int nHeightIn)
{
    // Create new block
    auto_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience
    CBlockIndex* pindexPrev;
    if(nHeightIn<=0)
    {
        pindexPrev = chainActive.Tip();
        
    }
    else
        pindexPrev = chainActive[nHeightIn-1];
    int nHeight = pindexPrev->nBlockHeight + 1;
    pblock->nBlockHeight=nHeight;
    UpdateTime(pblock, pindexPrev);
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vin[0].prevout.n=nHeight;    
    txNew.vin[0].scriptSig=CScript()<<0;
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;    
    
    txNew.vout[0].nLockTime=nHeight +COINBASE_MATURITY;

    // Add dummy coinbase tx as first transaction
    pblock->vtx.push_back(CTransaction());
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);
    //min tx size to judge finish of block.set this a little bit higher so as to make mining faster
    unsigned int nMinTxSize=200;
    // Collect memory pool transactions into the block
    CAmount nFees = 0;
    uint64_t nBlockSize = 1000;
    uint64_t nBlockTx = 0;
    int nBlockSigOps = 100;
    {
        LOCK2(cs_main, mempool.cs);
    if(nHeightIn<=0)
    {
        CCoinsViewCache view(pcoinsTip);

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        // Collect transactions into block

        for (int i=0;i<(int)mempool.queue.size();i++)
        {
            const CTransaction& tx = mempool.mapTx[mempool.queue[i]].GetTx();
            if (tx.IsCoinBase() || !IsFinalTx(tx, nHeight))
                continue;

            COrphan* porphan = NULL;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                if (!view.HaveCoins(txin.prevout.hash))
                {
                    //don't take in transactions with prevout in mempool,because txs are queued by fee, not sequence, we can't guarantee it's
                    //previous tx can be included in this block
                    fMissingInputs = true;
                    break;
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }
                const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                assert(coins);
                     if ((int64_t)coins->vout[txin.prevout.n].nLockTime >= ((int64_t)coins->vout[txin.prevout.n].nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nHeight :  std::min((int64_t)pindexPrev->nTime,std::max((int64_t)pindexPrev->GetMedianTimePast()+1, (int64_t)GetAdjustedTime()))))
                         fMissingInputs = true;
                CAmount nValueIn = coins->vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

            }
            if (fMissingInputs) continue;
            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            if (!view.HaveInputs(tx))
                continue;
            CAmount nTxFees = tx.GetFee();
            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;
            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            if (!CheckInputs(tx, tx,state, view, pblock,true, MANDATORY_SCRIPT_VERIFY_FLAGS, true))
                continue;
            CTxUndo txundo;
            UpdateCoins(tx, state, view, txundo, nHeight);
            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;
            if (nBlockSize+nMinTxSize>nBlockMaxSize) 
                break;
        }
    }
    CBlock prevBlock;
    ReadBlockFromDisk(prevBlock, pindexPrev);
    CAmount prevCoinbaseFee=prevBlock.vtx[0].GetFee();
    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;

    // Compute final coinbase transaction.
    CAmount coinbaseInput=GetBlockValue(nHeight, nFees)+prevCoinbaseFee;        
    txNew.vin[0].prevout.nValue = coinbaseInput;        
    txNew.vout[0].nValue = 0;
    CAmount coinbaseFee=CFeeRate(DEFAULT_TRANSACTION_FEE).GetFee(txNew.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION)+10);
    CAmount coinbaseOutput=coinbaseInput-coinbaseFee;
    
    if(nHeightIn<=0&&coinbaseOutput<=minRelayTxFee.GetFee(DUST_THRESHOLD))
        return NULL;                     
    txNew.vout[0].nValue =coinbaseOutput;
    pblock->vtx[0] = txNew;
    pblocktemplate->vTxFees[0] = -nFees;

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock);
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

    CValidationState state;
        if (nHeightIn<=0&&!TestBlockValidity(state, *pblock, pindexPrev, false, false))
        {
            LogPrintf("CreateNewBlock() : TestBlockValidity failed \n" );
            return NULL;
        }
    }
    return pblocktemplate.release();
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    
    txCoinbase.vout[0].strContent = strprintf("%s",nExtraNonce);
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}
static boost::thread_group* minerThreads = NULL;
#ifdef ENABLE_WALLET
//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return NULL;

    CBitcoinAddress address;
    address.Set(pubkey.GetID());
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    return CreateNewBlock(scriptPubKey);
}

bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("FaicoinMiner : generated block is stale");
    }

    // Remove key from key pool
    reservekey.KeepKey();

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, NULL, pblock))
        return error("FaicoinMiner : ProcessNewBlock, block not accepted");

    return true;
}

void static BitcoinMiner(CWallet *pwallet)
{
    LogPrintf("FaicoinMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("faicoin-miner");

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);

    try {
        while (true) {
            if (Params().MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                while (vNodes.empty())
                    MilliSleep(1000);
            }

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey));
            
            if (!pblocktemplate.get())  {
                if (pwallet->setKeyPool.empty())
                {
                    LogPrintf("Error in FaicoinMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                    return;
                }
            //if no fee collected
                MilliSleep(10);
                continue;
            }
            CBlock *pblock = &pblocktemplate->block;
            
            pblock->hashMerkleRoot = pblock->BuildMerkleTree();
            unsigned int rounds=(unsigned int)int(16*sqrt((double)pblock->nBlockHeight));
            LogPrintf("Running FaicoinMiner with %u transactions in block (%u bytes),%u rounds mhash\n", pblock->vtx.size(),
                ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION),rounds);            
            
            //
            // Search
            //
            int64_t nStart = GetTime();
            uint256 hashTarget = uint256().SetCompact(pblock->nBits);
            uint256 thash;
            while (true) {
                unsigned int nHashesDone = 0;
                pblock->nNonce += 1;
                while(true)
                {
                    thash=pblock->GetHash();
                    mixHash(&thash,(unsigned int)pblock->nBlockHeight);
                    if (thash <= hashTarget)
                    {
                        // Found a solution
                        SetThreadPriority(THREAD_PRIORITY_NORMAL);
                        LogPrintf("FaicoinMiner:\n");
                        LogPrintf("proof-of-work found  \n  powhash: %s  \ntarget: %s\n", thash.GetHex(), hashTarget.GetHex());
                        ProcessBlockFound(pblock, *pwallet, reservekey);
                        SetThreadPriority(THREAD_PRIORITY_LOWEST);

                        // In regression test mode, stop mining after a block is found.
                        if (Params().MineBlocksOnDemand())
                            throw boost::thread_interrupted();

                        break;
                    }
                    pblock->nNonce += 1;
                    nHashesDone += 1;
                    if ((pblock->nNonce & 0xFF) == 0)
                        break;
                }

                // Meter hashes/sec
                static int64_t nHashCounter;
                if (nHPSTimerStart == 0)
                {
                    nHPSTimerStart = GetTimeMillis();
                    nHashCounter = 0;
                }
                else
                    nHashCounter += nHashesDone;
                if (GetTimeMillis() - nHPSTimerStart > 4000)
                {
                    static CCriticalSection cs;
                    {
                        LOCK(cs);
                        if (GetTimeMillis() - nHPSTimerStart > 4000)
                        {
                            dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                            nHPSTimerStart = GetTimeMillis();
                            nHashCounter = 0;
                            static int64_t nLogTime;
                            if (GetTime() - nLogTime > 30 * 60)
                            {
                                nLogTime = GetTime();
                                LogPrintf("hashmeter %6.0f khash/s\n", dHashesPerSec/1000.0);
                            }
                        }
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                if (vNodes.empty() && Params().MiningRequiresPeers())
                    break;
                if (pblock->nNonce >= 0xffff000000000000)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                // Update nTime every few seconds
                UpdateTime(pblock, pindexPrev);
                if (Params().AllowMinDifficultyBlocks())
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("FaicoinMiner terminated\n");
        throw;
    }
}

void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads)
{

    if (nThreads < 0) {
        // In regtest threads defaults to 1
        if (Params().DefaultMinerThreads())
            nThreads = Params().DefaultMinerThreads();
        else
            nThreads = boost::thread::hardware_concurrency();
    }

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        fPoolMiningFinished=true;
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&BitcoinMiner, pwallet));
}

#endif // ENABLE_WALLET


void PoolMiningThread(CBlockHeader& block,uint64_t nNonceBegin,uint64_t nNonceEnd,uint32_t nbit)
{
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("faicoin-poolminer");
    CBlockHeader block1=block;
    block1.nNonce=nNonceBegin;
    

    try {
        
            
        
            
            //
            // Search
            //
            int64_t nStart = GetTime();

            uint256 hashTarget = uint256().SetCompact(nbit==0?block1.nBits:nbit);
    
            uint256 thash;
            while (true) {
                unsigned int nHashesDone = 0;
                while(true)
                {
                    thash=block1.GetHash();
                    
                    mixHash(&thash,(unsigned int)block1.nBlockHeight);
           
                    if (thash <= hashTarget)
                    {
                        // Found a solution
                        SetThreadPriority(THREAD_PRIORITY_NORMAL);
         
                        LogPrintf("proof-of-work found  \n  powhash: %s  \ntarget: %s\n", thash.GetHex(), hashTarget.GetHex());
      
                        SetThreadPriority(THREAD_PRIORITY_LOWEST);
                        static CCriticalSection cs;
                        {
                            LOCK(cs);     
                            nPoolMiningResult=block1.nNonce ;
                        }
                            throw boost::thread_interrupted();
                    }
                    block1.nNonce += 1;
                    nHashesDone += 1;
                    if ((block1.nNonce & 0xFF) == 0)
                        break;
                }

                // Meter hashes/sec
                static int64_t nHashCounter;
                if (nHPSTimerStart == 0)
                {
                    nHPSTimerStart = GetTimeMillis();
                    nHashCounter = 0;
                }
                else
                    nHashCounter += nHashesDone;
                if (GetTimeMillis() - nHPSTimerStart > 4000)
                {
                    static CCriticalSection cs;
                    {
                        LOCK(cs);
                        if (GetTimeMillis() - nHPSTimerStart > 4000)
                        {
                            dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                            nHPSTimerStart = GetTimeMillis();
                            nHashCounter = 0;
                            static int64_t nLogTime;
                            if (GetTime() - nLogTime > 30 * 60)
                            {
                                nLogTime = GetTime();
                                LogPrintf("hashmeter %6.3f khash/s\n", dHashesPerSec/1000.0);
                            }
                        }
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                
                if (block1.nNonce > nNonceEnd||(GetTime()-nStart)>60)
                {
                    static CCriticalSection cs;
                    {
                        LOCK(cs);
                        fPoolMiningFinished=true;
   
                    }
                 throw boost::thread_interrupted();
                }
                
            }
        
    }
    catch (boost::thread_interrupted)
    {

        throw;
    }
}
uint64_t PoolMiner(bool fGenerate,CBlockHeader block,uint64_t nNonceBegin,uint64_t nNonceEnd,int nThreads,uint32_t nbit)
{
    
    if (nThreads < 0) {
        // In regtest threads defaults to 1
        if (Params().DefaultMinerThreads())
            nThreads = Params().DefaultMinerThreads();
        else
            nThreads = boost::thread::hardware_concurrency();
    }
  
    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return 0;

    minerThreads = new boost::thread_group();
    uint64_t step=(uint64_t)((nNonceEnd-nNonceBegin)/nThreads);
 
    fPoolMiningFinished=false;
    nPoolMiningResult=0;
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&PoolMiningThread, block,nNonceBegin+step*i,nNonceBegin+step*(i+1),nbit));
    
    while(true)
    {
        MilliSleep(10);
        if(nPoolMiningResult>0)
        {

            minerThreads->interrupt_all();
            delete minerThreads;
            minerThreads = NULL;
            return nPoolMiningResult;
        }

        if(fPoolMiningFinished)
        {
            

            minerThreads->interrupt_all();
            delete minerThreads;
            minerThreads = NULL;
            return 0;
        }
    }  
    
}
