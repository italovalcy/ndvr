/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "wifi-adhoc-helper.hpp"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

namespace ns3 {

int
main(int argc, char* argv[])
{
  std::string phyMode("DsssRate1Mbps");
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));
  int numNodes = 3;
  int range = 60;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("tracing", "enable wifi tracing", tracing);
  cmd.Parse(argc, argv);

  ////////////////////////////////
  // Wi-Fi begin
  ////////////////////////////////
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel",
                                  "MaxRange", DoubleValue(range));

  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default ();
  wifiPhyHelper.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMacHelper;
  wifiMacHelper.SetType("ns3::AdhocWifiMac");
  ////////////////////////////////
  // Wi-Fi end
  ////////////////////////////////

  NodeContainer nodes;
  nodes.Create (numNodes);

  NetDeviceContainer wifiNetDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, nodes);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector ( 10.0, 10.0, 0.0));
  positionAlloc->Add (Vector ( 20.0, 10.0, 0.0));
  positionAlloc->Add (Vector ( 15.0, 20.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  if (tracing == true) {
    AsciiTraceHelper ascii;
    wifiPhyHelper.EnableAsciiAll (ascii.CreateFileStream ("wifi-tracing.tr"));
  }


  // 3. Install NDN stack
  ndn::StackHelper ndnHelper;
  ndnHelper.AddFaceCreateCallback(WifiNetDevice::GetTypeId(), MakeCallback(FixLinkTypeAdhocCb));
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(nodes);

  // 4. Set Forwarding Strategy
  ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/multicast");
  ndn::StrategyChoiceHelper::Install(nodes, "/localhop/Router2", "/localhost/nfd/strategy/localhop");

  // 5. Data application
  ndn::AppHelper p2("ns3::ndn::Producer");
  p2.SetPrefix("/localhop/Router2");
  p2.SetAttribute("PayloadSize", StringValue("1024"));
  p2.Install(nodes.Get(2));

  ndn::AppHelper c0("ns3::ndn::ConsumerCbr");
  c0.SetPrefix("/localhop/Router2");
  c0.SetAttribute("Frequency", StringValue("0.5")); // 1 interest every two seconds
  c0.Install(nodes.Get(0)).Start(Seconds(1.0));

  ndn::AppHelper c1("ns3::ndn::ConsumerCbr");
  c1.SetPrefix("/localhop/Router2");
  c1.SetAttribute("Frequency", StringValue("0.5")); // 1 interest every two seconds
  c1.Install(nodes.Get(1)).Start(Seconds(1.000005));

  Simulator::Stop(Seconds(2.5));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
