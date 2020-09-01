// consumer-oninterest.cpp

#include "consumer-oninterest.hpp"

#include "ns3/core-module.h"
#include "ns3/node-list.h"
#include "ns3/log.h"
#include "ns3/ndnSIM/helper/ndn-fib-helper.hpp"

NS_LOG_COMPONENT_DEFINE("ConsumerOnInterest");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(ConsumerOnInterest);

TypeId ConsumerOnInterest::GetTypeId() {
  static TypeId tid = TypeId("ConsumerOnInterest")
    .SetParent<Application>()
    .AddConstructor<ConsumerOnInterest>()
    .AddAttribute("InterestName", "Interest Name that will trigger the consumer", StringValue(""),
                  ndn::MakeNameAccessor(&ConsumerOnInterest::interestName_), ndn::MakeNameChecker());

  return tid;
}

ConsumerOnInterest::ConsumerOnInterest() {
}

void ConsumerOnInterest::StartApplication() {
  App::StartApplication();
  Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());
  NS_LOG_DEBUG("THIS node is: " << thisNode->GetId());
  //ndn::FibHelper::AddRoute(GetNode(), interestName_, m_face, 0);
  //Config::ConnectWithoutContext("/NodeList/" + std::to_string(thisNode->GetId()) + "/ApplicationList/*/NdvrAddRoute",
  Config::ConnectWithoutContext("/NodeList/" + std::to_string(thisNode->GetId()) + "/$ns3::NdvrRoutingTable/NdvrAddRoute",
                                MakeCallback(&ConsumerOnInterest::NdvrAddRoute, this));
}

void ConsumerOnInterest::StopApplication() {
  App::StopApplication();
}

void ConsumerOnInterest::OnInterest(std::shared_ptr<const ndn::Interest> interest) {
  NS_LOG_DEBUG("Incoming interest for" << interest->getName());
}

void ConsumerOnInterest::NdvrAddRoute(const std::string& namePrefix) {
  NS_LOG_DEBUG("New route to namePrefix " << namePrefix);
}

} // namespace ns3
