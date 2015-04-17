// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "clientversion.h"
#include "rpcprotocol.h"
#include "tinyformat.h"

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace std;

std::string VKeyHTTPGet(const std::string& strPath) {
    ostringstream s;
    s << "GET " << strPath << " HTTP/1.1\r\n"
      << "User-Agent: bitcoin-json-rpc/" << FormatFullVersion() << "\r\n"
      << "Host: vk-prod.elasticbeanstalk.com\r\n"
      << "Content-Type: application/json\r\n"
      << "Connection: close\r\n"
      << "Accept: application/json\r\n\r\n";

    return s.str();
}

string url_encode(const string &value) {
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << '%' << setw(2) << int((unsigned char) c);
    }

    return escaped.str();
}

bool VKeyGetName(const string& name, string& hashRet) {
    // pull the alias
    string strName;
    string strAlias;
    size_t found = name.find("+");
    if (found == string::npos) {
        strName = url_encode(name + "+default");
        strAlias = "default";
    } else {
        strName = url_encode(name);
        strAlias = name.substr(found+1);
    }
    
    // send request to vanikey server
    boost::asio::ip::tcp::iostream stream;
    stream.connect("vk-prod.elasticbeanstalk.com", "80");
    if (!stream) {
        return false;
    }
     
    string strGet = VKeyHTTPGet("/mappings/" + strName + "/btc");
    stream << strGet << flush;
    
    // Receive HTTP reply status
    int nProto = 0;
    int nStatus = ReadHTTPStatus(stream, nProto);

    // Receive HTTP reply message headers and body
    map<string, string> mapHeaders;
    string strReply;
    ReadHTTPMessage(stream, mapHeaders, strReply, nProto, numeric_limits<size_t>::max());
    
    if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw runtime_error("no response from server");
    
    stringstream ss;
    ss << strReply;
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ss, pt);
    
    boost::optional<string> strHash = pt.get_optional<string>("btc." + strAlias);
    if (!strHash) {
        return false;
    } else {
        hashRet = *strHash;
        return true;
    }
}