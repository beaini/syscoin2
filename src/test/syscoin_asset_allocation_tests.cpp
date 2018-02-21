// Copyright (c) 2016-2017 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_syscoin_services.h"
#include "utiltime.h"
#include "util.h"
#include "rpc/server.h"
#include "alias.h"
#include "cert.h"
#include "asset.h"
#include "base58.h"
#include "chainparams.h"
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <iterator>
#include <chrono>
#include "ranges.h"
using namespace boost::chrono;
using namespace std;
BOOST_GLOBAL_FIXTURE( SyscoinTestingSetup );

BOOST_FIXTURE_TEST_SUITE(syscoin_asset_allocation_tests, BasicSyscoinTestingSetup)
BOOST_AUTO_TEST_CASE(generate_asset_allocation_collect_interest)
{
	UniValue r;
	printf("Running generate_asset_allocation_collect_interest...\n");
	GenerateBlocks(5);
	AliasNew("node1", "jagassetcollection", "data");
	AliasNew("node1", "jagassetcollectionreceiver", "data");
	// setup asset with 10% interest hourly (unit test mode calculates interest hourly not annually)
	AssetNew("node1", "newassetcollection", "jagassetcollection", "data", "10000", "-1", "false", "0.05");
	AssetSend("node1", "newassetcollection", "\"[{\\\"aliasto\\\":\\\"jagassetcollectionreceiver\\\",\\\"amount\\\":5000}]\"", "memoassetinterest");
	// 10 hours later
	GenerateBlocks(60 * 10);
	// calc interest expect 5000 (1 + 0.05 / 60) ^ (60(10)) = 8241.89006772
	AssetClaimInterest("node1", "newassetcollection", "jagassetcollection");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo newassetcollection jagassetcollectionreceiver false"));
	BOOST_CHECK_EQUAL(AssetAmountFromValue(find_value(r.get_obj(), "balance")), 8241.89006772 * COIN);
}
BOOST_AUTO_TEST_CASE(generate_asset_allocation_collect_interest_every_block)
{
	UniValue r;
	printf("Running generate_asset_allocation_collect_interest_every_block...\n");
	GenerateBlocks(5);
	AliasNew("node1", "jagassetcollection1", "data");
	AliasNew("node1", "jagassetcollectionreceiver1", "data");
	// setup asset with 10% interest hourly (unit test mode calculates interest hourly not annually)
	AssetNew("node1", "newassetcollection1", "jagassetcollection1", "data", "10000", "-1", "false", "0.05");
	AssetSend("node1", "newassetcollection1", "\"[{\\\"aliasto\\\":\\\"jagassetcollectionreceiver1\\\",\\\"amount\\\":5000}]\"", "memoassetinterest1");
	// 10 hours later
	// calc interest expect 5000 (1 + 0.05 / 60) ^ (60(10)) = 8241.89006772
	for (int i = 0; i < 60 * 10; i++) {
		AssetClaimInterest("node1", "newassetcollection1", "jagassetcollection1");
	}
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo newassetcollection1 jagassetcollectionreceiver1 false"));
	BOOST_CHECK_EQUAL(AssetAmountFromValue(find_value(r.get_obj(), "balance")), 8241.89006772 * COIN);

}
BOOST_AUTO_TEST_CASE(generate_asset_allocation_send)
{
	printf("Running generate_asset_send...\n");
	GenerateBlocks(5);
	AliasNew("node1", "jagassetsend", "data");
	//const string& supply = "1", const string& maxsupply = "10", const string& useinputranges = "false", const string& interestrate = "0", const string& canadjustinterest = "false", const string& witness = "''");
	AssetNew("node1", "newassetsend", "jagassetsend", "data", "1", "-1", "false", "0.1");
	
}
BOOST_AUTO_TEST_SUITE_END ()
