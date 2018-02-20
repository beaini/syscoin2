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

BOOST_AUTO_TEST_CASE(generate_asset_send)
{
	printf("Running generate_asset_send...\n");
	GenerateBlocks(5);
	AliasNew("node1", "jagassetsend", "data");
	AssetNew("node1", "newassetsend", "jagassetsend", "data");
	
}
BOOST_AUTO_TEST_SUITE_END ()
