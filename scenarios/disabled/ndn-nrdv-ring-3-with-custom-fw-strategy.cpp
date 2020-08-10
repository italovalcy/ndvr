/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ndvr-app.hpp"
#include "ndvr-strategy.hpp"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-module.h"


namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(NdvrApp);

int
main(int argc, char* argv[])
{
  CommandLine cmd;
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

  // 4. Set Forwarding Strategy
  ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/multicast");
  ndn::StrategyChoiceHelper::Install<nfd::fw::NdvrMulticastStrategy>(nodes, "/ndvr/hello");

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
  }

  Simulator::Stop(Seconds(20.0));

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
