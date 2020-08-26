/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

//
// This program configures a grid (default 5x5) of nodes on an
// 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000
// (application) bytes to node 1.
//
// The default layout is like this, on a 2-D grid.
//
// n20  n21  n22  n23  n24
// n15  n16  n17  n18  n19
// n10  n11  n12  n13  n14
// n5   n6   n7   n8   n9
// n0   n1   n2   n3   n4
//
// the layout is affected by the parameters given to GridPositionAllocator;
// by default, GridWidth is 5 and numNodes is 25..
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "ndn-ndvr-wifi-adhoc-grid --help"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the ns-3 documentation.
//
// Common usage:
//
// NS_LOG=ndn.Ndvr:ndn.Ndvr.WifiAdhocGrid ./waf --run "ndn-ndvr-wifi-adhoc-grid --numNodes=9 --distance=50"
//
//
// Other examples:
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when distance increases beyond
// the default of 500m.
// To see this effect, try running:
//
// ./waf --run "ndn-ndvr-wifi-adhoc-grid --distance=500"
// ./waf --run "ndn-ndvr-wifi-adhoc-grid --distance=1000"
// ./waf --run "ndn-ndvr-wifi-adhoc-grid --distance=1500"
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
//
// ./waf --run "ndn-ndvr-wifi-adhoc-grid --verbose=1"
//
// By default, trace file writing is off-- to enable it, try:
// ./waf --run "ndn-ndvr-wifi-adhoc-grid --tracing=1"
//
// When you are done tracing, you will notice many pcap trace files
// in your directory.  If you have tcpdump installed, you can try this:
//
// tcpdump -r ndn-ndvr-wifi-adhoc-grid-0-0.pcap -nn -tt
//
// Inspired in ns-3/examples/wireless/wifi-simple-adhoc-grid.cc

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/ndnSIM-module.h"

#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"
#include "ns3/ndnSIM/NFD/daemon/face/generic-link-service.hpp"
#include "model/ndn-net-device-transport.hpp"
#include "ns3/wifi-module.h"

#include "ndvr-app.hpp"
#include "ndvr-security-helper.hpp"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(NdvrApp);
NS_LOG_COMPONENT_DEFINE ("ndn.Ndvr.WifiAdhocGrid");

std::string
constructFaceUri(Ptr<NetDevice> netDevice)
{
  std::string uri = "netdev://";
  Address address = netDevice->GetAddress();
  if (Mac48Address::IsMatchingType(address)) {
    uri += "[" + boost::lexical_cast<std::string>(Mac48Address::ConvertFrom(address)) + "]";
  }

  return uri;
}

shared_ptr<ndn::Face>
MyWifiNetDeviceCallback(Ptr<Node> node, Ptr<ndn::L3Protocol> ndn,
                                           Ptr<NetDevice> netDevice)
{
  // Create an ndnSIM-specific transport instance
  ::nfd::face::GenericLinkService::Options opts;
  opts.allowFragmentation = true;
  opts.allowReassembly = true;
  opts.allowCongestionMarking = true;

  auto linkService = make_unique<::nfd::face::GenericLinkService>(opts);

  auto transport = make_unique<ndn::NetDeviceTransport>(node, netDevice,
                                                   constructFaceUri(netDevice),
                                                   "netdev://[ff:ff:ff:ff:ff:ff]", /* remote face */
                                                   /* since NetDeviceTransport always send as broadcast, 
                                                    * there is no difference on creating the netdev with 
                                                    * other address than ff:ff:ff:ff:ff:ff as in:
                                                    * ndnSIM/model/ndn-net-device-transport.cpp:121
                                                    * @NetDeviceTransport::doSend()
                                                    *   ...
                                                    *   m_netDevice->Send(ns3Packet, m_netDevice->GetBroadcast(),
                                                    *                     L3Protocol::ETHERNET_FRAME_TYPE);
                                                    *   ...
                                                    * */
                                                   //"netdev://[00:00:00:00:00:01]",
                                                   //"netdev://[01:00:5e:00:17:aa]",
                                                   ::ndn::nfd::FACE_SCOPE_NON_LOCAL,
                                                   ::ndn::nfd::FACE_PERSISTENCY_PERSISTENT,
                                                   ::ndn::nfd::LINK_TYPE_AD_HOC);

  auto face = std::make_shared<ndn::Face>(std::move(linkService), std::move(transport));
  face->setMetric(1);

  ndn->addFace(face);
  NS_LOG_LOGIC("Node " << node->GetId() << ": added Face id=" << face->getId()
                                                   << " uri=" << face->getLocalUri());
  return face;
}

int main (int argc, char *argv[])
{
  std::string phyMode ("DsssRate1Mbps");
  double distance = 500;  // m
  uint32_t numNodes = 25;  // by default, 5x5
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.Parse (argc, argv);

  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));

  NodeContainer nodes;
  nodes.Create (numNodes);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // RxGain/TxGain: simulating a common wireless node with 5dBi omni antenna for indor receiver/transmission (range of 500ft on ideal conditions)
  wifiPhy.Set ("TxGain", DoubleValue (5) ); 
  wifiPhy.Set ("RxGain", DoubleValue (5) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add an upper mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  if (tracing == true)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("ndn-ndvr-wifi-adhoc-grid.tr"));
      wifiPhy.EnablePcap ("ndn-ndvr-wifi-adhoc-grid", devices);
    }


  /***************************************
   * NDN Stack
   **************************************/
  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.AddFaceCreateCallback(WifiNetDevice::GetTypeId(), MakeCallback(MyWifiNetDeviceCallback));
  ndnHelper.InstallAll();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/multicast");

  // Security
  std::string network = "/ufba.br";
  // TODO: the trusted keystore should be a config parameter (see config/validation.conf)
  ::ndn::ndvr::setupRootCert(ndn::Name(network), "config/trust.cert");

  // Install Ndvr App on all nodes
  uint64_t idx = 0;
  for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i, idx++) {
    Ptr<Node> node = *i;    /* This is NS3::Node, not ndn::Node */
    std::string routerName = "/\%C1.Router/Router"+std::to_string(idx);

    ndn::AppHelper appHelper("NdvrApp");
    appHelper.SetAttribute("Network", StringValue(network));
    appHelper.SetAttribute("RouterName", StringValue(routerName));
    appHelper.Install(node)
      .Start(MilliSeconds(1000.0 + 18*idx)); /* XXX: workaround to avoid wireless collisions */

    auto app = DynamicCast<NdvrApp>(node->GetApplication(0));
    app->AddNamePrefix(network + "/depto-" + std::to_string(idx));
    app->AddSigningInfo(::ndn::ndvr::setupSigningInfo(ndn::Name(network + routerName), ndn::Name(network)));
  }


  Simulator::Stop (Seconds (128.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
