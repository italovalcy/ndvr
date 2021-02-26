/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <math.h>

#include "ndvr-app.hpp"
#include "simplepubsub-app.hpp"
#include "ndvr-security-helper.hpp"
#include "wifi-adhoc-helper.hpp"
#include "admit-localhop-unsolicited-data-policy.hpp"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

namespace ns3 {

uint32_t MacTxDropCount, PhyTxDropCount, PhyRxDropCount;
//AsciiTraceHelper phydropascii;
//Ptr<OutputStreamWrapper> stream = phydropascii.CreateFileStream ("phyrxdrop.tr");

void setUnsolicitedDataPolicy(Ptr<Node> node) {
  unique_ptr<::nfd::fw::UnsolicitedDataPolicy> unsolicitedDataPolicy;
  unsolicitedDataPolicy = ::nfd::fw::UnsolicitedDataPolicy::create("admit-localhop");
  node->GetObject<ndn::L3Protocol>()->getForwarder()->setUnsolicitedDataPolicy(std::move(unsolicitedDataPolicy));
}

void
MacTxDrop(Ptr<const Packet> p)
{
  //NS_LOG_INFO("Packet Drop");
  MacTxDropCount++;
}

void
PhyTxDrop(Ptr<const Packet> p)
{
  //NS_LOG_INFO("Packet Drop");
  PhyTxDropCount++;
}
void
PhyRxDrop(Ptr<const Packet> p, WifiPhyRxfailureReason r)
{
  //NS_LOG_INFO("Packet Drop");
  //*stream->GetStream () << Simulator::Now().GetSeconds() << " PhyRxDrop pkt=" << *p << " reason=" << r << std::endl;
  PhyRxDropCount++;
}

void
PrintDrop()
{
  std::cout << Simulator::Now().GetSeconds() << "\t PktDropStats MacTxDrop=" << MacTxDropCount << "\t PhyTxDrop="<< PhyTxDropCount << "\t PhyRxDrop=" << PhyRxDropCount << "\n";
}


int
main(int argc, char* argv[])
{
  std::string phyMode("DsssRate1Mbps");
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));
  int run = 0;
  int numNodes = 20;
  int range = 60;
  std::string traceFile;
  double distance = 800;
  uint32_t duration = 100;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("numNodes", "numNodes", numNodes);
  cmd.AddValue("wifiRange", "the wifi range", range);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("duration", "duration (s)", duration);
  cmd.AddValue("run", "run number", run);
  cmd.AddValue("traceFile", "Ns2 movement trace file", traceFile);
  cmd.AddValue("tracing", "enable wifi tracing", tracing);
  cmd.Parse(argc, argv);
  RngSeedManager::SetRun (run);

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

  // Mobility model
  if (!traceFile.empty()) {
    Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);
    ns2.Install ();
  } else {
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (distance),
                                   "DeltaY", DoubleValue (distance),
                                   "GridWidth", UintegerValue (ceil(sqrt(numNodes))),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);
  }
  if (tracing == true) {
    AsciiTraceHelper ascii;
    wifiPhyHelper.EnableAsciiAll (ascii.CreateFileStream ("wifi-tracing.tr"));
  }


  // 3. Install NDN stack
  ndn::StackHelper ndnHelper;
  ndnHelper.AddFaceCreateCallback(WifiNetDevice::GetTypeId(), MakeCallback(FixLinkTypeAdhocCb));
  /* NFD add-nexthop Data (ContentResponse) is also saved on the Cs, so
   * this workaround gives us more space for real data (see also the
   * Freshness attr below) */
  ndnHelper.setCsSize(1000);
  //ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(nodes);

  // 4. Set Forwarding Strategy
  ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/best-route");
  //ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/m-asf");
  ndn::StrategyChoiceHelper::Install(nodes, "/localhop/ndvr", "/localhost/nfd/strategy/localhop");

  // Security - create root cert (to be used as trusted anchor later)
  std::string network = "/ndn";
  ::ndn::ndvr::setupRootCert(ndn::Name(network), "config/trust.cert");

  // 5. Install NDN Apps (Ndvr)
  uint64_t idx = 0;
  for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i, idx++) {
    Ptr<Node> node = *i;    /* This is NS3::Node, not ndn::Node */
    std::string routerName = "/\%C1.Router/Router"+std::to_string(idx);
    ndn::Name namePrefix("/ndn/ndvrSync");
    namePrefix.appendNumber(idx);

    // change the unsolicited data policy to save in cache localhop messages
    setUnsolicitedDataPolicy(node);

    // NDVR
    ndn::AppHelper ndvrApp("NdvrApp");
    ndvrApp.SetAttribute("Network", StringValue("/ndn"));
    ndvrApp.SetAttribute("RouterName", StringValue(routerName));
    ndvrApp.SetAttribute("EnableUnicastFace", BooleanValue(true));
    ndvrApp.Install(node).Start(MilliSeconds(10*idx));
    auto app = DynamicCast<NdvrApp>(node->GetApplication(0));
    app->AddSigningInfo(::ndn::ndvr::setupSigningInfo(ndn::Name(network + routerName), ndn::Name(network)));
    app->AddNamePrefix(namePrefix.toUri());

    // Producer
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetPrefix(namePrefix.toUri());
    producerHelper.SetAttribute("PayloadSize", StringValue("300"));
    /* Since some nodes stay disconnected for a long period of time, 
     * we increase the TTL so the CS can still be useful */
    producerHelper.SetAttribute("Freshness", TimeValue(Seconds(3600.0)));
    producerHelper.Install(node);

    // Consumer
    ndn::AppHelper appHelper("RangeConsumerApp");
    appHelper.SetAttribute("RangePrefix", StringValue("/ndn/ndvrSync"));
    appHelper.SetAttribute("RangeFirst", IntegerValue(0));
    appHelper.SetAttribute("RangeLast", IntegerValue(numNodes -1));
    appHelper.SetAttribute("Frequency", IntegerValue(10));
    appHelper.Install(node).Start(MilliSeconds(1000+10*idx));
  }

  // Trace Collisions
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&MacTxDrop));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback(&PhyRxDrop));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback(&PhyTxDrop));
  Simulator::Schedule(Seconds(duration - 1), &PrintDrop);

  Simulator::Stop(Seconds(duration));

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
