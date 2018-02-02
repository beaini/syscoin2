// Copyright (c) 2016-2017 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_syscoin_services.h"
#include "utiltime.h"
#include "util.h"
#include "rpc/server.h"
#include "alias.h"
#include "base58.h"
#include "chainparams.h"
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <iterator>
#include <chrono>
using namespace boost::chrono;
using namespace std;
BOOST_GLOBAL_FIXTURE(SyscoinTestingSetup);

void printRangeVector (vector<CRange> &vecRange, string tag) {
	printf("Printing vector range %s: ", tag.c_str());
	for(int index = 0; index < vecRange.size(); index++) {
		printf("{%i,%i} ", vecRange[index].start, vecRange[index].end);
	}
	printf("\n");
}
void addToRangeVector (vector<CRange> &vecRange, int range_single) { 
	CRange range(range_single, range_single);
	vecRange.push_back(range);
}
void addToRangeVector (vector<CRange> &vecRange, int range_start, int range_end) { 
	CRange range(range_start, range_end);
	vecRange.push_back(range);
}

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

	BOOST_CHECK(!DoesRangeContain("{0,1}", "{0,4294967295}"));//check big numbers 

	BOOST_CHECK(!DoesRangeContain("{1,0}", "{2,2}"));//wrong way range 
}
BOOST_AUTO_TEST_CASE(generate_range_complex)
{
	/* Test 1:  Generate two large input, 1 all even number 1 all odd and merge them */
	/* This test uses Range Test Library that contains addition vector operations    */
	printf("Running generate_range_complex...\n");
	string input1="", input2="", expected_output="";
	int total_range = 10000;
	int64_t ms1 = 0, ms2 = 0;

	printf("ExpectedOutput: range from 0-%i\n", total_range);
	printf("Input1: range from 0-%i\n Even number only\n", total_range-2);
	printf("Input2: range from 1-%i\n Odd number only\n", total_range-1);

	for (int index = 0; index < total_range; index=index+2) {
		input1 = input1 + "{" + to_string(index) + "," + to_string(index) +"} ";
		input2 = input2 + "{" + to_string(index+1) + "," + to_string(index+1) +"} ";
	}
	expected_output = "{0," + to_string(total_range-1) + "}"; 
	// Remove the last space from the string
	input1.pop_back();
	input2.pop_back();
	printf("Rangemerge Test: input1 + input2 = ExpectedOutput\n");
	ms1 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	CheckRangeMerge(input1, input2, expected_output);
	ms2 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	printf("CheckRangeMerge Completed %ldms\n", ms2-ms1);

	/* Test 2: Reverse of Test 1 (expected_output - input = 2) */
 	printf("RangeSubstract Test: ExpectedOutput - input1 = input2\n");	
	ms1 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	CheckRangeSubtract(expected_output, input1, input2);
	ms2 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	printf("CheckRangeSubtract1 Completed %ldms\n", ms2-ms1);

 	printf("RangeSubstract Test: ExpectedOutput - input2 = input1\n");	
	ms1 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	CheckRangeSubtract(expected_output, input2, input1);
	ms2 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	printf("CheckRangeSubtract2 Completed %ldms\n", ms2-ms1);
}
BOOST_AUTO_TEST_CASE(generate_range_stress_merge1) 
{
	// Test: merge1 
	// range1: {0-10m} even only
	// range2: {1-9999999} odd only
	// output:  range1 + range2
	printf("Running generate_range_stress_merge1:...\n");
	int total_range = 10000000;
	vector<CRange> vecRange1_i, vecRange2_i, vecRange_o, vecRange_expected;
	int64_t ms1 = 0, ms2 = 0;

	for (int index = 0; index < total_range; index=index+2) {
		addToRangeVector(vecRange1_i, index, index);
		addToRangeVector(vecRange2_i, index+1, index+1);
	}
	
	// Set expected outcome
	addToRangeVector(vecRange_expected, 0, total_range-1);
	
	// combine the two vectors of ranges
	vecRange1_i.insert(std::end(vecRange1_i), std::begin(vecRange2_i), std::end(vecRange2_i));

	ms1 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	mergeRanges(vecRange1_i, vecRange_o);
	ms2 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

	printf("\noutput range 1+2: merge time: %ldms\n", ms2-ms1);

	BOOST_CHECK(vecRange_o.size() == vecRange_expected.size());
	BOOST_CHECK(vecRange_o.back() == vecRange_expected.back());
	printf("\n Stress Test done \n");

}
BOOST_AUTO_TEST_CASE(generate_range_stress_subtract1) 
{
	// Test: subtract1 
	// range1: {0-1m} 
	// range2: {1m, 4m ,7m}
	// output:  range1 - range2
	printf("Running generate_range_stress_subtract1...\n");
	vector<CRange> vecRange1_i, vecRange2_i, vecRange_o, vecRange_expected;
	vector<CRange> vecRange2_i_copy;
	int64_t ms1 = 0, ms2 = 0;

	
	// Set input range 1 {0,10m}
	addToRangeVector(vecRange1_i, 0, 10000000);

	printRangeVector(vecRange1_i, "vecRange 1 input");
	// Set input range 2 {1m,4m,7m}
	addToRangeVector(vecRange2_i, 1000000);
	addToRangeVector(vecRange2_i, 4000000);
	addToRangeVector(vecRange2_i, 7000000);
	printRangeVector(vecRange2_i, "vecRange 2 input");
	
	// Set expected output {(1-999999),(1000001-3999999)...}
	addToRangeVector(vecRange_expected, 0, 999999);
	addToRangeVector(vecRange_expected, 1000001, 3999999);
	addToRangeVector(vecRange_expected, 4000001, 6999999);
	addToRangeVector(vecRange_expected, 7000001, 10000000);
	printRangeVector(vecRange_expected, "vecRange_expected");
	
	// Deep copy for test #2 since the vector will get modified
	vecRange2_i_copy = vecRange2_i;
	

	ms1 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	subtractRanges(vecRange1_i, vecRange2_i, vecRange_o);
	ms2 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

	BOOST_CHECK(vecRange_o.size() == vecRange_expected.size());
	BOOST_CHECK(vecRange_o.back() == vecRange_expected.back());

	vecRange2_i = vecRange2_i_copy;
	vector<CRange> vecRange2_o;
	vecRange2_i.insert(std::end(vecRange2_i), std::begin(vecRange_expected), std::end(vecRange_expected));
	ms1 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	mergeRanges(vecRange2_i, vecRange2_o);
	ms2 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	printf("\noutput range expected+2: merge time: %ld\n", ms2-ms1);

	BOOST_CHECK_EQUAL(vecRange2_o.size(), vecRange1_i.size());
	BOOST_CHECK_EQUAL(vecRange2_o.back().start, 0);
	BOOST_CHECK_EQUAL(vecRange2_o.back().end, 10000000);
}
BOOST_AUTO_TEST_CASE(generate_range_stress_merge2) 
{
	// Test: merge2
	// range1: {0-1m} odd only
	// range2: {100000 200000, ..., 900000 }
	// output:  range1 + range2
	printf("Running generate_range_stress_merge2...\n");
	vector<CRange> vecRange1_i, vecRange2_i, vecRange_o;
	int64_t ms1 = 0, ms2 = 0;
	int total_range = 1000000;

	// Create vector range 1 that's 0-1mill odd only
	for (int index = 0; index < total_range; index=index+2) {
		addToRangeVector(vecRange1_i, index+1, index+1);
	}
	// Create vector range 2 that's 100k,200k,300k...,900k
	for (int index = 100000; index < total_range; index=index+100000) {
		addToRangeVector(vecRange2_i, index, index);
	}

	ms1 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	vecRange1_i.insert(std::end(vecRange1_i), std::begin(vecRange2_i), std::end(vecRange2_i));
	mergeRanges(vecRange1_i, vecRange_o);
	ms2 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

	//HARDCODED checks
	BOOST_CHECK_EQUAL(vecRange_o.size(), (total_range/2) - 9);
	BOOST_CHECK_EQUAL(vecRange_o[49999].start, 99999); 
	BOOST_CHECK_EQUAL(vecRange_o[49999].end, 100001); 
	BOOST_CHECK_EQUAL(vecRange_o[99998].start, 199999); 
	BOOST_CHECK_EQUAL(vecRange_o[99998].end, 200001); 
	BOOST_CHECK_EQUAL(vecRange_o[449991].start, 899999); 
	BOOST_CHECK_EQUAL(vecRange_o[449991].end, 900001); 
	printf("CheckRangeSubtract Completed %ldms\n", ms2-ms1);
}
BOOST_AUTO_TEST_CASE(generate_range_stress_subtract2) 
{
	// Test: subtract2
	// range1: {0-1m} odd only
	// range2: {100001 200001, ..., 900001 }
	// output:  range1 - range2
	printf("Running generate_range_stress_subtract3...\n");
	vector<CRange> vecRange1_i, vecRange2_i, vecRange_o;
	int64_t ms1 = 0, ms2 = 0;
	int total_range = 1000000;

	// Create vector range 1 that's 0-1mill odd only
	for (int index = 0; index < total_range; index=index+2) {
		addToRangeVector(vecRange1_i, index+1,index+1);
	}
	// Create vector range 2 that's 100k,200k,300k...,900k
	for (int index = 100000; index < total_range; index=index+100000) {
		addToRangeVector(vecRange2_i, index+1,index+1);
	}

	ms1 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	subtractRanges(vecRange1_i, vecRange2_i, vecRange_o);
	ms2 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

	//HARDCODED checks
	BOOST_CHECK_EQUAL(vecRange_o.size(), 499991);
	printf("CheckRangeSubtract Completed %ldms\n", ms2-ms1);
}
BOOST_AUTO_TEST_SUITE_END()
