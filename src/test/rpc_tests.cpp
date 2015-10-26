// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"
#include "rpcclient.h"

#include "base58.h"
#include "netbase.h"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace json_spirit;

Array
createArgs(int nRequired, const char* address1=NULL, const char* address2=NULL)
{
    Array result;
    result.push_back(nRequired);
    Array addresses;
    if (address1) addresses.push_back(address1);
    if (address2) addresses.push_back(address2);
    result.push_back(addresses);
    return result;
}

Value CallRPC(string args)
{
    vector<string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    Array params = RPCConvertValues(strMethod, vArgs);

    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        Value result = (*method)(params, false);
        return result;
    }
    catch (Object& objError)
    {
        throw runtime_error(find_value(objError, "message").get_str());
    }
}


BOOST_AUTO_TEST_SUITE(rpc_tests)

BOOST_AUTO_TEST_CASE(rpc_rawparams)
{
    // Test raw transaction API argument handling
}

BOOST_AUTO_TEST_CASE(rpc_rawsign)
{
}

BOOST_AUTO_TEST_CASE(rpc_format_monetary_values)
{
}

static Value ValueFromString(const std::string &str)
{
    Value value;
    BOOST_CHECK(read_string(str, value));
    return value;
}

BOOST_AUTO_TEST_CASE(rpc_parse_monetary_values)
{
}

BOOST_AUTO_TEST_CASE(rpc_boostasiotocnetaddr)
{
    // Check IPv4 addresses
    BOOST_CHECK_EQUAL(BoostAsioToCNetAddr(boost::asio::ip::address::from_string("1.2.3.4")).ToString(), "1.2.3.4");
    BOOST_CHECK_EQUAL(BoostAsioToCNetAddr(boost::asio::ip::address::from_string("127.0.0.1")).ToString(), "127.0.0.1");
    // Check IPv6 addresses
    BOOST_CHECK_EQUAL(BoostAsioToCNetAddr(boost::asio::ip::address::from_string("::1")).ToString(), "::1");
    BOOST_CHECK_EQUAL(BoostAsioToCNetAddr(boost::asio::ip::address::from_string("123:4567:89ab:cdef:123:4567:89ab:cdef")).ToString(),
                                         "123:4567:89ab:cdef:123:4567:89ab:cdef");
    // v4 compatible must be interpreted as IPv4
    BOOST_CHECK_EQUAL(BoostAsioToCNetAddr(boost::asio::ip::address::from_string("::0:127.0.0.1")).ToString(), "127.0.0.1");
    // v4 mapped must be interpreted as IPv4
    BOOST_CHECK_EQUAL(BoostAsioToCNetAddr(boost::asio::ip::address::from_string("::ffff:127.0.0.1")).ToString(), "127.0.0.1");
}

BOOST_AUTO_TEST_SUITE_END()
