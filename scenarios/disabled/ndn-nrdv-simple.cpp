/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ndvr-app.hpp"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"


namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(NdvrApp);

int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Ptr<Node> node = CreateObject<Node>();

  ndn::StackHelper ndnHelper;
  ndnHelper.Install(node);

  ndn::AppHelper appHelper("NdvrApp");
  appHelper.SetAttribute("Network", StringValue("/ndn"));
  appHelper.SetAttribute("RouterName", StringValue("/\%C1.Router/RouterA"));
  appHelper.Install(node)
    .Start(Seconds(6.5));

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
