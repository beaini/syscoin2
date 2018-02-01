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
BOOST_GLOBAL_FIXTURE(SyscoinTestingSetup);

BOOST_FIXTURE_TEST_SUITE(syscoin_asset_tests, BasicSyscoinTestingSetup)
BOOST_AUTO_TEST_CASE(generate_range_merge)
{
	printf("Running generate_range_merge...\n");
	// start with {0,0} {2,3} {6,8}, add {4,5} to it and expect {0,0} {2,8}
	CheckRangeMerge("{0,0} {2,3} {6,8}", "{4,5}", "{0,0} {2,8}");

	CheckRangeMerge("{0,0} {2,3} {6,8}", "{4,5}", "{0,0} {2,8}");
	CheckRangeMerge("{2,3} {6,8}", "{0,0} {4,5}", "{0,0} {2,8}");
	CheckRangeMerge("{2,3}", "{0,0} {4,5} {6,8}", "{0,0} {2,8}");
	CheckRangeMerge("{0,0} {4,5} {6,8}", "{2,3}", "{0,0} {2,8}");

	CheckRangeMerge("{0,3}", "{4,5}", "{0,5}");//range within range
	CheckRangeMerge("{0,10}", "{4,8}", "{0,10}");//range within range on add 

	CheckRangeMerge("{0,11}", "{12,13}", "{0,13}");//check many ranges 
	CheckRangeMerge("{12,13}", "{0,11}", "{0,13}");//check many ranges 

	CheckRangeMerge("{7,73}", "{3,6}", "{3,73}");//check unusual numbers 

	CheckRangeMerge("{0,1}", "{2,4294967295}", "{0,4294967295}");//check large numbers

	CheckRangeMerge("{0,0}", "{0,0}", "{0,0}");//check all equal

	CheckRangeMerge("{0,1}", "{2,3}", "{0,3}");//check wrong way range
}
BOOST_AUTO_TEST_CASE(generate_range_subtract)
{
	printf("Running generate_range_subtract...\n");
	// start with {0,9}, subtract {0,0} {2,3} {6,8} from it and expect {1,1} {4,5} {9,9}
	CheckRangeSubtract("{0,9}", "{0,0} {2,3} {6,8}", "{1,1} {4,5} {9,9}");

	CheckRangeSubtract("{1,3} {6,10}", "{0,0} {2,2} {3,3}", "{1,1} {6,10}");
	CheckRangeSubtract("{1,3} {6,10}", "{0,0} {2,2}", "{1,1} {3,3} {6,10}");

	CheckRangeSubtract("{0,3}", "{2,3}", "{0,1}");//range within range

	CheckRangeSubtract("{0,11}", "{6,9}", "{0,5} {10,11}");//check many ranges 

	CheckRangeSubtract("{0,10}", "{1,2} {1,2}", "{0,0} {3,10}");//check double subtract 

	CheckRangeSubtract("{3,73}", "{7,13}", "{3,6} {14,73}");//check unusual numbers 

	CheckRangeSubtract("{0,4294967295}", "{0,1}", "{2,4294967295}");//check large numbers

	CheckRangeSubtract("{0,1}", "{10,15}", "{0,1}");//check subtract where there is nothing to subtract

	CheckRangeSubtract("{0,10}", "{5,4}", "{0,3} {6,10}");//check wrong way range
	CheckRangeSubtract("{10,0}", "{5,5}", "{0,4} {6,10}");//check wrong way range
	CheckRangeSubtract("{0,10}", "{10,10}", "{9,0}");//check wrong way range
}
BOOST_AUTO_TEST_CASE(generate_range_contain)
{
	printf("Running generate_range_contain...\n");
	// does {0,9} contain {0,0}?
	BOOST_CHECK(DoesRangeContain("{0,9}", "{0,0}"));
	BOOST_CHECK(DoesRangeContain("{0,2}", "{1,2}"));
	BOOST_CHECK(DoesRangeContain("{0,3}", "{2,2}"));
	BOOST_CHECK(DoesRangeContain("{0,0} {2,3} {6,8}", "{2,2}"));
	BOOST_CHECK(DoesRangeContain("{0,0} {2,3} {6,8}", "{2,3}"));
	BOOST_CHECK(DoesRangeContain("{0,0} {2,3} {6,8}", "{6,7}"));
	BOOST_CHECK(DoesRangeContain("{0,8}", "{0,0} {2,3} {6,8}"));
	BOOST_CHECK(DoesRangeContain("{0,8}", "{0,1} {2,4} {6,6}"));

	BOOST_CHECK(DoesRangeContain("{0,3}", "{0,1}"));//range within range

	BOOST_CHECK(DoesRangeContain("{0,11}", "{1,2} {10,11}"));//check many ranges 

	BOOST_CHECK(DoesRangeContain("{0,1} {0,1}", "{0,0} {1,1}"));//check double range 
	BOOST_CHECK(DoesRangeContain("{0,2}", "{2,2} {0,0}"));//check double range 

	BOOST_CHECK(DoesRangeContain("{3,73}", "{7,13}"));//check unusual numbers 

	BOOST_CHECK(DoesRangeContain("{0,4294967295}", "{0,1}"));//check big numbers 


	BOOST_CHECK(!DoesRangeContain("{1,9}", "{0,0}"));
	BOOST_CHECK(!DoesRangeContain("{1,9}", "{1,10}"));
	BOOST_CHECK(!DoesRangeContain("{1,2}", "{1,3}"));
	BOOST_CHECK(!DoesRangeContain("{1,2}", "{0,2}"));
	BOOST_CHECK(!DoesRangeContain("{1,2}", "{0,3}"));
	BOOST_CHECK(!DoesRangeContain("{0,0} {2,3} {6,8}", "{1,2}"));
	BOOST_CHECK(!DoesRangeContain("{0,0} {2,3} {6,8}", "{0,1}"));
	BOOST_CHECK(!DoesRangeContain("{0,0} {2,3} {6,8}", "{4,4}"));
	BOOST_CHECK(!DoesRangeContain("{0,0} {2,3} {6,8}", "{4,5}"));
	BOOST_CHECK(!DoesRangeContain("{0,0} {2,3} {6,8}", "{0,8}"));
	BOOST_CHECK(!DoesRangeContain("{0,8}", "{0,1} {2,4} {6,9}"));
	BOOST_CHECK(!DoesRangeContain("{0,8}", "{0,9} {2,4} {6,8}"));


	BOOST_CHECK(!DoesRangeContain("{0,2} {1,3} {2,3}", "{4,5}"));//range within range

	BOOST_CHECK(!DoesRangeContain("{0,11}", "{12,13} {10,11}"));//check many ranges 

	BOOST_CHECK(!DoesRangeContain("{0,1} {0,1}", "{10,10} {11,11}"));//check double range 
	BOOST_CHECK(!DoesRangeContain("{0,2}", "{10,10} {10,10}"));//check double range 

	BOOST_CHECK(!DoesRangeContain("{7,13}", "{3,73}"));//check unusual numbers 

	BOOST_CHECK(!DoesRangeContain("{0,1}", "{0,4294967296}"));//check big numbers 

	BOOST_CHECK(!DoesRangeContain("{1,0}", "{2,2}"));//wrong way range 
}
BOOST_AUTO_TEST_SUITE_END()