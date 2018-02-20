// Copyright (c) 2015-2017 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assetallocation.h"
#include "alias.h"
#include "asset.h"
#include "init.h"
#include "validation.h"
#include "txmempool.h"
#include "util.h"
#include "random.h"
#include "base58.h"
#include "core_io.h"
#include "rpc/server.h"
#include "wallet/wallet.h"
#include "chainparams.h"
#include "coincontrol.h"
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <mongoc.h>
#include <chrono>

using namespace std::chrono;
using namespace std;
extern mongoc_collection_t *assetallocation_collection;
extern mongoc_collection_t *aliastxhistory_collection;
extern void SendMoneySyscoin(const vector<unsigned char> &vchAlias, const vector<unsigned char> &vchWitness, const CRecipient &aliasRecipient, CRecipient &aliasPaymentRecipient, vector<CRecipient> &vecSend, CWalletTx& wtxNew, CCoinControl* coinControl, bool fUseInstantSend = false, bool transferAlias = false);

bool IsAssetAllocationOp(int op) {
	return op == OP_ASSET_ALLOCATION_SEND || op == OP_ASSET_COLLECT_INTEREST;
}
string CAssetAllocationTuple::ToString() const {
	return stringFromVch(vchAsset) + "-" + stringFromVch(vchAlias);
}
string assetAllocationFromOp(int op) {
    switch (op) {
	case OP_ASSET_ALLOCATION_SEND:
		return "assetallocationsend";
	case OP_ASSET_COLLECT_INTEREST:
		return "assetallocationcollectinterest";
    default:
        return "<unknown assetallocation op>";
    }
}
bool CAssetAllocation::UnserializeFromData(const vector<unsigned char> &vchData, const vector<unsigned char> &vchHash) {
    try {
        CDataStream dsAsset(vchData, SER_NETWORK, PROTOCOL_VERSION);
        dsAsset >> *this;

		vector<unsigned char> vchAssetData;
		Serialize(vchAssetData);
		const uint256 &calculatedHash = Hash(vchAssetData.begin(), vchAssetData.end());
		const vector<unsigned char> &vchRandAsset = vchFromValue(calculatedHash.GetHex());
		if(vchRandAsset != vchHash)
		{
			SetNull();
			return false;
		}

    } catch (std::exception &e) {
		SetNull();
        return false;
    }
	return true;
}
bool CAssetAllocation::UnserializeFromTx(const CTransaction &tx) {
	vector<unsigned char> vchData;
	vector<unsigned char> vchHash;
	int nOut;
	if(!GetSyscoinData(tx, vchData, vchHash, nOut))
	{
		SetNull();
		return false;
	}
	if(!UnserializeFromData(vchData, vchHash))
	{	
		return false;
	}
    return true;
}
void CAssetAllocation::Serialize( vector<unsigned char> &vchData) {
    CDataStream dsAsset(SER_NETWORK, PROTOCOL_VERSION);
    dsAsset << *this;
	vchData = vector<unsigned char>(dsAsset.begin(), dsAsset.end());

}
void CAssetAllocationDB::WriteAssetAllocationIndex(const CAssetAllocation& assetallocation) {
	if (!assetallocation_collection)
		return;
	bson_error_t error;
	bson_t *update = NULL;
	bson_t *selector = NULL;
	mongoc_write_concern_t* write_concern = NULL;
	UniValue oName(UniValue::VOBJ);

	mongoc_update_flags_t update_flags;
	update_flags = (mongoc_update_flags_t)(MONGOC_UPDATE_NO_VALIDATE | MONGOC_UPDATE_UPSERT);
	selector = BCON_NEW("_id", BCON_UTF8(CAssetAllocationTuple(assetallocation.vchAsset, assetallocation.vchAlias).ToString().c_str()));
	write_concern = mongoc_write_concern_new();
	mongoc_write_concern_set_w(write_concern, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);
	if (BuildAssetAllocationIndexerJson(assetallocation, oName)) {
		update = bson_new_from_json((unsigned char *)oName.write().c_str(), -1, &error);
		if (!update || !mongoc_collection_update(assetallocation_collection, update_flags, selector, update, write_concern, &error)) {
			LogPrintf("MONGODB ASSET ALLOCATION UPDATE ERROR: %s\n", error.message);
		}
	}
	if (update)
		bson_destroy(update);
	if (selector)
		bson_destroy(selector);
	if (write_concern)
		mongoc_write_concern_destroy(write_concern);
}
void CAssetAllocationDB::EraseAssetAllocationIndex(const CAssetAllocationTuple& assetAllocationTuple, bool cleanup) {
	if (!assetallocation_collection)
		return;
	bson_error_t error;
	bson_t *selector = NULL;
	mongoc_write_concern_t* write_concern = NULL;
	mongoc_remove_flags_t remove_flags;
	remove_flags = (mongoc_remove_flags_t)(MONGOC_REMOVE_NONE);
	selector = BCON_NEW("_id", BCON_UTF8(assetAllocationTuple.ToString().c_str()));
	write_concern = mongoc_write_concern_new();
	mongoc_write_concern_set_w(write_concern, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);
	if (!mongoc_collection_remove(assetallocation_collection, remove_flags, selector, cleanup ? NULL : write_concern, &error)) {
		LogPrintf("MONGODB ASSET ALLOCATION REMOVE ERROR: %s\n", error.message);
	}
	if (selector)
		bson_destroy(selector);
	if (write_concern)
		mongoc_write_concern_destroy(write_concern);
}
bool GetAssetAllocation(const CAssetAllocationTuple &assetAllocationTuple,
        CAssetAllocation& txPos) {
    if (!passetallocationdb || !passetallocationdb->ReadAssetAllocation(assetAllocationTuple, txPos))
        return false;
    return true;
}
bool DecodeAndParseAssetAllocationTx(const CTransaction& tx, int& op, int& nOut,
		vector<vector<unsigned char> >& vvch, char &type)
{
	CAssetAllocation assetallocation;
	bool decode = DecodeAssetAllocationTx(tx, op, nOut, vvch);
	bool parse = assetallocation.UnserializeFromTx(tx);
	if (decode&&parse) {
		type = ASSETALLOCATION;
		return true;
	}
	return false;
}
bool DecodeAssetAllocationTx(const CTransaction& tx, int& op, int& nOut,
        vector<vector<unsigned char> >& vvch) {
	if (tx.nVersion != SYSCOIN_TX_VERSION)
		return false;
    bool found = false;


    // Strict check - bug disallowed
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& out = tx.vout[i];
        vector<vector<unsigned char> > vvchRead;
        if (DecodeAssetAllocationScript(out.scriptPubKey, op, vvchRead)) {
            nOut = i; found = true; vvch = vvchRead;
            break;
        }
    }
    if (!found) vvch.clear();
    return found;
}


bool DecodeAssetAllocationScript(const CScript& script, int& op,
        vector<vector<unsigned char> > &vvch, CScript::const_iterator& pc) {
    opcodetype opcode;
	vvch.clear();
    if (!script.GetOp(pc, opcode)) return false;
    if (opcode < OP_1 || opcode > OP_16) return false;
    op = CScript::DecodeOP_N(opcode);
	if (op != OP_SYSCOIN_ASSET_ALLOCATION)
		return false;
	if (!script.GetOp(pc, opcode))
		return false;
	if (opcode < OP_1 || opcode > OP_16)
		return false;
	op = CScript::DecodeOP_N(opcode);
	if (!IsAssetOp(op))
		return false;

	bool found = false;
	for (;;) {
		vector<unsigned char> vch;
		if (!script.GetOp(pc, opcode, vch))
			return false;
		if (opcode == OP_DROP || opcode == OP_2DROP)
		{
			found = true;
			break;
		}
		if (!(opcode >= 0 && opcode <= OP_PUSHDATA4))
			return false;
		vvch.push_back(vch);
	}

	// move the pc to after any DROP or NOP
	while (opcode == OP_DROP || opcode == OP_2DROP) {
		if (!script.GetOp(pc, opcode))
			break;
	}

	pc--;
	return found;
}
bool DecodeAssetAllocationScript(const CScript& script, int& op,
        vector<vector<unsigned char> > &vvch) {
    CScript::const_iterator pc = script.begin();
    return DecodeAssetAllocationScript(script, op, vvch, pc);
}
bool RemoveAssetAllocationScriptPrefix(const CScript& scriptIn, CScript& scriptOut) {
    int op;
    vector<vector<unsigned char> > vvch;
    CScript::const_iterator pc = scriptIn.begin();

    if (!DecodeAssetAllocationScript(scriptIn, op, vvch, pc))
		return false;
	scriptOut = CScript(pc, scriptIn.end());
	return true;
}
// revert allocation to previous state and remove 
bool RevertAssetAllocation(const CAssetAllocationTuple &assetAllocationToRemove, const uint256 &txHash, sorted_vector<CAssetAllocationTuple> &revertedAssetAllocations) {
	paliasdb->EraseAliasIndexTxHistory(txHash.GetHex() + "-" + assetAllocationToRemove.ToString());
	// only revert asset allocation once
	if (revertedAssetAllocations.find(assetAllocationToRemove) != revertedAssetAllocations.end())
		return true;

	string errorMessage = "";
	CAssetAllocation dbAssetAllocation;
	if (!passetallocationdb->ReadLastAssetAllocation(assetAllocationToRemove, dbAssetAllocation)) {
		dbAssetAllocation.SetNull();
		dbAssetAllocation.vchAlias = assetAllocationToRemove.vchAlias;
		dbAssetAllocation.vchAsset = assetAllocationToRemove.vchAsset;
	}
	LogPrintf("RevertAssetAllocations %s\n", assetAllocationToRemove.ToString().c_str());
	// write the state back to previous state
	if (!passetallocationdb->WriteAssetAllocation(dbAssetAllocation, INT64_MAX, false))
	{
		errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2022 - " + _("Failed to write to asset allocation DB");
		return error(errorMessage.c_str());
	}

	// remove the sender arrival time from this tx
	passetallocationdb->EraseISArrivalTime(assetAllocationToRemove, txHash);

	revertedAssetAllocations.insert(assetAllocationToRemove);

	// remove the conflict once we revert since it is assumed to be resolved on POW
	sorted_vector<CAssetAllocationTuple>::const_iterator it = assetAllocationConflicts.find(assetAllocationToRemove);
	if (it != assetAllocationConflicts.end()) {
		LogPrintf("RevertAssetAllocation: removing asset allocation conflict %s\n", assetAllocationToRemove.ToString().c_str());
		assetAllocationConflicts.V.erase(const_iterator_cast(assetAllocationConflicts.V, it));
	}

	return true;
	
}
// calculate annual interest on an asset allocation
CAmount GetAssetAllocationInterest(const CAsset& asset, CAssetAllocation & assetAllocation, const int64_t& nHeight) {
	const int64_t &nBlockDifference = nHeight - assetAllocation.nLastInterestClaimHeight;
	if (nBlockDifference < ONE_YEAR_IN_BLOCKS)
		return 0;
	// need to do one more average balance calculation since the last update to this asset allocation
	if (!AccumulateBalanceSinceLastInterestClaim(assetAllocation, nHeight))
		return 0;
	const float fYears = nBlockDifference / ONE_YEAR_IN_BLOCKS;
	// apply compound annual interest to get total interest since last time interest was collected
	const CAmount& nBalanceOverTimeDifference = assetAllocation.nAccumulatedBalanceSinceLastInterestClaim / nBlockDifference;
	// get interest only and apply externally to this function
	return ((nBalanceOverTimeDifference*pow((1 + (asset.fInterestRate)), fYears))) - nBalanceOverTimeDifference;
}
bool ApplyAssetAllocationInterest(const CAsset& asset, CAssetAllocation & assetAllocation, const int64_t& nHeight) {
	CAmount nInterest = GetAssetAllocationInterest(asset, assetAllocation, nHeight);
	if (nInterest <= 0)
		return false;
	// if interest cross max supply, reduce interest to fill up to max supply
	const CAmount &nMaxSupply = asset.nMaxSupply > 0 ? asset.nMaxSupply : MAX_ASSET;
	if ((nInterest + asset.nTotalSupply) > nMaxSupply) {
		nInterest = nMaxSupply - asset.nTotalSupply;
		if (nInterest <= 0)
			return false;
	}
	assetAllocation.nBalance += nInterest;
	assetAllocation.nLastInterestClaimHeight = nHeight;
	// average balance from 0 again since we have claimed
	assetAllocation.nAccumulatedBalanceSinceLastInterestClaim = 0;
	return true;
}
// keep track of average balance within the interest claim period
bool AccumulateBalanceSinceLastInterestClaim(CAssetAllocation & assetAllocation, const int64_t& nHeight) {
	const int64_t &nBlocksSinceLastUpdate = (nHeight - assetAllocation.nHeight);
	if (nBlocksSinceLastUpdate <= 0)
		return false;
	// formula is 1/N * (blocks since last update * previous balance) where N is the number of blocks in the total time period
	assetAllocation.nAccumulatedBalanceSinceLastInterestClaim += assetAllocation.nBalance*nBlocksSinceLastUpdate;
	return true;
}
bool CheckAssetAllocationInputs(const CTransaction &tx, int op, int nOut, const vector<vector<unsigned char> > &vvchArgs, const std::vector<unsigned char> &vchAlias,
        bool fJustCheck, int nHeight, sorted_vector<CAssetAllocationTuple> &revertedAssetAllocations, string &errorMessage, bool dontaddtodb) {
	if (!paliasdb || !passetallocationdb)
		return false;
	if (tx.IsCoinBase() && !fJustCheck && !dontaddtodb)
	{
		LogPrintf("*Trying to add assetallocation in coinbase transaction, skipping...");
		return true;
	}
	if (fDebug && !dontaddtodb)
		LogPrintf("*** ASSET ALLOCATION %d %d %s %s\n", nHeight,
			chainActive.Tip()->nHeight, tx.GetHash().ToString().c_str(),
			fJustCheck ? "JUSTCHECK" : "BLOCK");

	// unserialize assetallocation from txn, check for valid
	CAssetAllocation theAssetAllocation;
	vector<unsigned char> vchData;
	vector<unsigned char> vchHash;
	int nDataOut;
	if(!GetSyscoinData(tx, vchData, vchHash, nDataOut) || !theAssetAllocation.UnserializeFromData(vchData, vchHash))
	{
		errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR ERRCODE: 2001 - " + _("Cannot unserialize data inside of this transaction relating to a assetallocation");
		return true;
	}

	if(fJustCheck)
	{
		if(vvchArgs.size() != 1)
		{
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2002 - " + _("Asset arguments incorrect size");
			return error(errorMessage.c_str());
		}		
		if(vchHash != vvchArgs[0])
		{
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2003 - " + _("Hash provided doesn't match the calculated hash of the data");
			return true;
		}		
	}

	CAliasIndex alias;
	string retError = "";
	if(fJustCheck)
	{
		switch (op) {
		case OP_ASSET_ALLOCATION_SEND:
			if (theAssetAllocation.listSendingAllocationInputs.empty() && theAssetAllocation.listSendingAllocationAmounts.empty())
			{
				errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2021 - " + _("Asset send must send an input or transfer balance");
				return error(errorMessage.c_str());
			}
			if (theAssetAllocation.listSendingAllocationInputs.size() > 250 || theAssetAllocation.listSendingAllocationAmounts.size() > 250)
			{
				errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2021 - " + _("Too many receivers in one allocation send, maximum of 250 is allowed at once");
				return error(errorMessage.c_str());
			}
			if (theAssetAllocation.vchMemo.size() > MAX_MEMO_LENGTH)
			{
				errorMessage = "SYSCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2007 - " + _("memo too long, must be 128 character or less");
				return error(errorMessage.c_str());
			}
			break;
		case OP_ASSET_COLLECT_INTEREST:
			if (!theAssetAllocation.listSendingAllocationInputs.empty() || !theAssetAllocation.listSendingAllocationAmounts.empty())
			{
				errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2021 - " + _("Cannot send tokens in an interest collection transaction");
				return error(errorMessage.c_str());
			}
			if (theAssetAllocation.vchMemo.size() > 0)
			{
				errorMessage = "SYSCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2007 - " + _("Cannot send memo when collecting interest");
				return error(errorMessage.c_str());
			}
			break;
		default:
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2021 - " + _("Asset transaction has unknown op");
			return error(errorMessage.c_str());
		}
	}
	const CAssetAllocationTuple assetAllocationTuple(theAssetAllocation.vchAsset, vchAlias);
	const string &user3 = "";
	const string &user2 = "";
	const string &user1 = stringFromVch(vchAlias);
	string strResponseEnglish = "";
	string strResponse = GetSyscoinTransactionDescription(op, strResponseEnglish, ASSETALLOCATION);
	CAssetAllocation dbAssetAllocation;
	CAsset dbAsset;
	bool bRevert = false;
	bool bBalanceOverrun = false;
	bool bAddAllReceiversToConflictList = false;
	if (op == OP_ASSET_COLLECT_INTEREST)
	{
		if (!GetAssetAllocation(assetAllocationTuple, dbAssetAllocation))
		{
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2024 - " + _("Cannot find sender asset allocation");
			return true;
		}
		if (!GetAsset(dbAssetAllocation.vchAsset, dbAsset))
		{
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2022 - " + _("Failed to read from asset DB");
			return true;
		}
		if (dbAsset.fInterestRate <= 0)
		{
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2022 - " + _("Cannot collect interest on this asset, no interest rate has been defined");
			return true;
		}
		theAssetAllocation = dbAssetAllocation;
		// only apply interest on PoW
		if (!fJustCheck) {
			if(!ApplyAssetAllocationInterest(dbAsset, theAssetAllocation, nHeight))
			{
				errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2022 - " + _("You cannot collect interest on this asset. You must wait atleast 1 year to try to collect interest");
				return true;
			}
		}
		if(dontaddtodb)
			theAssetAllocation = dbAssetAllocation;
	}
	else if (op == OP_ASSET_ALLOCATION_SEND)
	{
		if (!dontaddtodb) {
			bRevert = !fJustCheck;
			if (bRevert) {
				if (!RevertAssetAllocation(assetAllocationTuple, tx.GetHash(), revertedAssetAllocations))
				{
					errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2028 - " + _("Failed to revert asset allocation");
					return error(errorMessage.c_str());
				}
			}
		}
		if (!GetAssetAllocation(assetAllocationTuple, dbAssetAllocation))
		{
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2024 - " + _("Cannot find sender asset allocation.");
			return true;
		}
		if (!GetAsset(dbAssetAllocation.vchAsset, dbAsset))
		{
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2022 - " + _("Failed to read from asset DB");
			return true;
		}
		theAssetAllocation.vchAlias = vchAlias;
		theAssetAllocation.nBalance = dbAssetAllocation.nBalance;
		// cannot modify interest claim height when sending
		theAssetAllocation.nLastInterestClaimHeight = dbAssetAllocation.nLastInterestClaimHeight;
		// get sender assetallocation
		// if no custom allocations are sent with request
			// if sender assetallocation has custom allocations, break as invalid assetsend request
			// ensure sender balance >= balance being sent
			// ensure balance being sent >= minimum divisible quantity
				// if minimum divisible quantity is 0, ensure the balance being sent is a while quantity
			// deduct balance from sender and add to receiver(s) in loop
		// if custom allocations are sent with index numbers in an array
			// loop through array of allocations that are sent along with request
				// get qty of allocation
				// get receiver assetallocation allocation if exists through receiver alias/assetallocation id tuple key
				// check the sender has the allocation in senders allocation list, remove from senders allocation list
				// add allocation to receivers allocation list
				// deduct qty from sender and add to receiver
				// commit receiver details to database using  through receiver alias/assetallocation id tuple as key
		// commit sender details to database
		if (dbAssetAllocation.vchAlias != vchAlias)
		{
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Cannot send this asset. Asset allocation owner must sign off on this change");
			return true;
		}
		if (!theAssetAllocation.listSendingAllocationAmounts.empty()) {
			if (dbAsset.bUseInputRanges) {
				errorMessage = "SYSCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2025 - " + _("Invalid asset send, request to send amounts but asset uses input ranges");
				return true;
			}
			// check balance is sufficient on sender
			CAmount nTotal = 0;
			for (auto& amountTuple : theAssetAllocation.listSendingAllocationAmounts) {
				const CAssetAllocationTuple receiverAllocationTuple(theAssetAllocation.vchAsset, amountTuple.first);
				// one of the first things we do per receiver is revert it to last pow state on the pow(!fJustCheck)
				if (bRevert) {
					if (!RevertAssetAllocation(receiverAllocationTuple, tx.GetHash(), revertedAssetAllocations))
					{
						errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2028 - " + _("Failed to revert asset allocation");
						return error(errorMessage.c_str());
					}
				}
				nTotal += amountTuple.second;
				if (amountTuple.second <= 0)
				{
					errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2025 - " + _("Receiving amount must be positive");
					return true;
				}
			}

			if (theAssetAllocation.nBalance < nTotal) {
				bBalanceOverrun = true;
				errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2025 - " + _("Sender balance is insufficient");
				if (fJustCheck && !dontaddtodb) {
					// add conflicting sender
					assetAllocationConflicts.insert(assetAllocationTuple);
					LogPrintf("CheckAssetAllocationInputs: balance overrun dbAssetAllocation.nBalance %llu vs nTotal %llu\n", dbAssetAllocation.nBalance, nTotal);
				}
			}
			else if (fJustCheck && !dontaddtodb) {
				// if sender was is flagged as conflicting, add all receivers to conflict list
				if (assetAllocationConflicts.find(assetAllocationTuple) != assetAllocationConflicts.end())
				{
					bAddAllReceiversToConflictList = true;
					LogPrintf("CheckAssetAllocationInputs: found conflict on %s\n", assetAllocationTuple.ToString().c_str());
				}
			}
			for (auto& amountTuple : theAssetAllocation.listSendingAllocationAmounts) {
				CAssetAllocation receiverAllocation;
				if (amountTuple.first == vchAlias) {
					errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2025 - " + _("Cannot send an asset allocation to yourself");
					return true;
				}
				const CAssetAllocationTuple receiverAllocationTuple(theAssetAllocation.vchAsset, amountTuple.first);
				if (fJustCheck) {
					if (bAddAllReceiversToConflictList || bBalanceOverrun) {
						LogPrintf("CheckAssetAllocationInputs: adding recver %s to conflict list\n", receiverAllocationTuple.ToString().c_str());
						assetAllocationConflicts.insert(receiverAllocationTuple);
					}

				}

				if (!dontaddtodb) {
					if (!GetAssetAllocation(receiverAllocationTuple, receiverAllocation)) {
						receiverAllocation.SetNull();
						receiverAllocation.vchAlias = receiverAllocationTuple.vchAlias;
						receiverAllocation.vchAsset = receiverAllocationTuple.vchAsset;
					}
					if (!bBalanceOverrun) {
						receiverAllocation.txHash = tx.GetHash();
						if (dbAsset.fInterestRate > 0) {
							// accumulate balances as sender/receiver allocations balances are adjusted
							if (receiverAllocation.nHeight > 0) {
								AccumulateBalanceSinceLastInterestClaim(receiverAllocation, nHeight);
							}
							AccumulateBalanceSinceLastInterestClaim(theAssetAllocation, nHeight);
						}
						receiverAllocation.nHeight = nHeight;
						receiverAllocation.vchMemo = theAssetAllocation.vchMemo;
						receiverAllocation.nBalance += amountTuple.second;
						theAssetAllocation.nBalance -= amountTuple.second;
					}

					if (!passetallocationdb->WriteAssetAllocation(receiverAllocation, INT64_MAX, fJustCheck))
					{
						errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2028 - " + _("Failed to write to asset allocation DB");
						return error(errorMessage.c_str());
					}

					if (fJustCheck) {
						if (strResponse != "") {
							paliasdb->WriteAliasIndexTxHistory(user1, stringFromVch(receiverAllocation.vchAlias), user3, tx.GetHash(), nHeight, strResponseEnglish, receiverAllocationTuple.ToString());
						}
					}
				}
			}
		}
		else if (!theAssetAllocation.listSendingAllocationInputs.empty()) {
			if (!dbAsset.bUseInputRanges) {
				errorMessage = "SYSCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2025 - " + _("Invalid asset send, request to send input ranges but asset uses amounts");
				return true;
			}
			// check balance is sufficient on sender
			CAmount nTotal = 0;
			vector<CAmount> rangeTotals;
			for (auto& inputTuple : theAssetAllocation.listSendingAllocationInputs) {
				const CAssetAllocationTuple receiverAllocationTuple(theAssetAllocation.vchAsset, inputTuple.first);
				// one of the first things we do per receiver is revert it to last pow state on the pow(!fJustCheck)
				if (bRevert) {
					if (!RevertAssetAllocation(receiverAllocationTuple, tx.GetHash(), revertedAssetAllocations))
					{
						errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2028 - " + _("Failed to revert asset allocation");
						return error(errorMessage.c_str());
					}
				}
				const unsigned int rangeTotal = validateRangesAndGetCount(inputTuple.second);
				if(rangeTotal == 0)
				{
					errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2025 - " + _("Invalid input ranges");
					return true;
				}
				const CAmount rangeTotalAmount = rangeTotal*COIN;
				rangeTotals.push_back(rangeTotalAmount);
				nTotal += rangeTotalAmount;
			}
			if (dbAssetAllocation.nBalance < nTotal) {
				bBalanceOverrun = true;
				errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2025 - " + _("Sender balance is insufficient");
				if (fJustCheck && !dontaddtodb) {
					// add conflicting sender
					assetAllocationConflicts.insert(assetAllocationTuple);
					LogPrintf("CheckAssetAllocationInputs: input balance overrun dbAssetAllocation.nBalance %llu vs nTotal %llu\n", dbAssetAllocation.nBalance, nTotal);
				}
			}
			else if (fJustCheck && !dontaddtodb) {
				// if sender was is flagged as conflicting, add all receivers to conflict list
				if (assetAllocationConflicts.find(assetAllocationTuple) != assetAllocationConflicts.end())
				{
					bAddAllReceiversToConflictList = true;
					LogPrintf("CheckAssetAllocationInputs: found input conflict on %s\n", assetAllocationTuple.ToString().c_str());
				}
			}
			for (unsigned int i = 0; i < theAssetAllocation.listSendingAllocationInputs.size();i++) {
				InputRanges &input = theAssetAllocation.listSendingAllocationInputs[i];
				CAssetAllocation receiverAllocation;
				if (input.first == vchAlias) {
					errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2025 - " + _("Cannot send an asset allocation to yourself");
					return true;
				}
				const CAssetAllocationTuple receiverAllocationTuple(theAssetAllocation.vchAsset, input.first);
				if (fJustCheck) {
					if (bAddAllReceiversToConflictList || bBalanceOverrun) {
						LogPrintf("CheckAssetAllocationInputs: adding recver %s to conflict list\n", receiverAllocationTuple.ToString().c_str());
						assetAllocationConflicts.insert(receiverAllocationTuple);
					}

				}
				// ensure entire allocation range being subtracted exists on sender (full inclusion check)
				if (!doesRangeContain(dbAssetAllocation.listAllocationInputs, input.second))
				{
					errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2025 - " + _("Input not found");
					return true;
				}
				if (!dontaddtodb) {
					if (!GetAssetAllocation(receiverAllocationTuple, receiverAllocation)) {
						receiverAllocation.SetNull();
						receiverAllocation.vchAlias = receiverAllocationTuple.vchAlias;
						receiverAllocation.vchAsset = receiverAllocationTuple.vchAsset;
					}
					if (!bBalanceOverrun) {
						receiverAllocation.txHash = tx.GetHash();
						if (dbAsset.fInterestRate > 0) {
							// accumulate balances as sender/receiver allocations balances are adjusted
							if (receiverAllocation.nHeight > 0) {
								AccumulateBalanceSinceLastInterestClaim(receiverAllocation, nHeight);
							}
							AccumulateBalanceSinceLastInterestClaim(theAssetAllocation, nHeight);
						}
						receiverAllocation.nHeight = nHeight;
						// figure out receivers added ranges and balance
						vector<CRange> outputMerge;
						receiverAllocation.listAllocationInputs.insert(std::end(receiverAllocation.listAllocationInputs), std::begin(input.second), std::end(input.second));
						mergeRanges(receiverAllocation.listAllocationInputs, outputMerge);
						receiverAllocation.listAllocationInputs = outputMerge;
						const CAmount prevBalance = receiverAllocation.nBalance;
						receiverAllocation.nBalance += rangeTotals[i];

						// figure out senders subtracted ranges and balance
						vector<CRange> outputSubtract;
						subtractRanges(dbAssetAllocation.listAllocationInputs, input.second, outputSubtract);
						theAssetAllocation.listAllocationInputs = outputSubtract;
						theAssetAllocation.nBalance -= rangeTotals[i];
					}

					if (!passetallocationdb->WriteAssetAllocation(receiverAllocation, INT64_MAX, fJustCheck))
					{
						errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2028 - " + _("Failed to write to asset allocation DB");
						return error(errorMessage.c_str());
					}

					if (fJustCheck) {
						if (strResponse != "") {
							paliasdb->WriteAliasIndexTxHistory(user1, stringFromVch(receiverAllocation.vchAlias), user3, tx.GetHash(), nHeight, strResponseEnglish, receiverAllocationTuple.ToString());
						}
					}
				}
			}
		}
	}

	// write assetallocation  
	// interest collection is only available on PoW
	if (!dontaddtodb && ((op == OP_ASSET_COLLECT_INTEREST && !fJustCheck) || (op != OP_ASSET_COLLECT_INTEREST))) {
		// set the assetallocation's txn-dependent 
		if (!bBalanceOverrun) {
			theAssetAllocation.nHeight = nHeight;
			theAssetAllocation.txHash = tx.GetHash();
		}

		int64_t ms = INT64_MAX;
		if (fJustCheck) {
			ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
		}

		if (!passetallocationdb->WriteAssetAllocation(theAssetAllocation, ms, fJustCheck))
		{
			errorMessage = "SYSCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 2028 - " + _("Failed to write to asset allocation DB");
			return error(errorMessage.c_str());
		}
		// debug
		if (fDebug)
			LogPrintf("CONNECTED ASSET ALLOCATION: op=%s assetallocation=%s hash=%s height=%d fJustCheck=%d at time %lld\n",
				assetFromOp(op).c_str(),
				assetAllocationTuple.ToString().c_str(),
				tx.GetHash().ToString().c_str(),
				nHeight,
				fJustCheck ? 1 : 0, (long long)ms);

	}
    return true;
}
UniValue assetallocationsend(const UniValue& params, bool fHelp) {
	if (fHelp || params.size() != 5)
		throw runtime_error(
			"assetallocationsend [asset] [aliasfrom] ( [{\"alias\":\"aliasname\",\"amount\":amount},...] or [{\"alias\":\"aliasname\",\"ranges\":[{\"start\":index,\"end\":index},...]},...] ) [memo] [witness]\n"
			"Send an asset allocation you own to another alias.\n"
			"<asset> Asset name.\n"
			"<aliasfrom> alias to transfer from.\n"
			"<memo> Message to include in this asset allocation transfer.\n"
			"<witness> Witness alias name that will sign for web-of-trust notarization of this transaction.\n"
			"The third parameter can be either an array of alias and amounts if sending amount pairs or an array of alias and array of start/end pairs of indexes for input ranges.\n"
			+ HelpRequiringPassphrase());

	// gather & validate inputs
	vector<unsigned char> vchAsset = vchFromValue(params[0]);
	vector<unsigned char> vchAliasFrom = vchFromValue(params[1]);
	UniValue valueTo = params[2];
	vector<unsigned char> vchMemo = vchFromValue(params[3]);
	vector<unsigned char> vchWitness = vchFromValue(params[4]);
	if (!valueTo.isArray())
		throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Array of receivers not found");

	CAliasIndex toAlias;
	CAssetAllocation theAssetAllocation;
	theAssetAllocation.vchAsset = vchAsset;
	theAssetAllocation.vchMemo = vchMemo;

	UniValue receivers = valueTo.get_array();
	for (unsigned int idx = 0; idx < receivers.size(); idx++) {
		const UniValue& receiver = receivers[idx];
		if (!receiver.isObject())
			throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected object with {\"alias'\",\"inputranges\" or \"amount\"}");

		UniValue receiverObj = receiver.get_obj();
		vector<unsigned char> vchAliasTo = vchFromValue(find_value(receiverObj, "alias"));
		if (!GetAlias(vchAliasTo, toAlias))
			throw runtime_error("SYSCOIN_ASSET_ALLOCATION_RPC_ERROR: ERRCODE: 2509 - " + _("Failed to read recipient alias from DB"));

		UniValue inputRangeObj = find_value(receiverObj, "ranges");
		UniValue amountObj = find_value(receiverObj, "amount");
		if (inputRangeObj.isArray()) {
			UniValue inputRanges = inputRangeObj.get_array();
			vector<CRange> vectorOfRanges;
			for (unsigned int rangeIndex = 0; rangeIndex < inputRanges.size(); rangeIndex++) {
				const UniValue& inputRangeObj = inputRanges[rangeIndex];
				if(!inputRangeObj.isObject())
					throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected object with {\"start'\",\"end\"}");
				UniValue startRangeObj = find_value(inputRangeObj, "start");
				UniValue endRangeObj = find_value(inputRangeObj, "end");
				if(!startRangeObj.isNum())
					throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "start range not found for an input");
				if(!endRangeObj.isNum())
					throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "end range not found for an input");
				vectorOfRanges.push_back(CRange(startRangeObj.get_int(), endRangeObj.get_int()));
			}
			theAssetAllocation.listSendingAllocationInputs.push_back(make_pair(vchAliasTo, vectorOfRanges));
		}
		else if (amountObj.isNum()) {
			const CAmount &amount = AmountFromValue(amountObj);
			if (amount < 0)
				throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "amount must be positive");
			theAssetAllocation.listSendingAllocationAmounts.push_back(make_pair(vchAliasTo, amount));
		}
		else
			throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected inputrange as string or amount as number in receiver array");

	}
	// check for alias existence in DB
	CAliasIndex fromAlias;
	if (!GetAlias(vchAliasFrom, fromAlias))
		throw runtime_error("SYSCOIN_ASSET_ALLOCATION_RPC_ERROR: ERRCODE: 2509 - " + _("Failed to read transfer alias from DB"));

	// this is a syscoin txn
	CWalletTx wtx;
	CScript scriptPubKeyFromOrig;

	CAsset theAsset;
	if (!GetAsset(vchAsset, theAsset))
		throw runtime_error("SYSCOIN_ASSET_ALLOCATION_RPC_ERROR: ERRCODE: 2510 - " + _("Could not find a asset with this key"));

	CSyscoinAddress fromAddr;
	GetAddress(fromAlias, &fromAddr, scriptPubKeyFromOrig);

	CScript scriptPubKey;

	CAssetAllocationTuple assetAllocationTuple(vchAsset, vchAliasFrom);
	if (!GetBoolArg("-unittest", false)) {
		// check to see if a transaction for this asset/alias tuple has arrived before minimum latency period
		ArrivalTimesMap arrivalTimes;
		passetallocationdb->ReadISArrivalTimes(assetAllocationTuple, arrivalTimes);
		const int64_t & nNow = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
		for (auto& arrivalTime : arrivalTimes) {
			// if this tx arrived within the minimum latency period flag it as potentially conflicting
			if ((nNow - (arrivalTime.second / 1000)) < ZDAG_MINIMUM_LATENCY_SECONDS) {
				throw runtime_error("SYSCOIN_ASSET_ALLOCATION_RPC_ERROR: ERRCODE: 2510 - " + _("Please wait a few more seconds and try again..."));
			}
		}
	}
	if (assetAllocationConflicts.find(assetAllocationTuple) != assetAllocationConflicts.end())
		throw runtime_error("SYSCOIN_ASSET_ALLOCATION_RPC_ERROR: ERRCODE: 2510 - " + _("This asset allocation is involved in a conflict which must be resolved with Proof-Of-Work. Please wait for a block confirmation and try again..."));
	
	vector<unsigned char> data;
	theAssetAllocation.Serialize(data);
	uint256 hash = Hash(data.begin(), data.end());

	vector<unsigned char> vchHashAsset = vchFromValue(hash.GetHex());
	scriptPubKey << CScript::EncodeOP_N(OP_SYSCOIN_ASSET_ALLOCATION) << CScript::EncodeOP_N(OP_ASSET_ALLOCATION_SEND) << vchHashAsset << OP_2DROP << OP_DROP;
	scriptPubKey += scriptPubKeyFromOrig;
	// send the asset pay txn
	vector<CRecipient> vecSend;
	CRecipient recipient;
	CreateRecipient(scriptPubKey, recipient);
	vecSend.push_back(recipient);

	CScript scriptPubKeyAlias;
	scriptPubKeyAlias << CScript::EncodeOP_N(OP_SYSCOIN_ALIAS) << CScript::EncodeOP_N(OP_ALIAS_UPDATE) << fromAlias.vchAlias << fromAlias.vchGUID << vchFromString("") << vchWitness << OP_2DROP << OP_2DROP << OP_2DROP;
	scriptPubKeyAlias += scriptPubKeyFromOrig;
	CRecipient aliasRecipient;
	CreateRecipient(scriptPubKeyAlias, aliasRecipient);
	CRecipient aliasPaymentRecipient;
	CreateAliasRecipient(scriptPubKeyFromOrig, aliasPaymentRecipient);

	CScript scriptData;
	scriptData << OP_RETURN << data;
	CRecipient fee;
	CreateFeeRecipient(scriptData, data, fee);
	vecSend.push_back(fee);


	CCoinControl coinControl;
	coinControl.fAllowOtherInputs = false;
	coinControl.fAllowWatchOnly = false;
	SendMoneySyscoin(fromAlias.vchAlias, vchWitness, aliasRecipient, aliasPaymentRecipient, vecSend, wtx, &coinControl);
	UniValue res(UniValue::VARR);
	res.push_back(EncodeHexTx(wtx));
	return res;
}
UniValue assetallocationcollectioninterest(const UniValue& params, bool fHelp) {
	if (fHelp || params.size() != 3)
		throw runtime_error(
			"assetallocationcollectioninterest [asset] [alias] [witness]\n"
			"Collect interest on this asset allocation if an interest rate is set on this asset.\n"
			"<asset> Asset name.\n"
			"<alias> alias which owns this asset allocation.\n"
			"<witness> Witness alias name that will sign for web-of-trust notarization of this transaction.\n"
			+ HelpRequiringPassphrase());

	// gather & validate inputs
	vector<unsigned char> vchAsset = vchFromValue(params[0]);
	vector<unsigned char> vchAliasFrom = vchFromValue(params[1]);
	vector<unsigned char> vchWitness;
	vchWitness = vchFromValue(params[2]);
	

	CAssetAllocation theAssetAllocation;
	theAssetAllocation.vchAsset = vchAsset;

	// check for alias existence in DB
	CAliasIndex fromAlias;
	if (!GetAlias(vchAliasFrom, fromAlias))
		throw runtime_error("SYSCOIN_ASSET_ALLOCATION_RPC_ERROR: ERRCODE: 2509 - " + _("Failed to read transfer alias from DB"));

	// this is a syscoin txn
	CWalletTx wtx;
	CScript scriptPubKeyFromOrig;

	CAsset theAsset;
	if (!GetAsset(vchAsset, theAsset))
		throw runtime_error("SYSCOIN_ASSET_ALLOCATION_RPC_ERROR: ERRCODE: 2510 - " + _("Could not find a asset with this key"));

	CSyscoinAddress fromAddr;
	GetAddress(fromAlias, &fromAddr, scriptPubKeyFromOrig);

	CScript scriptPubKey;

	vector<unsigned char> data;
	theAssetAllocation.Serialize(data);
	uint256 hash = Hash(data.begin(), data.end());

	vector<unsigned char> vchHashAsset = vchFromValue(hash.GetHex());
	scriptPubKey << CScript::EncodeOP_N(OP_SYSCOIN_ASSET_ALLOCATION) << CScript::EncodeOP_N(OP_ASSET_COLLECT_INTEREST) << vchHashAsset << OP_2DROP << OP_DROP;
	scriptPubKey += scriptPubKeyFromOrig;
	// send the asset pay txn
	vector<CRecipient> vecSend;
	CRecipient recipient;
	CreateRecipient(scriptPubKey, recipient);
	vecSend.push_back(recipient);

	CScript scriptPubKeyAlias;
	scriptPubKeyAlias << CScript::EncodeOP_N(OP_SYSCOIN_ALIAS) << CScript::EncodeOP_N(OP_ALIAS_UPDATE) << fromAlias.vchAlias << fromAlias.vchGUID << vchFromString("") << vchWitness << OP_2DROP << OP_2DROP << OP_2DROP;
	scriptPubKeyAlias += scriptPubKeyFromOrig;
	CRecipient aliasRecipient;
	CreateRecipient(scriptPubKeyAlias, aliasRecipient);
	CRecipient aliasPaymentRecipient;
	CreateAliasRecipient(scriptPubKeyFromOrig, aliasPaymentRecipient);

	CScript scriptData;
	scriptData << OP_RETURN << data;
	CRecipient fee;
	CreateFeeRecipient(scriptData, data, fee);
	vecSend.push_back(fee);


	CCoinControl coinControl;
	coinControl.fAllowOtherInputs = false;
	coinControl.fAllowWatchOnly = false;
	SendMoneySyscoin(fromAlias.vchAlias, vchWitness, aliasRecipient, aliasPaymentRecipient, vecSend, wtx, &coinControl);
	UniValue res(UniValue::VARR);
	res.push_back(EncodeHexTx(wtx));
	return res;
}
UniValue assetallocationinfo(const UniValue& params, bool fHelp) {
    if (fHelp || 3 != params.size())
        throw runtime_error("assetallocationinfo <asset> <alias> <getinputs>\n"
                "Show stored values of a single asset allocation. Set getinputs to true if you want to get the allocation inputs, if applicable.\n");

    vector<unsigned char> vchAsset = vchFromValue(params[0]);
	vector<unsigned char> vchAlias = vchFromValue(params[1]);
	bool bGetInputs = params[2].get_bool();
	UniValue oAssetAllocation(UniValue::VOBJ);

	CAssetAllocation txPos;
	if (!passetallocationdb || !passetallocationdb->ReadAssetAllocation(CAssetAllocationTuple(vchAsset, vchAlias), txPos))
		throw runtime_error("SYSCOIN_ASSET_ALLOCATION_RPC_ERROR: ERRCODE: 5536 - " + _("Failed to read from assetallocation DB"));

	if(!BuildAssetAllocationJson(txPos, bGetInputs, oAssetAllocation))
		oAssetAllocation.clear();
    return oAssetAllocation;
}
int DetectPotentialAssetAllocationSenderConflicts(const CAssetAllocationTuple& assetAllocationTupleSender, const CAssetAllocationTuple& assetAllocationTupleReceiver) {
	CAssetAllocation dbAssetAllocation;
	ArrivalTimesMap arrivalTimes;
	// get last POW asset allocation balance to ensure we use POW balance to check for potential conflicts in mempool (real-time balances).
	// The idea is that real-time spending amounts can in some cases overrun the POW balance safely whereas in some cases some of the spends are 
	// put in another block due to not using enough fees or for other reasons that miners don't mine them.
	// We just want to flag them as level 1 so it warrants deeper investigation on receiver side if desired (if fund amounts being transferred are not negligible)
	if (!passetallocationdb || !passetallocationdb->ReadLastAssetAllocation(assetAllocationTupleSender, dbAssetAllocation))
		return ZDAG_STATUS_OK;

	// ensure that this transaction exists in the arrivalTimes DB (which is the running stored lists of all real-time asset allocation sends not in POW)
	// the arrivalTimes DB is only added to for valid asset allocation sends that happen in real-time and it is removed once there is POW on that transaction
	passetallocationdb->ReadISArrivalTimes(assetAllocationTupleSender, arrivalTimes);
	
	// sort the arrivalTimesMap ascending based on arrival time value

	// Declaring the type of Predicate for comparing arrivalTimesMap
	typedef std::function<bool(std::pair<uint256, int64_t>, std::pair<uint256, int64_t>)> Comparator;

	// Defining a lambda function to compare two pairs. It will compare two pairs using second field
	Comparator compFunctor =
		[](std::pair<uint256, int64_t> elem1, std::pair<uint256, int64_t> elem2)
	{
		return elem1.second < elem2.second;
	};

	// Declaring a set that will store the pairs using above comparision logic
	std::set<std::pair<uint256, int64_t>, Comparator> arrivalTimesSet(
		arrivalTimes.begin(), arrivalTimes.end(), compFunctor);

	LOCK(cs_main);
	// go through arrival times and check that balances don't overrun the POW balance
	CAmount nRealtimeBalanceRequired = 0;
	pair<uint256, int64_t> lastArrivalTime;
	map<vector<unsigned char>, CAmount> mapBalances;
	// init sender balance, track balances by alias
	// this is important because asset allocations can be sent/received within blocks and will overrun balances prematurely if not tracked properly, for example pow balance 3, sender sends 3, gets 2 sends 2 (total send 3+2=5 > balance of 3 from last stored state, this is a valid scenario and shouldn't be flagged)
	CAmount &senderBalance = mapBalances[assetAllocationTupleSender.vchAlias];
	senderBalance = dbAssetAllocation.nBalance;
	for(auto& arrivalTime: arrivalTimesSet)
	{
		CTransaction tx;
		// ensure mempool has this transaction and it is not yet mined, get the transaction in question
		if (!mempool.lookup(arrivalTime.first, tx))
			continue;

		// if this tx arrived within the minimum latency period flag it as potentially conflicting
		if ((arrivalTime.second - lastArrivalTime.second) < (ZDAG_MINIMUM_LATENCY_SECONDS*1000)) {
			return ZDAG_MINOR_CONFLICT_OK;
		}
		lastArrivalTime = arrivalTime;
		// get asset allocation object from this tx, if for some reason it doesn't have it, just skip (shouldn't happen)
		CAssetAllocation assetallocation(tx);
		if (assetallocation.IsNull())
			continue;

		if (!assetallocation.listSendingAllocationAmounts.empty()) {
			for (auto& amountTuple : assetallocation.listSendingAllocationAmounts) {
				const CAssetAllocationTuple assetAllocation(assetAllocationTupleReceiver.vchAsset, amountTuple.first);
				senderBalance -= amountTuple.second;
				mapBalances[amountTuple.first] += amountTuple.second;
				// if running balance overruns the stored balance then we have a potential conflict
				if (senderBalance < 0) {
					return ZDAG_MINOR_CONFLICT_OK;
				}
				// even if the sender may be flagged, the order of events suggests that this receiver should get his money confirmed upon pow because real-time balance is sufficient for this receiver
				else if(assetAllocation == assetAllocationTupleReceiver) {
					return ZDAG_STATUS_OK;
				}
			}
		}
		else if (!assetallocation.listSendingAllocationInputs.empty()) {
			for (auto& inputTuple : assetallocation.listSendingAllocationInputs) {
				const CAssetAllocationTuple assetAllocation(assetAllocationTupleReceiver.vchAsset, inputTuple.first);
				const unsigned int rangeCount = validateRangesAndGetCount(inputTuple.second);
				if (rangeCount == 0)
					continue;
				senderBalance -= rangeCount*COIN;
				mapBalances[inputTuple.first] += rangeCount*COIN;
				// if running balance overruns the stored balance then we have a potential conflict
				if (senderBalance < 0) {
					return ZDAG_MINOR_CONFLICT_OK;
				}
				// even if the sender may be flagged, the order of events suggests that this receiver should get his money confirmed upon pow because real-time balance is sufficient for this receiver
				else if(assetAllocation == assetAllocationTupleReceiver) {
					return ZDAG_STATUS_OK;
				}
			}
		}
	}
	return ZDAG_STATUS_OK;
}
UniValue assetallocationsenderstatus(const UniValue& params, bool fHelp) {
	if (fHelp || 3 != params.size())
		throw runtime_error("assetallocationsenderstatus <asset> <sender> <receiver>\n"
			"Show status as it pertains to any current Z-DAG conflicts or warnings related to a sender or sender/receiver combination of an asset allocation transfer. Leave receiver empty if you are not checking for a specific sender/reciever transfer.\n"
			"Return value is in the status field and can represent 3 levels(0, 1 or 2)\n"
			"Level 0 means OK.\n"
			"Level 1 means warning (checked that in the mempool there are more spending balances than current POW sender balance). An active stance should be taken and perhaps a deeper analysis as to potential conflicts related to the sender.\n"
			"Level 2 means an active double spend was found and any depending asset allocation sends are also flagged as dangerous and should wait for POW confirmation before proceeding.\n");

	vector<unsigned char> vchAsset = vchFromValue(params[0]);
	vector<unsigned char> vchAliasSender = vchFromValue(params[1]);
	vector<unsigned char> vchAliasReceiver = vchFromValue(params[1]);
	UniValue oAssetAllocationStatus(UniValue::VOBJ);

	CAssetAllocationTuple assetAllocationTupleSender(vchAsset, vchAliasSender);
	CAssetAllocationTuple assetAllocationTupleReceiver(vchAsset, vchAliasReceiver);
	int nStatus = ZDAG_STATUS_OK;
	if (assetAllocationConflicts.find(assetAllocationTupleSender) != assetAllocationConflicts.end())
		nStatus = ZDAG_MAJOR_CONFLICT_OK;
	else {
		nStatus = DetectPotentialAssetAllocationSenderConflicts(assetAllocationTupleSender, assetAllocationTupleReceiver);
	}
	oAssetAllocationStatus.push_back(Pair("status", nStatus));
	return oAssetAllocationStatus;
}
bool BuildAssetAllocationJson(const CAssetAllocation& assetallocation, const bool bGetInputs, UniValue& oAssetAllocation)
{
	CAssetAllocationTuple assetAllocationTuple(assetallocation.vchAsset, assetallocation.vchAlias);
    oAssetAllocation.push_back(Pair("_id", assetAllocationTuple.ToString()));
	oAssetAllocation.push_back(Pair("asset", stringFromVch(assetallocation.vchAsset)));
    oAssetAllocation.push_back(Pair("txid", assetallocation.txHash.GetHex()));
    oAssetAllocation.push_back(Pair("height", (int)assetallocation.nHeight));
	oAssetAllocation.push_back(Pair("alias", stringFromVch(assetallocation.vchAlias)));
	oAssetAllocation.push_back(Pair("balance", ValueFromAmount(assetallocation.nBalance)));
	if (bGetInputs) {
		UniValue oAssetAllocationInputsArray(UniValue::VARR);
		for (auto& input : assetallocation.listAllocationInputs) {
			UniValue oAssetAllocationInputObj(UniValue::VOBJ);
			oAssetAllocationInputObj.push_back(Pair("start", (int)input.start));
			oAssetAllocationInputObj.push_back(Pair("end", (int)input.end));
			oAssetAllocationInputsArray.push_back(oAssetAllocationInputObj);
		}
		oAssetAllocation.push_back(Pair("inputs", oAssetAllocationInputsArray));
	}
	return true;
}
bool BuildAssetAllocationIndexerJson(const CAssetAllocation& assetallocation, UniValue& oAssetAllocation)
{
	oAssetAllocation.push_back(Pair("_id", CAssetAllocationTuple(assetallocation.vchAsset, assetallocation.vchAlias).ToString()));
	oAssetAllocation.push_back(Pair("txid", assetallocation.txHash.GetHex()));
	oAssetAllocation.push_back(Pair("asset", stringFromVch(assetallocation.vchAsset)));
	oAssetAllocation.push_back(Pair("height", (int)assetallocation.nHeight));
	oAssetAllocation.push_back(Pair("alias", stringFromVch(assetallocation.vchAlias)));
	oAssetAllocation.push_back(Pair("balance", ValueFromAmount(assetallocation.nBalance)));
	return true;
}
void AssetAllocationTxToJSON(const int op, const std::vector<unsigned char> &vchData, const std::vector<unsigned char> &vchHash, UniValue &entry)
{
	string opName = assetFromOp(op);
	CAssetAllocation assetallocation;
	if(!assetallocation.UnserializeFromData(vchData, vchHash))
		return;
	entry.push_back(Pair("txtype", opName));
	entry.push_back(Pair("_id", CAssetAllocationTuple(assetallocation.vchAsset, assetallocation.vchAlias).ToString()));
	entry.push_back(Pair("asset", stringFromVch(assetallocation.vchAsset)));
	entry.push_back(Pair("alias", stringFromVch(assetallocation.vchAlias)));
	UniValue oAssetAllocationReceiversArray(UniValue::VARR);
	if (!assetallocation.listSendingAllocationAmounts.empty()) {
		for (auto& amountTuple : assetallocation.listSendingAllocationAmounts) {
			UniValue oAssetAllocationReceiversObj(UniValue::VOBJ);
			oAssetAllocationReceiversObj.push_back(Pair("alias", stringFromVch(amountTuple.first)));
			oAssetAllocationReceiversObj.push_back(Pair("amount", ValueFromAmount(amountTuple.second)));
			oAssetAllocationReceiversArray.push_back(oAssetAllocationReceiversObj);
		}
		
	}
	else if (!assetallocation.listSendingAllocationInputs.empty()) {
		for (auto& inputTuple : assetallocation.listSendingAllocationInputs) {
			UniValue oAssetAllocationReceiversObj(UniValue::VOBJ);
			oAssetAllocationReceiversObj.push_back(Pair("alias", stringFromVch(inputTuple.first)));
			for (auto& inputRange : inputTuple.second) {
				oAssetAllocationReceiversObj.push_back(Pair("start", (int)inputRange.start));
				oAssetAllocationReceiversObj.push_back(Pair("end", (int)inputRange.end));
			}
			oAssetAllocationReceiversArray.push_back(oAssetAllocationReceiversObj);
		}
	}
	entry.push_back(Pair("allocations", oAssetAllocationReceiversArray));


}




