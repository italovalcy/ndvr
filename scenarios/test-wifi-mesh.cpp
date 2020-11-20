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
  int numNodes = 10;
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
  positionAlloc->Add (Vector (  0.0,  0.0, 0.0));
  positionAlloc->Add (Vector ( 10.0,  5.0, 0.0));
  positionAlloc->Add (Vector ( 20.0, 10.0, 0.0));
  positionAlloc->Add (Vector ( 20.0, 20.0, 0.0));
  positionAlloc->Add (Vector ( 10.0, 25.0, 0.0));
  positionAlloc->Add (Vector (  0.0, 30.0, 0.0));
  positionAlloc->Add (Vector (-10.0, 25.0, 0.0));
  positionAlloc->Add (Vector (-20.0, 20.0, 0.0));
  positionAlloc->Add (Vector (-20.0, 10.0, 0.0));
  positionAlloc->Add (Vector (-10.0,  5.0, 0.0));
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
    appHelper.SetAttribute("SyncDataRounds", IntegerValue(syncDataRounds));
    appHelper.Install(node).Start(MilliSeconds(10*idx));
    auto app = DynamicCast<NdvrApp>(node->GetApplication(0));
    app->AddSigningInfo(::ndn::ndvr::setupSigningInfo(ndn::Name(network + routerName), ndn::Name(network)));

    // Producer
    ndn::Name namePrefix("/ndn/ndvrSync");
    namePrefix.appendNumber(idx);
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetPrefix(namePrefix.toUri());
    producerHelper.SetAttribute("PayloadSize", StringValue("300"));
    /* Since some nodes stay disconnected for a long period of time, 
     * we increase the TTL so the CS can still be useful */
    producerHelper.SetAttribute("Freshness", TimeValue(Seconds(3600.0)));
    producerHelper.Install(node);
  }

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
