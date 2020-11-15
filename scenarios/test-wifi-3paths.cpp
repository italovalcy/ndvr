/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ndvr-app.hpp"
#include "ndvr-security-helper.hpp"
#include "wifi-adhoc-helper.hpp"
#include "admit-localhop-unsolicited-data-policy.hpp"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(NdvrApp);

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
  int numNodes = 30;
  int range = 60;
  std::string traceFile;
  double distance = 800;
  uint32_t syncDataRounds = 0;
  bool tracing = false;

  CommandLine cmd;
  //cmd.AddValue("numNodes", "numNodes", numNodes);
  cmd.AddValue("wifiRange", "the wifi range", range);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("syncDataRounds", "number of rounds to run the publish / sync Data", syncDataRounds);
  cmd.AddValue("run", "run number", run);
  cmd.AddValue("tracing", "enable wifi tracing", tracing);
  cmd.Parse(argc, argv);
  RngSeedManager::SetRun (run);
  /* each sync data round interval is ~40s and we give more 3x more time to finish the sync process, plus extra 128s */
  uint32_t sim_time = 128 + syncDataRounds*40*3;

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
  positionAlloc->Add (Vector ( 70.0, 210.0, 0.0));
  positionAlloc->Add (Vector ( 70.0, 160.0, 0.0));
  positionAlloc->Add (Vector ( 80.0, 110.0, 0.0));
  positionAlloc->Add (Vector (120.0,  70.0, 0.0));
  positionAlloc->Add (Vector (170.0,  70.0, 0.0));
  positionAlloc->Add (Vector (220.0,  70.0, 0.0));
  positionAlloc->Add (Vector (260.0,  68.0, 0.0));
  positionAlloc->Add (Vector (300.0,  90.0, 0.0));
  positionAlloc->Add (Vector (340.0, 110.0, 0.0));
  positionAlloc->Add (Vector (350.0, 160.0, 0.0));
  positionAlloc->Add (Vector ( 70.0, 260.0, 0.0));
  positionAlloc->Add (Vector ( 70.0, 310.0, 0.0));
  positionAlloc->Add (Vector ( 70.0, 350.0, 0.0));
  positionAlloc->Add (Vector ( 70.0, 400.0, 0.0));
  positionAlloc->Add (Vector (100.0, 440.0, 0.0));
  positionAlloc->Add (Vector (150.0, 440.0, 0.0));
  positionAlloc->Add (Vector (190.0, 440.0, 0.0));
  positionAlloc->Add (Vector (230.0, 440.0, 0.0));
  positionAlloc->Add (Vector (270.0, 440.0, 0.0));
  positionAlloc->Add (Vector (320.0, 430.0, 0.0));
  positionAlloc->Add (Vector (350.0, 390.0, 0.0));
  positionAlloc->Add (Vector (350.0, 350.0, 0.0));
  positionAlloc->Add (Vector (350.0, 310.0, 0.0));
  positionAlloc->Add (Vector (360.0, 260.0, 0.0));
  positionAlloc->Add (Vector (120.0, 210.0, 0.0));
  positionAlloc->Add (Vector (170.0, 210.0, 0.0));
  positionAlloc->Add (Vector (220.0, 210.0, 0.0));
  positionAlloc->Add (Vector (260.0, 210.0, 0.0));
  positionAlloc->Add (Vector (310.0, 210.0, 0.0));
  positionAlloc->Add (Vector (350.0, 210.0, 0.0));
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
  /* NFD add-nexthop Data (ContentResponse) is also saved on the Cs, so
   * this workaround gives us more space for real data (see also the
   * Freshness attr below) */
  ndnHelper.setCsSize(1000);
  ndnHelper.Install(nodes);

  // 4. Set Forwarding Strategy
  ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/multicast");
  ndn::StrategyChoiceHelper::Install(nodes, "/localhop/ndvr", "/localhost/nfd/strategy/localhop");

  // Security - create root cert (to be used as trusted anchor later)
  std::string network = "/ndn";
  ::ndn::ndvr::setupRootCert(ndn::Name(network), "config/trust.cert");

  // 5. Install NDN Apps (Ndvr)
  uint64_t idx = 0;
  for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i, idx++) {
    Ptr<Node> node = *i;    /* This is NS3::Node, not ndn::Node */

    // change the unsolicited data policy to save in cache localhop messages
    setUnsolicitedDataPolicy(node);

    // NDVR
    ndn::AppHelper appHelper("NdvrApp");
    appHelper.SetAttribute("Network", StringValue("/ndn"));
    std::string routerName = "/\%C1.Router/Router"+std::to_string(idx);
    appHelper.SetAttribute("RouterName", StringValue(routerName));
    appHelper.Install(node).Start(MilliSeconds(10*idx));
    auto app = DynamicCast<NdvrApp>(node->GetApplication(0));
    app->AddSigningInfo(::ndn::ndvr::setupSigningInfo(ndn::Name(network + routerName), ndn::Name(network)));
    if (idx==29)
      app->AddNamePrefix("/dataSync");

  }
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix("/dataSync");
  producerHelper.SetAttribute("PayloadSize", StringValue("1000"));
  producerHelper.Install(nodes.Get(29));

  ndn::AppHelper c0("ns3::ndn::ConsumerCbr");
  c0.SetPrefix("/dataSync");
  c0.SetAttribute("Frequency", StringValue("10")); // 10 interests per second
  c0.Install(nodes.Get(0)).Start(Seconds(10.0));

  // Trace Collisions
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&MacTxDrop));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback(&PhyRxDrop));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback(&PhyTxDrop));
  Simulator::Schedule(Seconds(sim_time - 5), &PrintDrop);

  Simulator::Stop(Seconds(sim_time));

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
