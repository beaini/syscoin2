// Copyright (c) 2016-2017 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_TEST_TEST_SYSCOIN_SERVICES_H
#define SYSCOIN_TEST_TEST_SYSCOIN_SERVICES_H

#include <stdio.h>
#include <univalue.h>
#include <map>
using namespace std;
static map<string, float> pegRates;
/** Testing syscoin services setup that configures a complete environment with 3 nodes.
 */
UniValue CallRPC(const string &dataDir, const string& commandWithArgs, bool regTest = true, bool readJson = true);
void StartNode(const string &dataDir, bool regTest = true, const string& extraArgs="");
void StopNode(const string &dataDir="node1");
void StartNodes();
void StartMainNetNodes();
void StopMainNetNodes();
void StopNodes();
void GenerateBlocks(int nBlocks, const string& node="node1");
void GenerateMainNetBlocks(int nBlocks, const string& node);
string CallExternal(string &cmd);
void SetSysMocktime(const int64_t& expiryTime);
void ExpireAlias(const string& alias);
void CheckRangeSubtract(const string& originalRanges, const string& subtractRanges, const string& expectedOutputRanges);
void CheckRangeMerge(const string& originalRanges, const string& newRanges, const string& expectedOutputRanges);
bool DoesRangeContain(const string& parentRange, const string& childRange);
void GetOtherNodes(const string& node, string& otherNode1, string& otherNode2);
string AliasNew(const string& node, const string& aliasname, const string& pubdata, string witness="''");
string AliasUpdate(const string& node, const string& aliasname, const string& pubdata="''", string addressStr = "''", string witness="''");
string AliasTransfer(const string& node, const string& aliasname, const string& tonode, const string& pubdata="''", const string& witness="''");
bool AliasFilter(const string& node, const string& regex);
UniValue AliasTxHistoryFilter(const string& node, const string& txid);

const string CertNew(const string& node, const string& alias, const string& title, const string& pubdata, const string& witness="''");
void CertUpdate(const string& node, const string& guid, const string& title="''", const string& pubdata="''", const string& witness="''");
void CertTransfer(const string& node, const string& tonode, const string& guid, const string& toalias, const string& witness="''");
bool CertFilter(const string& node, const string& regex);

// SYSCOIN testing setup
struct SyscoinTestingSetup {
    SyscoinTestingSetup();
    ~SyscoinTestingSetup();
};
struct BasicSyscoinTestingSetup {
    BasicSyscoinTestingSetup();
    ~BasicSyscoinTestingSetup();
};
struct SyscoinMainNetSetup {
	SyscoinMainNetSetup();
	~SyscoinMainNetSetup();
};
#endif
