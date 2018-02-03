// Copyright (c) 2015-2017 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ALIAS_H
#define ALIAS_H

#include "rpc/server.h"
#include "dbwrapper.h"
#include "consensus/params.h"
#include "sync.h"
#include "script/script.h"
#include "serialize.h"
class CWalletTx;
class CTransaction;
class CTxOut;
class COutPoint;
class CReserveKey;
class CCoinsViewCache;
class CCoins;
class CBlock;
class CSyscoinAddress;
class COutPoint;
class CCoinControl;
struct CRecipient;
static const unsigned int MAX_GUID_LENGTH = 71;
static const unsigned int MAX_NAME_LENGTH = 256;
static const unsigned int MAX_VALUE_LENGTH = 512;
static const unsigned int MAX_ID_LENGTH = 20;
static const unsigned int MAX_ENCRYPTED_GUID_LENGTH = MAX_GUID_LENGTH + 85;
static const uint64_t ONE_YEAR_IN_SECONDS = 31536000;
enum {
	ALIAS=0,
	ASSET,
	ASSETALLOCATION
};
enum {
	ACCEPT_TRANSFER_NONE=0,
	ACCEPT_TRANSFER_ALL,
};
class CAssetDB;
class CAssetAllocationDB;
extern CAssetDB *passetdb;
extern CAssetAllocationDB *passetallocationdb;

std::string stringFromVch(const std::vector<unsigned char> &vch);
std::vector<unsigned char> vchFromValue(const UniValue& value);
std::vector<unsigned char> vchFromString(const std::string &str);
std::string stringFromValue(const UniValue& value);
const int SYSCOIN_TX_VERSION = 0x7400;
void CreateRecipient(const CScript& scriptPubKey, CRecipient& recipient);
void CreateFeeRecipient(CScript& scriptPubKey, const std::vector<unsigned char>& data, CRecipient& recipient);
int assetselectpaymentcoins(const std::vector<unsigned char> &vchAsset, const CAmount &nAmount, std::vector<COutPoint>& outPoints, bool& bIsFunded, CAmount &nRequiredAmount, bool bSelectFeePlacement, bool bSelectAll=false, bool bNoAssetRecipient=false);
CAmount GetDataFee(const CScript& scriptPubKey);
bool DecodeAndParseSyscoinTx(const CTransaction& tx, int& op, int& nOut, std::vector<std::vector<unsigned char> >& vvch, char &type);
std::string GenerateSyscoinGuid();
int GetSyscoinDataOutput(const CTransaction& tx);
bool IsSyscoinDataOutput(const CTxOut& out);
bool GetSyscoinData(const CTransaction &tx, std::vector<unsigned char> &vchData, std::vector<unsigned char> &vchHash, int& nOut);
bool GetSyscoinData(const CScript &scriptPubKey, std::vector<unsigned char> &vchData, std::vector<unsigned char> &vchHash);
bool IsSysServiceExpired(const uint64_t &nTime);
bool GetTimeToPrune(const CScript& scriptPubKey, uint64_t &nTime);
bool GetSyscoinTransaction(int nHeight, const uint256 &hash, CTransaction &txOut, const Consensus::Params& consensusParams);
bool GetSyscoinTransaction(int nHeight, const uint256 &hash, CTransaction &txOut, uint256& hashBlock, const Consensus::Params& consensusParams);
bool IsSyscoinScript(const CScript& scriptPubKey, int &op, std::vector<std::vector<unsigned char> > &vvchArgs);
bool RemoveSyscoinScript(const CScript& scriptPubKeyIn, CScript& scriptPubKeyOut);
void SysTxToJSON(const int op, const std::vector<unsigned char> &vchData, const std::vector<unsigned char> &vchHash, UniValue &entry, const char& type);
void CleanupSyscoinServiceDatabases(int &servicesCleaned);
int assetunspent(const std::vector<unsigned char> &vchAsset, COutPoint& outPoint);
void startMongoDB();
void stopMongoDB();
std::string GetSyscoinTransactionDescription(const int op, std::string& responseEnglish, const char &type);
#endif // ALIAS_H
