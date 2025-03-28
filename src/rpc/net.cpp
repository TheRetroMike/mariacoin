// Copyright (c) 2009-2015 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The MARIA developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/server.h"

#include "clientversion.h"
#include "net.h"
#include "netbase.h"
#include "net_processing.h"
#include "optional.h"
#include "protocol.h"
#include "sync.h"
#include "timedata.h"
#include "guiinterface.h"
#include "util/system.h"
#include "version.h"
#include "validation.h"
#include "warnings.h"

#include <univalue.h>


UniValue getconnectioncount(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getconnectioncount\n"
            "\nReturns the number of connections to other nodes.\n"

            "\nbResult:\n"
            "n          (numeric) The connection count\n"

            "\nExamples:\n" +
            HelpExampleCli("getconnectioncount", "") + HelpExampleRpc("getconnectioncount", ""));

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    return (int)g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL);
}

UniValue ping(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "ping\n"
            "\nRequests that a ping be sent to all other nodes, to measure ping time.\n"
            "Results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
            "Ping command is handled in queue with all other commands, so it measures processing backlog, not just network ping.\n"

            "\nExamples:\n" +
            HelpExampleCli("ping", "") + HelpExampleRpc("ping", ""));

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    // Request that each node send a ping during next message processing pass
    g_connman->ForEachNode([](CNode* pnode) {
        pnode->fPingQueued = true;
    });
    return NullUniValue;
}

UniValue getpeerinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getpeerinfo\n"
            "\nReturns data about each connected network node as a json array of objects.\n"

            "\nbResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": n,                   (numeric) Peer index\n"
            "    \"addr\":\"host:port\",      (string) The ip address and port of the peer\n"
            "    \"addrlocal\":\"ip:port\",   (string) local address\n"
            "    \"mapped_as\":\"mapped_as\", (string) The AS in the BGP route to the peer used for diversifying\n"
                                                       "peer selection (only available if the asmap config flag is set)\n"
            "    \"services\":\"xxxxxxxxxxxxxxxx\",   (string) The services offered\n"
            "    \"lastsend\": ttt,           (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last send\n"
            "    \"lastrecv\": ttt,           (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last receive\n"
            "    \"bytessent\": n,            (numeric) The total bytes sent\n"
            "    \"bytesrecv\": n,            (numeric) The total bytes received\n"
            "    \"conntime\": ttt,           (numeric) The connection time in seconds since epoch (Jan 1 1970 GMT)\n"
            "    \"timeoffset\": ttt,         (numeric) The time offset in seconds\n"
            "    \"pingtime\": n,             (numeric) ping time\n"
            "    \"pingwait\": n,             (numeric) ping wait\n"
            "    \"version\": v,              (numeric) The peer version, such as 7001\n"
            "    \"subver\": \"/Maria Core:x.x.x.x/\",  (string) The string version\n"
            "    \"inbound\": true|false,     (boolean) Inbound (true) or Outbound (false)\n"
            "    \"addnode\": true|false,     (boolean) Whether connection was due to addnode and is using an addnode slot\n"
            "    \"masternode\": true|false,  (boolean) Whether the connection is only for masternode quorums related messages\n"
            "    \"startingheight\": n,       (numeric) The starting height (block) of the peer\n"
            "    \"banscore\": n,             (numeric) The ban score\n"
            "    \"synced_headers\": n,       (numeric) The last header we have in common with this peer\n"
            "    \"synced_blocks\": n,        (numeric) The last block we have in common with this peer\n"
            "    \"inflight\": [\n"
            "       n,                        (numeric) The heights of blocks we're currently asking from this peer\n"
            "       ...\n"
            "    ]\n"
            "    \"bytessent_per_msg\": {\n"
            "       \"addr\": n,             (numeric) The total bytes sent aggregated by message type\n"
            "       ...\n"
            "    }\n"
            "    \"bytesrecv_per_msg\": {\n"
            "       \"addr\": n,             (numeric) The total bytes received aggregated by message type\n"
            "       ...\n"
            "    }\n"
            "   \"masternode_iqr_conn\": true|false,          (boolean) Whether the connection is an intra-quorum relay connection or not\n"
            "   \"verif_mn_proreg_tx_hash\": \"hex\",         (string) The MN provider register tx hash (if the connection is verified)\n"
            "   \"verif_mn_operator_pubkey_hash\": \"hex\",   (string) The MN operator pubkey hash (if the connection is verified)\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getpeerinfo", "") + HelpExampleRpc("getpeerinfo", ""));

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    std::vector<CNodeStats> vstats;
    g_connman->GetNodeStats(vstats);

    UniValue ret(UniValue::VARR);

    for (const CNodeStats& stats : vstats) {
        UniValue obj(UniValue::VOBJ);
        CNodeStateStats statestats;
        bool fStateStats = GetNodeStateStats(stats.nodeid, statestats);
        obj.pushKV("id", stats.nodeid);
        obj.pushKV("addr", stats.addrName);
        if (!(stats.addrLocal.empty()))
            obj.pushKV("addrlocal", stats.addrLocal);
        if (stats.m_mapped_as != 0) {
            obj.pushKV("mapped_as", uint64_t(stats.m_mapped_as));
        }
        obj.pushKV("services", strprintf("%016x", stats.nServices));
        obj.pushKV("lastsend", stats.nLastSend);
        obj.pushKV("lastrecv", stats.nLastRecv);
        obj.pushKV("bytessent", stats.nSendBytes);
        obj.pushKV("bytesrecv", stats.nRecvBytes);
        obj.pushKV("conntime", stats.nTimeConnected);
        obj.pushKV("timeoffset", stats.nTimeOffset);
        obj.pushKV("pingtime", stats.dPingTime);
        if (stats.dPingWait > 0.0)
            obj.pushKV("pingwait", stats.dPingWait);
        obj.pushKV("version", stats.nVersion);
        // Use the sanitized form of subver here, to avoid tricksy remote peers from
        // corrupting or modifiying the JSON output by putting special characters in
        // their ver message.
        obj.pushKV("subver", stats.cleanSubVer);
        obj.pushKV("inbound", stats.fInbound);
        obj.pushKV("addnode", stats.fAddnode);
        obj.pushKV("masternode", stats.m_masternode_connection);
        obj.pushKV("startingheight", stats.nStartingHeight);
        if (fStateStats) {
            obj.pushKV("banscore", statestats.nMisbehavior);
            obj.pushKV("synced_headers", statestats.nSyncHeight);
            obj.pushKV("synced_blocks", statestats.nCommonHeight);
            UniValue heights(UniValue::VARR);
            for (int height : statestats.vHeightInFlight) {
                heights.push_back(height);
            }
            obj.pushKV("inflight", heights);
        }
        obj.pushKV("whitelisted", stats.fWhitelisted);

        UniValue sendPerMsgCmd(UniValue::VOBJ);
        for (const mapMsgCmdSize::value_type &i : stats.mapSendBytesPerMsgCmd) {
            if (i.second > 0)
                sendPerMsgCmd.pushKV(i.first, i.second);
        }
        obj.pushKV("bytessent_per_msg", sendPerMsgCmd);

        UniValue recvPerMsgCmd(UniValue::VOBJ);
        for (const mapMsgCmdSize::value_type &i : stats.mapRecvBytesPerMsgCmd) {
            if (i.second > 0)
                recvPerMsgCmd.pushKV(i.first, i.second);
        }
        obj.pushKV("bytesrecv_per_msg", recvPerMsgCmd);

        // DMN data
        if (stats.m_masternode_connection) {
            obj.pushKV("masternode_iqr_conn", stats.m_masternode_iqr_connection);
            obj.pushKV("verif_mn_proreg_tx_hash", stats.verifiedProRegTxHash.GetHex());
            obj.pushKV("verif_mn_operator_pubkey_hash", stats.verifiedPubKeyHash.GetHex());
        }

        ret.push_back(obj);
    }

    return ret;
}

UniValue addnode(const JSONRPCRequest& request)
{
    std::string strCommand;
    if (request.params.size() == 2)
        strCommand = request.params[1].get_str();
    if (request.fHelp || request.params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw std::runtime_error(
            "addnode \"node\" \"add|remove|onetry\"\n"
            "\nAttempts add or remove a node from the addnode list.\n"
            "Or try a connection to a node once.\n"

            "\nArguments:\n"
            "1. \"node\"     (string, required) The node (see getpeerinfo for nodes)\n"
            "2. \"command\"  (string, required) 'add' to add a node to the list, 'remove' to remove a node from the list, 'onetry' to try a connection to the node once\n"

            "\nExamples:\n" +
            HelpExampleCli("addnode", "\"192.168.0.6:47773\" \"onetry\"") + HelpExampleRpc("addnode", "\"192.168.0.6:47773\", \"onetry\""));

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    std::string strNode = request.params[0].get_str();

    if (strCommand == "onetry") {
        CAddress addr;
        g_connman->OpenNetworkConnection(addr, false, NULL, strNode.c_str());
        return NullUniValue;
    }

    if (strCommand == "add") {
        if(!g_connman->AddNode(strNode))
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node already added");
    } else if (strCommand == "remove") {
        if(!g_connman->RemoveAddedNode(strNode))
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
    }

    return NullUniValue;
}

UniValue disconnectnode(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "disconnectnode \"node\" \n"
            "\nImmediately disconnects from the specified node.\n"

            "\nArguments:\n"
            "1. \"node\"     (string, required) The node (see getpeerinfo for nodes)\n"

            "\nExamples:\n"
            + HelpExampleCli("disconnectnode", "\"192.168.0.6:8333\"")
            + HelpExampleRpc("disconnectnode", "\"192.168.0.6:8333\"")
        );

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    bool ret = g_connman->DisconnectNode(request.params[0].get_str());
    if (!ret)
        throw JSONRPCError(RPC_CLIENT_NODE_NOT_CONNECTED, "Node not found in connected nodes");

    return NullUniValue;
}

UniValue getaddednodeinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "getaddednodeinfo dummy ( \"node\" )\n"
            "\nReturns information about the given added node, or all added nodes\n"
            "(note that onetry addnodes are not listed here)\n"

            "\nArguments:\n"
            "1. dummy      (boolean, required) Kept for historical purposes but ignored\n"
            "2. \"node\"   (string, optional) If provided, return information about this specific node, otherwise all nodes are returned.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"addednode\" : \"192.168.0.201\",   (string) The node ip address or name (as provided to addnode)\n"
            "    \"connected\" : true|false,          (boolean) If connected\n"
            "    \"addresses\" : [                    (list of objects) Only when connected = true\n"
            "       {\n"
            "         \"address\" : \"192.168.0.201:47773\",  (string) The maria server IP and port we're connected to\n"
            "         \"connected\" : \"outbound\"           (string) connection, inbound or outbound\n"
            "       }\n"
            "     ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getaddednodeinfo", "true") + HelpExampleCli("getaddednodeinfo", "true \"192.168.0.201\"") + HelpExampleRpc("getaddednodeinfo", "true, \"192.168.0.201\""));

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    std::vector<AddedNodeInfo> vInfo = g_connman->GetAddedNodeInfo();

    if (request.params.size() == 2) {
        bool found = false;
        for (const AddedNodeInfo& info : vInfo) {
            if (info.strAddedNode == request.params[1].get_str()) {
                vInfo.assign(1, info);
                found = true;
                break;
            }
        }
        if (!found) {
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
        }
    }

    UniValue ret(UniValue::VARR);

    for (const AddedNodeInfo& info : vInfo) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("addednode", info.strAddedNode);
        obj.pushKV("connected", info.fConnected);
        UniValue addresses(UniValue::VARR);
        if (info.fConnected) {
            UniValue address(UniValue::VOBJ);
            address.pushKV("address", info.resolvedAddress.ToString());
            address.pushKV("connected", info.fInbound ? "inbound" : "outbound");
            addresses.push_back(address);
        }
        obj.pushKV("addresses", addresses);
        ret.push_back(obj);
    }

    return ret;
}

UniValue getnettotals(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "getnettotals\n"
            "\nReturns information about network traffic, including bytes in, bytes out,\n"
            "and current time.\n"

            "\nResult:\n"
            "{\n"
            "  \"totalbytesrecv\": n,   (numeric) Total bytes received\n"
            "  \"totalbytessent\": n,   (numeric) Total bytes sent\n"
            "  \"timemillis\": t        (numeric) Total cpu time\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getnettotals", "") + HelpExampleRpc("getnettotals", ""));

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("totalbytesrecv", g_connman->GetTotalBytesRecv());
    obj.pushKV("totalbytessent", g_connman->GetTotalBytesSent());
    obj.pushKV("timemillis", GetTimeMillis());
    return obj;
}

static UniValue GetNetworksInfo()
{
    UniValue networks(UniValue::VARR);
    for (int n = 0; n < NET_MAX; ++n) {
        enum Network network = static_cast<enum Network>(n);
        if (network == NET_UNROUTABLE || network == NET_I2P || network == NET_CJDNS || network == NET_INTERNAL) continue;
        proxyType proxy;
        UniValue obj(UniValue::VOBJ);
        GetProxy(network, proxy);
        obj.pushKV("name", GetNetworkName(network));
        obj.pushKV("limited", !IsReachable(network));
        obj.pushKV("reachable", IsReachable(network));
        obj.pushKV("proxy", proxy.IsValid() ? proxy.proxy.ToStringIPPort() : std::string());
        obj.pushKV("proxy_randomize_credentials", proxy.randomize_credentials);
        networks.push_back(obj);
    }
    return networks;
}

UniValue getnetworkinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getnetworkinfo\n"
            "\nReturns an object containing various state info regarding P2P networking.\n"

            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,                      (numeric) the server version\n"
            "  \"subversion\": \"/Maria Core:x.x.x.x/\",     (string) the server subversion string\n"
            "  \"protocolversion\": xxxxx,              (numeric) the protocol version\n"
            "  \"localservices\": \"xxxxxxxxxxxxxxxx\", (string) the services we offer to the network\n"
            "  \"timeoffset\": xxxxx,                   (numeric) the time offset\n"
            "  \"connections\": xxxxx,                  (numeric) the number of connections\n"
            "  \"networkactive\": true|false,           (boolean) the network activity status\n"
            "  \"networks\": [                          (array) information per network\n"
            "  {\n"
            "    \"name\": \"xxx\",                     (string) network (ipv4, ipv6 or onion)\n"
            "    \"limited\": true|false,               (boolean) is the network limited using -onlynet?\n"
            "    \"reachable\": true|false,             (boolean) is the network reachable?\n"
            "    \"proxy\": \"host:port\"               (string) the proxy that is used for this network, or empty if none\n"
            "  }\n"
            "  ,...\n"
            "  ],\n"
            "  \"relayfee\": x.xxxxxxxx,                (numeric) minimum relay fee for transactions in " + CURRENCY_UNIT + "/kB\n"
            "  \"incrementalfee\": x.xxxxxxxx,          (numeric) minimum fee increment for mempool limiting or BIP 125 replacement in " + CURRENCY_UNIT + "/kB\n"
            "  \"localaddresses\": [                    (array) list of local addresses\n"
            "  {\n"
            "    \"address\": \"xxxx\",                 (string) network address\n"
            "    \"port\": xxx,                         (numeric) network port\n"
            "    \"score\": xxx                         (numeric) relative score\n"
            "  }\n"
            "  ,...\n"
            "  ]\n"
            "  \"warnings\": \"...\"                    (string) any network and blockchain warnings\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getnetworkinfo", "") + HelpExampleRpc("getnetworkinfo", ""));

    LOCK(cs_main);
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("version", CLIENT_VERSION);
    obj.pushKV("subversion",    strSubVersion);
    obj.pushKV("protocolversion", PROTOCOL_VERSION);
    if (g_connman)
        obj.pushKV("localservices", strprintf("%016x", g_connman->GetLocalServices()));
    obj.pushKV("timeoffset", GetTimeOffset());
    if (g_connman) {
        obj.pushKV("networkactive", g_connman->GetNetworkActive());
        obj.pushKV("connections",   (int)g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL));
    }
    obj.pushKV("networks", GetNetworksInfo());
    obj.pushKV("relayfee", ValueFromAmount(::minRelayTxFee.GetFeePerK()));
    UniValue localAddresses(UniValue::VARR);
    {
        LOCK(cs_mapLocalHost);
        for (const std::pair<const CNetAddr, LocalServiceInfo> &item : mapLocalHost) {
            UniValue rec(UniValue::VOBJ);
            rec.pushKV("address", item.first.ToString());
            rec.pushKV("port", item.second.nPort);
            rec.pushKV("score", item.second.nScore);
            localAddresses.push_back(rec);
        }
    }
    obj.pushKV("localaddresses", localAddresses);
    obj.pushKV("warnings", GetWarnings("statusbar"));
    return obj;
}

UniValue setban(const JSONRPCRequest& request)
{
    std::string strCommand;
    if (request.params.size() >= 2)
        strCommand = request.params[1].get_str();
    if (request.fHelp || request.params.size() < 2 ||
        (strCommand != "add" && strCommand != "remove"))
        throw std::runtime_error(
            "setban \"subnet\" \"add|remove\" ( bantime absolute )\n"
            "\nAttempts add or remove a IP/Subnet from the banned list.\n"

            "\nArguments:\n"
            "1. \"subnet\"       (string, required) The IP/Subnet (see getpeerinfo for nodes ip) with a optional netmask (default is /32 = single ip)\n"
            "2. \"command\"      (string, required) 'add' to add a IP/Subnet to the list, 'remove' to remove a IP/Subnet from the list\n"
            "3. \"bantime\"      (numeric, optional) time in seconds how long (or until when if [absolute] is set) the ip is banned (0 or empty means using the default time of 24h which can also be overwritten by the -bantime startup argument)\n"
            "4. \"absolute\"     (boolean, optional) If set, the bantime must be a absolute timestamp in seconds since epoch (Jan 1 1970 GMT)\n"

            "\nExamples:\n"
            + HelpExampleCli("setban", "\"192.168.0.6\" \"add\" 86400")
            + HelpExampleCli("setban", "\"192.168.0.0/24\" \"add\"")
            + HelpExampleRpc("setban", "\"192.168.0.6\", \"add\" 86400"));

    if (!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    CSubNet subNet;
    CNetAddr netAddr;
    bool isSubnet = false;

    if (request.params[0].get_str().find('/') != std::string::npos)
        isSubnet = true;

    if (!isSubnet) {
        CNetAddr resolved;
        LookupHost(request.params[0].get_str(), resolved, false);
        netAddr = resolved;
    } else
        LookupSubNet(request.params[0].get_str(), subNet);

    if (! (isSubnet ? subNet.IsValid() : netAddr.IsValid()) )
        throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Invalid IP/Subnet");

    if (strCommand == "add")
    {
        if (isSubnet ? g_connman->IsBanned(subNet) : g_connman->IsBanned(netAddr))
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: IP/Subnet already banned");

        int64_t banTime = 0; //use standard bantime if not specified
        if (request.params.size() >= 3 && !request.params[2].isNull())
            banTime = request.params[2].get_int64();

        bool absolute = false;
        if (request.params.size() == 4)
            absolute = request.params[3].get_bool();

        isSubnet ? g_connman->Ban(subNet, BanReasonManuallyAdded, banTime, absolute) : g_connman->Ban(netAddr, BanReasonManuallyAdded, banTime, absolute);
    }
    else if(strCommand == "remove")
    {
        if (!( isSubnet ? g_connman->Unban(subNet) : g_connman->Unban(netAddr) ))
            throw JSONRPCError(RPC_MISC_ERROR, "Error: Unban failed");
    }
    return NullUniValue;
}

UniValue listbanned(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "listbanned\n"
            "\nList all banned IPs/Subnets.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"address\": \"xxx\",          (string) Network address of banned client.\n"
            "    \"banned_until\": nnn,         (numeric) Timestamp when the ban is lifted.\n"
            "    \"ban_created\": nnn,          (numeric) Timestamp when the ban was created.\n"
            "    \"ban_reason\": \"xxx\"        (string) Reason for banning.\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listbanned", "")
            + HelpExampleRpc("listbanned", ""));

    if (!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    banmap_t banMap;
    g_connman->GetBanned(banMap);

    UniValue bannedAddresses(UniValue::VARR);
    for (const auto& entry : banMap)
    {
        const CBanEntry& banEntry = entry.second;
        UniValue rec(UniValue::VOBJ);
        rec.pushKV("address", entry.first.ToString());
        rec.pushKV("banned_until", banEntry.nBanUntil);
        rec.pushKV("ban_created", banEntry.nCreateTime);
        rec.pushKV("ban_reason", banEntry.banReasonToString());

        bannedAddresses.push_back(rec);
    }

    return bannedAddresses;
}

UniValue clearbanned(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "clearbanned\n"
            "\nClear all banned IPs.\n"

            "\nExamples:\n"
            + HelpExampleCli("clearbanned", "")
            + HelpExampleRpc("clearbanned", ""));

    if (!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    g_connman->ClearBanned();

    return NullUniValue;
}

static UniValue getnodeaddresses(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "getnodeaddresses ( count \"network\" )\n"
            "\nReturn known addresses which can potentially be used to find new nodes in the network\n"

            "\nArguments:\n"
            "1. count        (numeric, optional) The maximum number of addresses to return. Specify 0 to return all known addresses.\n"
            "2. \"network\"  (string, optional) Return only addresses of the specified network. Can be one of: ipv4, ipv6, onion."

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"time\": ttt,                (numeric) Timestamp in seconds since epoch (Jan 1 1970 GMT) when the node was last seen\n"
            "    \"services\": n,              (numeric) The services offered by the node\n"
            "    \"address\": \"host\",        (string) The address of the node\n"
            "    \"port\": n,                  (numeric) The port number of the node\n"
            "    \"network\": \"xxxx\"         (string) The network (ipv4, ipv6, onion) the node connected through\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("getnodeaddresses", "8")
            + HelpExampleCli("getnodeaddresses", "4 \"ipv4\"")
            + HelpExampleRpc("getnodeaddresses", "8")
            + HelpExampleRpc("getnodeaddresses", "4 \"ipv4\"")
        );
    }
    if (!g_connman) {
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }

    const int count{request.params[0].isNull() ? 1 : request.params[0].get_int()};
    if (count < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Address count out of range");

    const Optional<Network> network{request.params[1].isNull() ? nullopt : Optional<Network>{ParseNetwork(request.params[1].get_str())}};
    if (network == NET_UNROUTABLE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Network not recognized: %s", request.params[1].get_str()));
    }

    // returns a shuffled list of CAddress
    const std::vector<CAddress> vAddr{g_connman->GetAddresses(count, /* max_pct */ 0, network)};
    UniValue ret(UniValue::VARR);

    for (const CAddress& addr : vAddr) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("time", (int)addr.nTime);
        obj.pushKV("services", (uint64_t)addr.nServices);
        obj.pushKV("address", addr.ToStringIP());
        obj.pushKV("port", addr.GetPort());
        obj.pushKV("network", GetNetworkName(addr.GetNetClass()));
        ret.push_back(obj);
    }
    return ret;
}

static UniValue addpeeraddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "addpeeraddress \"address\" port\n"
            "\nAdd the address of a potential peer to the address manager. This RPC is for testing only.\n"

            "\nArguments\n"
            "1. \"address\"     (string, required) The IP address of the peer\n"
            "2. port            (numeric, required) The port of the peer\n"

            "\nResult:\n"
            "{\n"
            "  \"success\": true|false      (boolean) Whether the peer address was successfully added to the address manager\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("addpeeraddress", "\"1.2.3.4\" 47773")
            + HelpExampleRpc("addpeeraddress", "\"1.2.3.4\", 47773"));
    }
    if (!g_connman) {
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }

    UniValue obj(UniValue::VOBJ);

    std::string addr_string = request.params[0].get_str();
    uint16_t port = request.params[1].get_int();

    CNetAddr net_addr;
    if (!LookupHost(addr_string, net_addr, false)) {
        obj.pushKV("success", false);
        return obj;
    }
    CAddress address = CAddress({net_addr, port}, ServiceFlags(NODE_NETWORK));
    address.nTime = GetAdjustedTime();
    // The source address is set equal to the address. This is equivalent to the peer
    // announcing itself.
    if (!g_connman->AddNewAddresses({address}, address)) {
        obj.pushKV("success", false);
        return obj;
    }

    obj.pushKV("success", true);
    return obj;
}

UniValue setnetworkactive(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
                "setnetworkactive \"true|false\"\n"
                "Disable/enable all p2p network activity.\n"

                "\nResult:\n"
                "status    (boolean) The final network activity status\n"
                "\nExamples:\n" +
                HelpExampleCli("setnetworkactive", "true") + HelpExampleRpc("setnetworkactive", "true"));
    }
    if (!g_connman) {
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }
    g_connman->SetNetworkActive(request.params[0].get_bool());
    return g_connman->GetNetworkActive();
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafe argNames
  //  --------------------- ------------------------  -----------------------  ------ --------
    { "network",            "addnode",                &addnode,                true,  {"node","command"} },
    { "network",            "clearbanned",            &clearbanned,            true,  {} },
    { "network",            "disconnectnode",         &disconnectnode,         true,  {"node"} },
    { "network",            "getaddednodeinfo",       &getaddednodeinfo,       true,  {"dummy","node"} },
    { "network",            "getconnectioncount",     &getconnectioncount,     true,  {} },
    { "network",            "getnettotals",           &getnettotals,           true,  {} },
    { "network",            "getnetworkinfo",         &getnetworkinfo,         true,  {} },
    { "network",            "getnodeaddresses",       &getnodeaddresses,       true,  {"count"} },
    { "network",            "getpeerinfo",            &getpeerinfo,            true,  {} },
    { "network",            "listbanned",             &listbanned,             true,  {} },
    { "network",            "ping",                   &ping,                   true,  {} },
    { "network",            "setban",                 &setban,                 true,  {"subnet", "command", "bantime", "absolute"} },
    { "network",            "setnetworkactive",       &setnetworkactive,       true,  {"active"}},

    // Hidden, for testing only
    { "hidden",             "addpeeraddress",         &addpeeraddress,         true,  {"address", "port"} },
};

void RegisterNetRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
