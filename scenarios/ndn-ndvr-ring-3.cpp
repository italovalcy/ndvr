/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ndvr-app.hpp"
#include "ndvr-security-helper.hpp"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-module.h"


namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(NdvrApp);

int
main(int argc, char* argv[])
{
  uint32_t duration = 200;
  bool enableDSK = false;
  bool enablePcap = false;
  std::string keyType = "e"; // default EDCSA

  CommandLine cmd;
  cmd.AddValue ("duration", "duration (s)", duration);
  cmd.AddValue ("enableDSK", "enable DSK", enableDSK);
  cmd.AddValue ("enablePcap", "enable Pcap", enablePcap);
  cmd.AddValue ("keyType", "key type", keyType);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create (3);

  /*
   *  n0 -- n1
   *   \   /
   *     n2
   **/
  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(1), nodes.Get(2));
  p2p.Install(nodes.Get(2), nodes.Get(0));

  // 3. Install NDN stack
  ndn::StackHelper ndnHelper;
  ndnHelper.Install(nodes);
  if (enablePcap) {
    p2p.EnablePcapAll ("capture-p2p");
  }

  // 4. Set Forwarding Strategy
  ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/multicast");

  // 4.1 Security
  std::string network = "/ndn";
  // TODO: the trusted keystore should be a config parameter (see config/validation.conf)
  ::ndn::ndvr::setupRootCert(ndn::Name(network), "config/trust.cert");

  // 5. Install NDN Apps (Ndvr)
  uint64_t idx = 0;
  for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i, idx++) {
    Ptr<Node> node = *i;    /* This is NS3::Node, not ndn::Node */
    std::string routerName = "/\%C1.Router/Router"+std::to_string(idx);

    std::cout << "node " << idx << " RouterName: " << routerName << std::endl;

    ndn::AppHelper appHelper("NdvrApp");
    appHelper.SetAttribute("Network", StringValue("/ndn"));
    appHelper.SetAttribute("RouterName", StringValue(routerName));
    appHelper.Install(node)
      .Start(Seconds(1.0));

    auto app = DynamicCast<NdvrApp>(node->GetApplication(0));
    app->AddNamePrefix("/ndn/site-"+std::to_string(idx));
    app->AddNamePrefix("/ndn/xpto-"+std::to_string(idx));
    app->AddSigningInfo(::ndn::ndvr::setupSigningInfo(ndn::Name(network + routerName), ndn::Name(network)));
    if (enableDSK) {
      app->EnableDSKMaxSecs(60);
    }

    app->SetKeyType(keyType);
  }

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
