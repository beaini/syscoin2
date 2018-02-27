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
BOOST_AUTO_TEST_CASE(generate_asset_allocation_send)
{
	UniValue r;
	printf("Running generate_asset_allocation_send...\n");
	GenerateBlocks(5);
	AliasNew("node1", "jagassetallocationsend1", "data");
	AliasNew("node2", "jagassetallocationsend2", "data");
	AssetNew("node1", "newassetsend", "jagassetallocationsend1", "data", "1", "-1");

	AssetSend("node1", "newassetsend", "\"[{\\\"aliasto\\\":\\\"jagassetallocationsend1\\\",\\\"amount\\\":1}]\"", "assetallocationsend");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo newassetsend jagassetallocationsend1 false"));
	BOOST_CHECK_EQUAL(AssetAmountFromValue(find_value(r.get_obj(), "balance")), 1 * COIN);

	AssetAllocationSend(false, "node1", "newassetsend", "jagassetallocationsend1", "\"[{\\\"aliasto\\\":\\\"jagassetallocationsend2\\\",\\\"amount\\\":0.1}]\"", "allocationsendmemo");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo newassetsend jagassetallocationsend2 false"));
	BOOST_CHECK_EQUAL(AssetAmountFromValue(find_value(r.get_obj(), "balance")),0.1 * COIN);

	// send using zdag
	AssetAllocationSend(true, "node1", "newassetsend", "jagassetallocationsend1", "\"[{\\\"aliasto\\\":\\\"jagassetallocationsend2\\\",\\\"amount\\\":0.1}]\"", "allocationsendmemo");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo newassetsend jagassetallocationsend2 false"));
	BOOST_CHECK_EQUAL(AssetAmountFromValue(find_value(r.get_obj(), "balance")), 0.2 * COIN);

	// first tx should have to wait 1 sec for good status
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus newassetsend jagassetallocationsend1 jagassetallocationsend2"));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MINOR_CONFLICT_OK);

	// wait for 0.6 second as required by unit test
	MilliSleep(600);

	// second send
	AssetAllocationSend(true, "node1", "newassetsend", "jagassetallocationsend1", "\"[{\\\"aliasto\\\":\\\"jagassetallocationsend2\\\",\\\"amount\\\":0.1}]\"", "allocationsendmemo");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo newassetsend jagassetallocationsend2 false"));
	BOOST_CHECK_EQUAL(AssetAmountFromValue(find_value(r.get_obj(), "balance")), 0.3 * COIN);
	
	// for 0.5 seconds it should be flagged as a warning because assetallocationsend checks for 0.5 sec delay and assetallocationsenderstatus checks for 1 sec for minor warning
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus newassetsend jagassetallocationsend1 jagassetallocationsend2"));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MINOR_CONFLICT_OK);

	// wait for 1 second to clear minor warning status
	MilliSleep(1000);
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus newassetsend jagassetallocationsend1 jagassetallocationsend2"));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);
	BOOST_CHECK_NO_THROW(r = CallRPC("node2", "assetallocationsenderstatus newassetsend jagassetallocationsend1 jagassetallocationsend2"));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);
	BOOST_CHECK_NO_THROW(r = CallRPC("node3", "assetallocationsenderstatus newassetsend jagassetallocationsend1 jagassetallocationsend2"));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);

}
BOOST_AUTO_TEST_SUITE_END ()
