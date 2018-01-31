// Copyright (c) 2016-2017 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_syscoin_services.h"
#include "utiltime.h"
#include "util.h"
#include "rpc/server.h"
#include "alias.h"
#include "cert.h"
#include "base58.h"
#include "chainparams.h"
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <iterator>
using namespace std;
BOOST_GLOBAL_FIXTURE( SyscoinTestingSetup );

BOOST_FIXTURE_TEST_SUITE (syscoin_asset_tests, BasicSyscoinTestingSetup)
BOOST_AUTO_TEST_CASE(generate_range_merge)
{
	printf("Running generate_range_merge...\n");
	CheckRangeMerge("{0,0} {2,3} {6,8}", "{4,5}", "{0,0} {2,8}");
}
BOOST_AUTO_TEST_CASE(generate_range_subtract)
{
	printf("Running generate_range_subtract...\n");
	CheckRangeSubtract("{0,9}", "{0,0} {2,3} {6,8}", "{1,1} {4,5} {9,9}");
	CheckRangeSubtract("{1,2} {3,3} {6,10}", "{0,0} {2,2} {3,3}", "{1,1} {6,10}");
}
BOOST_AUTO_TEST_CASE(generate_range_contain)
{
	printf("Running generate_range_contain...\n");
	BOOST_CHECK(DoesRangeContain("{0,9}", "{0,0}"));
	BOOST_CHECK(!DoesRangeContain("{1,9}", "{0,0}"));

}
BOOST_AUTO_TEST_SUITE_END ()