// Copyright (c) 2016-2017 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test_syscoin_services.h"
#include "utiltime.h"
#include "util.h"
#include "amount.h"
#include "rpc/server.h"
#include "asset.h"
#include "assetallocation.h"
#include "alias.h"
#include "wallet/crypter.h"
#include "random.h"
#include "base58.h"
#include "chainparams.h"
#include "core_io.h"
#include <memory>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include "ranges.h"
static int node1LastBlock=0;
static int node2LastBlock=0;
static int node3LastBlock=0;
static int node4LastBlock=0;
static bool node1Online = false;
static bool node2Online = false;
static bool node3Online = false;
// SYSCOIN testing setup
void StartNodes()
{
	printf("Stopping any test nodes that are running...\n");
	StopNodes();
	node1LastBlock=0;
	node2LastBlock=0;
	node3LastBlock=0;
	node4LastBlock=0;
	if(boost::filesystem::exists(boost::filesystem::system_complete("node1/wallet.dat")))
		boost::filesystem::remove(boost::filesystem::system_complete("node1//wallet.dat"));
	if(boost::filesystem::exists(boost::filesystem::system_complete("node2/wallet.dat")))
		boost::filesystem::remove(boost::filesystem::system_complete("node2//wallet.dat"));
	if(boost::filesystem::exists(boost::filesystem::system_complete("node3/wallet.dat")))
		boost::filesystem::remove(boost::filesystem::system_complete("node3//wallet.dat"));
	if(boost::filesystem::exists(boost::filesystem::system_complete("node4/wallet.dat")))
		boost::filesystem::remove(boost::filesystem::system_complete("node4//wallet.dat"));
	StopMainNetNodes();
	printf("Starting 4 nodes in a regtest setup...\n");
	StartNode("node1");
	StartNode("node2");
	StartNode("node3");
	StartNode("node4", true, "-txindex");
	StopNode("node4");
	StartNode("node4", true, "-txindex");
	SelectParams(CBaseChainParams::REGTEST);

}
void StartMainNetNodes()
{
	StopMainNetNodes();
	printf("Starting 1 node in mainnet setup...\n");
	StartNode("mainnet1", false);
}
void StopMainNetNodes()
{
	printf("Stopping mainnet1..\n");
	try{
		CallRPC("mainnet1", "stop");
	}
	catch(const runtime_error& error)
	{
	}	
	printf("Done!\n");
}
void StopNodes()
{
	StopNode("node1");
	StopNode("node2");
	StopNode("node3");
	StopNode("node4");
	printf("Done!\n");
}
void StartNode(const string &dataDir, bool regTest, const string& extraArgs)
{
	if(boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/wallet.dat")))
	{
		if (!boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/regtest")))
			boost::filesystem::create_directory(boost::filesystem::system_complete(dataDir + "/regtest"));
		boost::filesystem::copy_file(boost::filesystem::system_complete(dataDir + "/wallet.dat"),boost::filesystem::system_complete(dataDir + "/regtest/wallet.dat"),boost::filesystem::copy_option::overwrite_if_exists);
		boost::filesystem::remove(boost::filesystem::system_complete(dataDir + "/wallet.dat"));
	}
    boost::filesystem::path fpath = boost::filesystem::system_complete("../syscoind");
	string nodePath = fpath.string() + string(" -datadir=") + dataDir;
	if(regTest)
		nodePath += string(" -regtest -debug -addressindex -unittest");
	if(!extraArgs.empty())
		nodePath += string(" ") + extraArgs;
	if (!boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/db")))
		boost::filesystem::create_directory(boost::filesystem::system_complete(dataDir + "/db"));
	if (dataDir == "node1") {
		printf("Launching mongod on port 27017...\n");
		string path = string("mongod --port=27017 --quiet --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
		path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
		boost::thread t(runCommand, path);
	}
	else if (dataDir == "node2") {
		printf("Launching mongod on port 27018...\n");
		string path = string("mongod --port=27018 --quiet --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
		path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
		boost::thread t(runCommand, path);
	}
	else if (dataDir == "node3") {
		printf("Launching mongod on port 27019...\n");
		string path = string("mongod --port=27019 --quiet --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
		path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
		boost::thread t(runCommand, path);
	}
    boost::thread t(runCommand, nodePath);
	printf("Launching %s, waiting 1 second before trying to ping...\n", nodePath.c_str());
	MilliSleep(1000);
	UniValue r;
	while (1)
	{
		try{
			printf("Calling getinfo!\n");
			r = CallRPC(dataDir, "getinfo", regTest);
			if(dataDir == "node1")
			{
				if(node1LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node1LastBlock);
					MilliSleep(500);
					continue;
				}
				node1Online = true;
				node1LastBlock = 0;
			}
			else if(dataDir == "node2")
			{
				if(node2LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node2LastBlock);
					MilliSleep(500);
					continue;
				}
				node2Online = true;
				node2LastBlock = 0;
			}
			else if(dataDir == "node3")
			{
				if(node3LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node3LastBlock);
					MilliSleep(500);
					continue;
				}
				node3Online = true;
				node3LastBlock = 0;
			}
			else if(dataDir == "node4")
			{
				if(node4LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node4LastBlock);
					MilliSleep(500);
					continue;
				}
				node4LastBlock = 0;
			}
			MilliSleep(500);
			CallRPC(dataDir, "prunesyscoinservices", regTest);
			MilliSleep(500);
		}
		catch(const runtime_error& error)
		{
			printf("Waiting for %s to come online, trying again in 1 second...\n", dataDir.c_str());
			MilliSleep(1000);
			continue;
		}
		break;
	}
	printf("Done!\n");
}

void StopNode (const string &dataDir) {
	printf("Stopping %s..\n", dataDir.c_str());
	UniValue r;
	try{
		r = CallRPC(dataDir, "getinfo");
		if(r.isObject())
		{
			if(dataDir == "node1")
				node1LastBlock = find_value(r.get_obj(), "blocks").get_int();
			else if(dataDir == "node2")
				node2LastBlock = find_value(r.get_obj(), "blocks").get_int();
			else if(dataDir == "node3")
				node3LastBlock = find_value(r.get_obj(), "blocks").get_int();
			else if(dataDir == "node4")
				node4LastBlock = find_value(r.get_obj(), "blocks").get_int();
		}
	}
	catch(const runtime_error& error)
	{
	}
	try{
		CallRPC(dataDir, "stop");
	}
	catch(const runtime_error& error)
	{
	}
	while (1)
	{
		try {
			MilliSleep(1000);
			CallRPC(dataDir, "getinfo");
		}
		catch (const runtime_error& error)
		{
			break;
		}
	}
	try {
		if (dataDir == "node1" && node1Online) {
			printf("Stopping mongod on port 27017...\n");
			string path = string("mongod --port=27017 --quiet --shutdown --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
			path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
			boost::thread t(runCommand, path);
		}
		else if (dataDir == "node2" && node2Online) {
			printf("Stopping mongod on port 27018...\n");
			string path = string("mongod --port=27018 --quiet --shutdown --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
			path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
			boost::thread t(runCommand, path);
		}
		else if (dataDir == "node3" && node3Online) {
			printf("Stopping mongod on port 27019...\n");
			string path = string("mongod --port=27019 --quiet --shutdown --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
			path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
			boost::thread t(runCommand, path);
		}
		if (dataDir == "node1")
			node1Online = false;
		else if (dataDir == "node2")
			node2Online = false;
		else if (dataDir == "node3")
			node3Online = false;
	}
	catch (const runtime_error& error)
	{
		
	}
	MilliSleep(1000);
	if(boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/regtest/wallet.dat")))
		boost::filesystem::copy_file(boost::filesystem::system_complete(dataDir + "/regtest/wallet.dat"),boost::filesystem::system_complete(dataDir + "/wallet.dat"),boost::filesystem::copy_option::overwrite_if_exists);
	if(boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/regtest")))
		boost::filesystem::remove_all(boost::filesystem::system_complete(dataDir + "/regtest"));
	try {
		if (boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/db")))
			boost::filesystem::remove_all(boost::filesystem::system_complete(dataDir + "/db"));
	}
	catch (...) {
	}
}

UniValue CallRPC(const string &dataDir, const string& commandWithArgs, bool regTest, bool readJson)
{
	UniValue val;
	boost::filesystem::path fpath = boost::filesystem::system_complete("../syscoin-cli");
	string path = fpath.string() + string(" -datadir=") + dataDir;
	if(regTest)
		path += string(" -regtest ");
	else
		path += " ";
	path += commandWithArgs;
	string rawJson = CallExternal(path);
	if(readJson)
	{
		val.read(rawJson);
		if(val.isNull())
			throw runtime_error("Could not parse rpc results");
	}
	return val;
}
int fsize(FILE *fp){
    int prev=ftell(fp);
    fseek(fp, 0L, SEEK_END);
    int sz=ftell(fp);
    fseek(fp,prev,SEEK_SET); //go back to where we were
    return sz;
}
void safe_fclose(FILE* file)
{
      if (file)
         BOOST_VERIFY(0 == fclose(file));
	if(boost::filesystem::exists("cmdoutput.log"))
		boost::filesystem::remove("cmdoutput.log");
}
int runSysCommand(const std::string& strCommand)
{
    int nErr = ::system(strCommand.c_str());
	return nErr;
}
std::string CallExternal(std::string &cmd)
{
	cmd += " > cmdoutput.log || true";
	if(runSysCommand(cmd))
		return string("ERROR");
    boost::shared_ptr<FILE> pipe(fopen("cmdoutput.log", "r"), safe_fclose);
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
	if(fsize(pipe.get()) > 0)
	{
		while (!feof(pipe.get())) {
			if (fgets(buffer, 128, pipe.get()) != NULL)
				result += buffer;
		}
	}
    return result;
}
void GenerateMainNetBlocks(int nBlocks, const string& node)
{
	int targetHeight, newHeight;
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "getinfo"));
	targetHeight = find_value(r.get_obj(), "blocks").get_int() + nBlocks;
	newHeight = 0;
	const string &sBlocks = strprintf("%d",nBlocks);

	while(newHeight < targetHeight)
	{
	  BOOST_CHECK_NO_THROW(r = CallRPC(node, "generate " + sBlocks));
	  MilliSleep(1000);
	  BOOST_CHECK_NO_THROW(r = CallRPC(node, "getinfo"));
	  newHeight = find_value(r.get_obj(), "blocks").get_int();
	  BOOST_CHECK_NO_THROW(r = CallRPC(node, "getinfo"));
	  CAmount balance = AmountFromValue(find_value(r.get_obj(), "balance"));
	  printf("Current block height %d, Target block height %d, balance %f\n", newHeight, targetHeight, ValueFromAmount(balance).get_real()); 
	}
	BOOST_CHECK(newHeight >= targetHeight);
}
// generate n Blocks, with up to 10 seconds relay time buffer for other nodes to get the blocks.
// may fail if your network is slow or you try to generate too many blocks such that can't relay within 10 seconds
void GenerateBlocks(int nBlocks, const string& node)
{
  int height, newHeight, timeoutCounter;
  UniValue r;
  string otherNode1, otherNode2;
  GetOtherNodes(node, otherNode1, otherNode2);
  try
  {
	r = CallRPC(node, "getinfo");
  }
  catch(const runtime_error &e)
  {
	return;
  }
  newHeight = find_value(r.get_obj(), "blocks").get_int() + nBlocks;
  const string &sBlocks = strprintf("%d",nBlocks);
  BOOST_CHECK_NO_THROW(r = CallRPC(node, "generate " + sBlocks));
  BOOST_CHECK_NO_THROW(r = CallRPC(node, "getinfo"));
  height = find_value(r.get_obj(), "blocks").get_int();
  BOOST_CHECK(height >= newHeight);
  height = 0;
  timeoutCounter = 0;
  MilliSleep(10);
  while(!otherNode1.empty() && height < newHeight)
  {
	  try
	  {
		r = CallRPC(otherNode1, "getinfo");
	  }
	  catch(const runtime_error &e)
	  {
		r = NullUniValue;
	  }
	  if(!r.isObject())
	  {
		 height = newHeight;
		 break;
	  }
	  height = find_value(r.get_obj(), "blocks").get_int();
	  timeoutCounter++;
	  if (timeoutCounter > 100) {
		  printf("Error: Timeout on getinfo for %s, height %d vs newHeight %d!\n", otherNode1.c_str(), height, newHeight);
		  break;
	  }
	  MilliSleep(10);
  }
  if(!otherNode1.empty())
	BOOST_CHECK(height >= newHeight);
  height = 0;
  timeoutCounter = 0;
  while(!otherNode2.empty() &&height < newHeight)
  {
	  try
	  {
		r = CallRPC(otherNode2, "getinfo");
	  }
	  catch(const runtime_error &e)
	  {
		r = NullUniValue;
	  }
	  if(!r.isObject())
	  {
		 height = newHeight;
		 break;
	  }
	  height = find_value(r.get_obj(), "blocks").get_int();
	  timeoutCounter++;
	  if (timeoutCounter > 100) {
		printf("Error: Timeout on getinfo for %s, height %d vs newHeight %d!\n", otherNode2.c_str(), height, newHeight);
		break;
	  }
	  MilliSleep(10);
  }
  if(!otherNode2.empty())
	BOOST_CHECK(height >= newHeight);
  height = 0;
  timeoutCounter = 0;
}
void SetSysMocktime(const int64_t& expiryTime) {
	BOOST_CHECK(expiryTime > 0);
	string cmd = strprintf("setmocktime %lld", expiryTime);
	try
	{
		CallRPC("node1", "getinfo");
		BOOST_CHECK_NO_THROW(CallRPC("node1", cmd, true, false));
		GenerateBlocks(5, "node1");
		GenerateBlocks(5, "node1");
		GenerateBlocks(5, "node1");
		UniValue r;
		BOOST_CHECK_NO_THROW(r = CallRPC("node1", "getblockchaininfo"));
		BOOST_CHECK(expiryTime <= find_value(r.get_obj(), "mediantime").get_int64());
	}
	catch (const runtime_error &e)
	{
	}
	try
	{
		CallRPC("node2", "getinfo");
		BOOST_CHECK_NO_THROW(CallRPC("node2", cmd, true, false));
		GenerateBlocks(5, "node2");
		GenerateBlocks(5, "node2");
		GenerateBlocks(5, "node2");
		UniValue r;
		BOOST_CHECK_NO_THROW(r = CallRPC("node2", "getblockchaininfo"));
		BOOST_CHECK(expiryTime <= find_value(r.get_obj(), "mediantime").get_int64());
	}
	catch (const runtime_error &e)
	{
	}
	try
	{
		CallRPC("node3", "getinfo");
		BOOST_CHECK_NO_THROW(CallRPC("node3", cmd, true, false));
		GenerateBlocks(5, "node3");
		GenerateBlocks(5, "node3");
		GenerateBlocks(5, "node3");
		UniValue r;
		BOOST_CHECK_NO_THROW(r = CallRPC("node3", "getblockchaininfo"));
		BOOST_CHECK(expiryTime <= find_value(r.get_obj(), "mediantime").get_int64());
	}
	catch (const runtime_error &e)
	{
	}
}
void ExpireAlias(const string& alias)
{
	int64_t expiryTime = 0;
	// ensure alias is expired
	UniValue r;
	try
	{
		UniValue aliasres;
		aliasres = CallRPC("node1", "aliasinfo " + alias);
		expiryTime = find_value(aliasres.get_obj(), "expires_on").get_int64();
		SetSysMocktime(expiryTime + 2);
		r = CallRPC("node1", "aliasinfo " + alias);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), true);
	}
	catch(const runtime_error &e)
	{
	}
	try
	{
		if (expiryTime <= 0) {
			UniValue aliasres;
			aliasres = CallRPC("node2", "aliasinfo " + alias);
			expiryTime = find_value(aliasres.get_obj(), "expires_on").get_int64();
			SetSysMocktime(expiryTime + 2);
		}
		r = CallRPC("node2", "aliasinfo " + alias);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), true);
	}
	catch(const runtime_error &e)
	{
	}
	try
	{
		if (expiryTime <= 0) {
			UniValue aliasres;
			aliasres = CallRPC("node3", "aliasinfo " + alias);
			expiryTime = find_value(aliasres.get_obj(), "expires_on").get_int64();
			SetSysMocktime(expiryTime + 2);
		}
		r = CallRPC("node3", "aliasinfo " + alias);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), true);
	}
	catch(const runtime_error &e)
	{
	}
}
void GetOtherNodes(const string& node, string& otherNode1, string& otherNode2)
{
	otherNode1 = "";
	otherNode2 = "";
	if(node == "node1")
	{
		if(node2Online)
			otherNode1 = "node2";
		if(node3Online)
			otherNode2 = "node3";
	}
	else if(node == "node2")
	{
		if(node1Online)
			otherNode1 = "node1";
		if(node3Online)
			otherNode2 = "node3";
	}
	else if(node == "node3")
	{
		if(node1Online)
			otherNode1 = "node1";
		if(node2Online)
			otherNode2 = "node2";
	}

}
void CheckRangeMerge(const string& originalRanges, const string& newRanges, const string& expectedOutputRanges) 
{
	vector<string> originalRangeTokens;
	boost::split(originalRangeTokens, originalRanges, boost::is_any_of(" "));
	if (originalRangeTokens.empty() && !originalRanges.empty())
		originalRangeTokens.push_back(originalRanges);

	vector<CRange> vecRanges;
	for (auto &token : originalRangeTokens) {
		BOOST_CHECK(token.size() > 2);
		token = token.substr(1, token.size() - 2);
		vector<string> ranges;
		boost::split(ranges, token, boost::is_any_of(","));
		BOOST_CHECK_EQUAL(ranges.size(), 2);
		CRange range(boost::lexical_cast<unsigned int>(ranges[0]), boost::lexical_cast<unsigned int>(ranges[1]));
		vecRanges.push_back(range);
	}


	vector<string> newRangeTokens;
	boost::split(newRangeTokens, newRanges, boost::is_any_of(" "));
	if (newRangeTokens.empty() && !newRanges.empty())
		newRangeTokens.push_back(newRanges);
	vector<CRange> vecNewRanges;
	for (auto &token : newRangeTokens) {
		BOOST_CHECK(token.size() > 2);
		token = token.substr(1, token.size() - 2);
		vector<string> ranges;
		boost::split(ranges, token, boost::is_any_of(","));
		BOOST_CHECK_EQUAL(ranges.size(), 2);
		CRange range(boost::lexical_cast<unsigned int>(ranges[0]), boost::lexical_cast<unsigned int>(ranges[1]));
		vecNewRanges.push_back(range);
	}

	vecRanges.insert(std::end(vecRanges), std::begin(vecNewRanges), std::end(vecNewRanges));

	vector<CRange> mergedRanges;
	mergeRanges(vecRanges, mergedRanges);

	vector<string> expectedOutputRangeTokens;
	boost::split(expectedOutputRangeTokens, expectedOutputRanges, boost::is_any_of(" "));
	if (expectedOutputRangeTokens.empty() && !expectedOutputRanges.empty())
		expectedOutputRangeTokens.push_back(expectedOutputRanges);

	vector<CRange> vecExpectedOutputRanges;
	for (auto &token : expectedOutputRangeTokens) {
		BOOST_CHECK(token.size() > 2);
		token = token.substr(1, token.size() - 2);
		vector<string> ranges;
		boost::split(ranges, token, boost::is_any_of(","));
		BOOST_CHECK_EQUAL(ranges.size(), 2);
		CRange range(boost::lexical_cast<unsigned int>(ranges[0]), boost::lexical_cast<unsigned int>(ranges[1]));
		vecExpectedOutputRanges.push_back(range);
	}

	BOOST_CHECK_EQUAL(mergedRanges.size(), vecExpectedOutputRanges.size());
	for (int i = 0; i < mergedRanges.size();i++) {
		BOOST_CHECK(mergedRanges[i] == vecExpectedOutputRanges[i]);
	}
}
void CheckRangeSubtract(const string& originalRanges, const string& subtractRange, const string& expectedOutputRanges) 
{
	vector<string> originalRangeTokens;
	boost::split(originalRangeTokens, originalRanges, boost::is_any_of(" "));
	if (originalRangeTokens.empty() && !originalRanges.empty())
		originalRangeTokens.push_back(originalRanges);

	vector<CRange> vecRanges;
	for (auto &token : originalRangeTokens) {
		BOOST_CHECK(token.size() > 2);
		token = token.substr(1, token.size() - 2);
		vector<string> ranges;
		boost::split(ranges, token, boost::is_any_of(","));
		BOOST_CHECK_EQUAL(ranges.size(), 2);
		CRange range(boost::lexical_cast<unsigned int>(ranges[0]), boost::lexical_cast<unsigned int>(ranges[1]));
		vecRanges.push_back(range);
	}


	vector<string> subtractRangeTokens;
	boost::split(subtractRangeTokens, subtractRange, boost::is_any_of(" "));
	if (subtractRangeTokens.empty() && !subtractRange.empty())
		subtractRangeTokens.push_back(subtractRange);

	vector<CRange> vecSubtractRanges;
	for (auto &token : subtractRangeTokens) {
		BOOST_CHECK(token.size() > 2);
		token = token.substr(1, token.size() - 2);
		vector<string> ranges;
		boost::split(ranges, token, boost::is_any_of(","));
		BOOST_CHECK_EQUAL(ranges.size(), 2);
		CRange range(boost::lexical_cast<unsigned int>(ranges[0]), boost::lexical_cast<unsigned int>(ranges[1]));
		vecSubtractRanges.push_back(range);
	}

	vector<CRange> mergedRanges;
	subtractRanges(vecRanges, vecSubtractRanges, mergedRanges);

	vector<string> expectedOutputRangeTokens;
	boost::split(expectedOutputRangeTokens, expectedOutputRanges, boost::is_any_of(" "));
	if (expectedOutputRangeTokens.empty() && !expectedOutputRanges.empty())
		expectedOutputRangeTokens.push_back(expectedOutputRanges);

	vector<CRange> vecExpectedOutputRanges;
	for (auto &token : expectedOutputRangeTokens) {
		BOOST_CHECK(token.size() > 2);
		token = token.substr(1, token.size() - 2);
		vector<string> ranges;
		boost::split(ranges, token, boost::is_any_of(","));
		BOOST_CHECK_EQUAL(ranges.size(), 2);
		CRange range(boost::lexical_cast<unsigned int>(ranges[0]), boost::lexical_cast<unsigned int>(ranges[1]));
		vecExpectedOutputRanges.push_back(range);
	}

	BOOST_CHECK_EQUAL(mergedRanges.size(), vecExpectedOutputRanges.size());
	for (int i = 0; i < mergedRanges.size(); i++) {
		BOOST_CHECK(mergedRanges[i] == vecExpectedOutputRanges[i]);
	}
}
bool DoesRangeContain(const string& parentRange, const string& childRange) {
	vector<string> parentRangeTokens;
	boost::split(parentRangeTokens, parentRange, boost::is_any_of(" "));
	if (parentRangeTokens.empty() && !parentRange.empty())
		parentRangeTokens.push_back(parentRange);

	vector<CRange> vecParentRanges;
	for (auto &token : parentRangeTokens) {
		BOOST_CHECK(token.size() > 2);
		token = token.substr(1, token.size() - 2);
		vector<string> ranges;
		boost::split(ranges, token, boost::is_any_of(","));
		BOOST_CHECK_EQUAL(ranges.size(), 2);
		CRange range(boost::lexical_cast<unsigned int>(ranges[0]), boost::lexical_cast<unsigned int>(ranges[1]));
		vecParentRanges.push_back(range);
	}

	vector<string> childRangeTokens;
	boost::split(childRangeTokens, childRange, boost::is_any_of(" "));
	if (childRangeTokens.empty() && !childRange.empty())
		childRangeTokens.push_back(childRange);

	vector<CRange> vecChildRanges;
	for (auto &token : childRangeTokens) {
		BOOST_CHECK(token.size() > 2);
		token = token.substr(1, token.size() - 2);
		vector<string> ranges;
		boost::split(ranges, token, boost::is_any_of(","));
		BOOST_CHECK_EQUAL(ranges.size(), 2);
		CRange range(boost::lexical_cast<unsigned int>(ranges[0]), boost::lexical_cast<unsigned int>(ranges[1]));
		vecChildRanges.push_back(range);
	}

	return doesRangeContain(vecParentRanges, vecChildRanges);
}
string AliasNew(const string& node, const string& aliasname, const string& pubdata, string witness)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);

	CKey privEncryptionKey;
	privEncryptionKey.MakeNewKey(true);
	CPubKey pubEncryptionKey = privEncryptionKey.GetPubKey();
	vector<unsigned char> vchPrivEncryptionKey(privEncryptionKey.begin(), privEncryptionKey.end());
	vector<unsigned char> vchPubEncryptionKey(pubEncryptionKey.begin(), pubEncryptionKey.end());

	vector<unsigned char> vchPubKey;
	CKey privKey;
	vector<unsigned char> vchPasswordSalt;

	privKey.MakeNewKey(true);
	CPubKey pubKey = privKey.GetPubKey();
	vchPubKey = vector<unsigned char>(pubKey.begin(), pubKey.end());
	CSyscoinAddress aliasAddress(pubKey.GetID());
	vector<unsigned char> vchPrivKey(privKey.begin(), privKey.end());
	BOOST_CHECK(privKey.IsValid());
	BOOST_CHECK(privEncryptionKey.IsValid());
	BOOST_CHECK(pubKey.IsFullyValid());
	BOOST_CHECK_NO_THROW(CallRPC(node, "importprivkey " + CSyscoinSecret(privKey).ToString() + " \"\" false", true, false));
	BOOST_CHECK_NO_THROW(CallRPC(node, "importprivkey " + CSyscoinSecret(privEncryptionKey).ToString() + " \"\" false", true, false));

	string strEncryptionPrivateKeyHex = HexStr(vchPrivEncryptionKey);
	string acceptTransfers = "3";
	string expireTime = "0";

	UniValue r;
	// registration
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasnew " + aliasname + " " + pubdata + " " + acceptTransfers +  " " + expireTime + " " + aliasAddress.ToString() + " " + strEncryptionPrivateKeyHex + " " + HexStr(vchPubEncryptionKey) + " " + witness));
	UniValue varray = r.get_array();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransaction " + varray[0].get_str()));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinsendrawtransaction " + find_value(r.get_obj(), "hex").get_str()));
	GenerateBlocks(5, node);
	// activation
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasnew " + aliasname + " " + pubdata + " " + acceptTransfers + " " + expireTime + " " + aliasAddress.ToString() + " " + strEncryptionPrivateKeyHex + " " + HexStr(vchPubEncryptionKey) + " " + witness));
	UniValue varray1 = r.get_array();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransaction " + varray1[0].get_str()));
	string hex_str = find_value(r.get_obj(), "hex").get_str();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinsendrawtransaction " + hex_str));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "decoderawtransaction " + hex_str));
	string txid = find_value(r.get_obj(), "txid").get_str();
	GenerateBlocks(5, node);
	GenerateBlocks(5, node);
	BOOST_CHECK_THROW(CallRPC(node, "sendtoaddress " + aliasname + " 10"), runtime_error);
	GenerateBlocks(5, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + aliasname));
	CAmount balanceAfter = AmountFromValue(find_value(r.get_obj(), "balance"));
	BOOST_CHECK(balanceAfter >= 10*COIN);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));

	UniValue txHistoryResult = AliasTxHistoryFilter(node, txid + "-" + aliasname);
	BOOST_CHECK(!txHistoryResult.empty());
	UniValue ret;
	BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
	UniValue historyResultObj = ret.get_obj();
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "user1").get_str(), aliasname);
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "_id").get_str(), txid + "-" + aliasname);
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "type").get_str(), "Alias Activated");

	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str(), pubdata);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "aliasinfo " + aliasname));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str(), pubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());
		txHistoryResult = AliasTxHistoryFilter(otherNode1, txid + "-" + aliasname);
		BOOST_CHECK(!txHistoryResult.empty());
		BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
		historyResultObj = ret.get_obj();
	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "aliasinfo " + aliasname));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str(), pubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());
		txHistoryResult = AliasTxHistoryFilter(otherNode2, txid + "-" + aliasname);
		BOOST_CHECK(!txHistoryResult.empty());
		BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
		historyResultObj = ret.get_obj();
	}
	return aliasAddress.ToString();
}
string AliasTransfer(const string& node, const string& aliasname, const string& tonode, const string& pubdata, const string& witness)
{
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));

	string oldvalue = find_value(r.get_obj(), "publicvalue").get_str();
	int nAcceptTransferFlags = find_value(r.get_obj(), "accepttransferflags").get_int();
	string expires = boost::lexical_cast<string>(find_value(r.get_obj(), "expires_on").get_int64());
	string encryptionkey = find_value(r.get_obj(), "encryption_publickey").get_str();
	string encryptionprivkey = find_value(r.get_obj(), "encryption_privatekey").get_str();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + aliasname));
	CAmount balanceBefore = AmountFromValue(find_value(r.get_obj(), "balance"));

	CKey privKey, encryptionPrivKey;
	privKey.MakeNewKey(true);
	CPubKey pubKey = privKey.GetPubKey();
	vector<unsigned char> vchPubKey(pubKey.begin(), pubKey.end());
	CSyscoinAddress aliasAddress(pubKey.GetID());
	vector<unsigned char> vchPrivKey(privKey.begin(), privKey.end());
	BOOST_CHECK(privKey.IsValid());
	BOOST_CHECK(pubKey.IsFullyValid());
	BOOST_CHECK_NO_THROW(CallRPC(tonode, "importprivkey " + CSyscoinSecret(privKey).ToString() + " \"\" false", true, false));	

	string address = aliasAddress.ToString();
	string newpubdata = pubdata == "''" ? oldvalue : pubdata;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasupdate " + aliasname + " " + newpubdata + " " + address + " " + boost::lexical_cast<string>(nAcceptTransferFlags) + " " + expires + " " + encryptionprivkey + " " + encryptionkey + " " + witness));
	UniValue varray = r.get_array();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransaction " + varray[0].get_str()));
	string hex_str;
	try
	{
		hex_str = find_value(r.get_obj(), "hex").get_str();
		if (!find_value(r.get_obj(), "complete").get_bool())
			return hex_str;
	}
	catch (runtime_error &err)
	{
		return hex_str;
	}
	BOOST_CHECK_NO_THROW(CallRPC(node, "syscoinsendrawtransaction " + hex_str));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "decoderawtransaction " + hex_str));
	string txid = find_value(r.get_obj(), "txid").get_str();


	GenerateBlocks(5, tonode);
	GenerateBlocks(5, node);


	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));

	UniValue txHistoryResult = AliasTxHistoryFilter(node, txid + "-" + aliasname);
	BOOST_CHECK(!txHistoryResult.empty());
	UniValue ret;
	BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
	UniValue historyResultObj = ret.get_obj();
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "user1").get_str(), aliasname);
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "user2").get_str(), address);
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "_id").get_str(), txid + "-" + aliasname);
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "type").get_str(), "Alias Updated");


	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + aliasname));
	CAmount balanceAfter = AmountFromValue(find_value(r.get_obj(), "balance"));

	BOOST_CHECK(balanceAfter >= (balanceBefore-COIN));
	BOOST_CHECK_NO_THROW(r = CallRPC(tonode, "aliasinfo " + aliasname));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , newpubdata);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_privatekey").get_str() , encryptionprivkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());

	// check xferred right person and data changed
	BOOST_CHECK_NO_THROW(r = CallRPC(tonode, "aliasbalance " + aliasname));
	balanceAfter = AmountFromValue(find_value(r.get_obj(), "balance"));
	BOOST_CHECK_NO_THROW(r = CallRPC(tonode, "aliasinfo " + aliasname));
	BOOST_CHECK(balanceAfter >= (balanceBefore-COIN));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , newpubdata);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_privatekey").get_str() , encryptionprivkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());
	txHistoryResult = AliasTxHistoryFilter(tonode, txid + "-" + aliasname);
	BOOST_CHECK(!txHistoryResult.empty());
	BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
	historyResultObj = ret.get_obj();
	return "";
}
string AliasUpdate(const string& node, const string& aliasname, const string& pubdata, string addressStr, string witness)
{
	string addressStr1 = addressStr;
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));

	string oldvalue = find_value(r.get_obj(), "publicvalue").get_str();
	string oldAddressStr = find_value(r.get_obj(), "address").get_str();
	string encryptionkey = find_value(r.get_obj(), "encryption_publickey").get_str();
	string encryptionprivkey = find_value(r.get_obj(), "encryption_privatekey").get_str();
	string expires = boost::lexical_cast<string>(find_value(r.get_obj(), "expires_on").get_int64());
	int nAcceptTransferFlags = find_value(r.get_obj(), "accepttransferflags").get_int();

	string newpubdata = pubdata == "''" ? oldvalue : pubdata;
	string newAddressStr = addressStr == "''" ? oldAddressStr : addressStr;
	// "aliasupdate <aliasname> [public value]  [address] [accept_transfers=true] [expire_timestamp] [encryption_privatekey] [encryption_publickey] [witness]\n"
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasupdate " + aliasname + " " + newpubdata + " " + newAddressStr + " " + boost::lexical_cast<string>(nAcceptTransferFlags) + " " + expires + " " + encryptionprivkey + " " + encryptionkey + " " + witness));
	UniValue varray = r.get_array();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransaction " + varray[0].get_str()));
	string hex_str;
	try
	{
		hex_str = find_value(r.get_obj(), "hex").get_str();
		if (!find_value(r.get_obj(), "complete").get_bool())
			return hex_str;
	}
	catch (runtime_error &err)
	{
		return hex_str;
	}
	BOOST_CHECK_NO_THROW(CallRPC(node, "syscoinsendrawtransaction " + hex_str));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "decoderawtransaction " + hex_str));
	string txid = find_value(r.get_obj(), "txid").get_str();

	GenerateBlocks(5, node);

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , newAddressStr);
	
	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);

	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , newpubdata);
	
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_privatekey").get_str() , encryptionprivkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);

	UniValue txHistoryResult = AliasTxHistoryFilter(node, txid + "-" + aliasname);
	BOOST_CHECK(!txHistoryResult.empty());
	UniValue ret;
	BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
	UniValue historyResultObj = ret.get_obj();
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "user1").get_str(), aliasname);
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "_id").get_str(), txid + "-" + aliasname);
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "type").get_str(), "Alias Updated");
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "aliasinfo " + aliasname));
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , newAddressStr);
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , newpubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);
		txHistoryResult = AliasTxHistoryFilter(otherNode1, txid + "-" + aliasname);
		BOOST_CHECK(!txHistoryResult.empty());
		BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
		historyResultObj = ret.get_obj();

	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "aliasinfo " + aliasname));
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , newAddressStr);
		
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);

		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , newpubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);
		txHistoryResult = AliasTxHistoryFilter(otherNode2, txid + "-" + aliasname);
		BOOST_CHECK(!txHistoryResult.empty());
		BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
		historyResultObj = ret.get_obj();
	}
	return "";

}
bool AliasFilter(const string& node, const string& alias)
{
	UniValue r;
	int64_t currentTime = GetTime();
	string query = "\"{\\\"_id\\\":\\\"" + alias + "\\\"}\"";
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinquery alias " + query));
	const UniValue &arr = r.get_array();
	return !arr.empty();
}
UniValue AliasTxHistoryFilter(const string& node, const string& id)
{
	UniValue r;
	int64_t currentTime = GetTime();
	string query = "\"{\\\"_id\\\":\\\"" + id + "\\\"}\"";
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinquery aliastxhistory " + query));
	const UniValue &arr = r.get_array();
	return arr;
}

bool CertFilter(const string& node, const string& cert)
{
	UniValue r;
	int64_t currentTime = GetTime();
	string query = "\"{\\\"_id\\\":\\\"" + cert + "\\\"}\"";
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinquery cert " + query));
	const UniValue &arr = r.get_array();
	return !arr.empty();
}

const string CertNew(const string& node, const string& alias, const string& title, const string& pubdata, const string& witness)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + alias));


	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certnew " + alias + " " + title + " " + pubdata + " " + " certificates " + witness));
	UniValue arr = r.get_array();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransaction " + arr[0].get_str()));
	string hex_str = find_value(r.get_obj(), "hex").get_str();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinsendrawtransaction " + hex_str));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "decoderawtransaction " + hex_str));
	string txid = find_value(r.get_obj(), "txid").get_str();

	string guid = arr[1].get_str();

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certinfo " + guid));

	const UniValue &txHistoryResult = AliasTxHistoryFilter(node, txid + "-" + guid);
	BOOST_CHECK(!txHistoryResult.empty());
	UniValue ret;
	BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
	const UniValue &historyResultObj = ret.get_obj();
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "user1").get_str(), alias);
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "_id").get_str(), txid + "-" + guid);
	
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "type").get_str(), "Certificate Activated");

	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
	BOOST_CHECK(find_value(r.get_obj(), "alias").get_str() == alias);
	BOOST_CHECK(find_value(r.get_obj(), "title").get_str() == title);
	BOOST_CHECK(find_value(r.get_obj(), "publicvalue").get_str() == pubdata);
	GenerateBlocks(5, node);
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "certinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "publicvalue").get_str() == pubdata);
		BOOST_CHECK(find_value(r.get_obj(), "title").get_str() == title);
	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "certinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "publicvalue").get_str() == pubdata);
		BOOST_CHECK(find_value(r.get_obj(), "title").get_str() == title);
	}
	
	return guid;
}
void CertUpdate(const string& node, const string& guid, const string& title, const string& pubdata, const string& witness)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certinfo " + guid));
	string oldalias = find_value(r.get_obj(), "alias").get_str();
	string oldpubdata = find_value(r.get_obj(), "publicvalue").get_str();
	string oldtitle = find_value(r.get_obj(), "title").get_str();

	string newpubdata = pubdata == "''" ? oldpubdata : pubdata;
	string newtitle = title == "''" ? oldtitle : title;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + oldalias));

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certupdate " + guid + " " + newtitle + " " + newpubdata + " certificates " + witness));
	UniValue arr = r.get_array();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransaction " + arr[0].get_str()));
	string hex_str = find_value(r.get_obj(), "hex").get_str();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinsendrawtransaction " + hex_str));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "decoderawtransaction " + hex_str));
	string txid = find_value(r.get_obj(), "txid").get_str();


	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certinfo " + guid));

	const UniValue &txHistoryResult = AliasTxHistoryFilter(node, txid + "-" + guid);
	BOOST_CHECK(!txHistoryResult.empty());
	UniValue ret;
	BOOST_CHECK(ret.read(txHistoryResult[0].get_str()));
	const UniValue &historyResultObj = ret.get_obj();
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "user1").get_str(), oldalias);
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "_id").get_str(), txid + "-" + guid);
	
	BOOST_CHECK_EQUAL(find_value(historyResultObj, "type").get_str(), "Certificate Updated");


	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
	BOOST_CHECK(find_value(r.get_obj(), "alias").get_str() == oldalias);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "title").get_str(), newtitle);
	GenerateBlocks(5, node);
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "certinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "alias").get_str() == oldalias);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , newpubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "title").get_str(), newtitle);

	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "certinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "alias").get_str() == oldalias);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , newpubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "title").get_str(), newtitle);

	}
	
}

BasicSyscoinTestingSetup::BasicSyscoinTestingSetup()
{
}
BasicSyscoinTestingSetup::~BasicSyscoinTestingSetup()
{
}
SyscoinTestingSetup::SyscoinTestingSetup()
{
	StartNodes();
}
SyscoinTestingSetup::~SyscoinTestingSetup()
{
	StopNodes();
}
SyscoinMainNetSetup::SyscoinMainNetSetup()
{
	StartMainNetNodes();
}
SyscoinMainNetSetup::~SyscoinMainNetSetup()
{
	StopMainNetNodes();
}
