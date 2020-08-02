/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "nrdv.hpp"
#include <ns3/simulator.h>
#include <ns3/log.h>
#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/node-list.h>
#include <ns3/ndnSIM/helper/ndn-stack-helper.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.Nrdv");

namespace ndn {
namespace nrdv {

Nrdv::Nrdv(ndn::KeyChain& keyChain, Name network, Name routerName, std::vector<std::string>& npv)
  : m_keyChain(keyChain)
  , m_scheduler(m_face.getIoService())
  , m_seq(0)
  , m_rand(ns3::CreateObject<ns3::UniformRandomVariable>())
  , m_network(network)
  , m_routerName(routerName)
  , m_helloIntervalIni(1)
  , m_helloIntervalCur(1)
  , m_helloIntervalMax(60)
  , m_dvinfoInterval(1)
  , m_dvinfoTimeout(1)
{
  for (std::vector<std::string>::iterator it = npv.begin() ; it != npv.end(); ++it) {
    NamePrefix np(*it, 1, 0);
    m_np[*it] = np;
  }
  m_face.setInterestFilter(kNrdvPrefix, std::bind(&Nrdv::processInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      throw Error("Failed to register sync interest prefix: " + reason);
  });

  buildRouterPrefix();
  registerPrefixes();
  ns3::Simulator::Schedule(ns3::Seconds(1.0), &Nrdv::printFib, this);
}

void Nrdv::Start() {
  SendHelloInterest();
}

void Nrdv::Stop() {
}

void Nrdv::run() {
  m_face.processEvents();
}

void Nrdv::printFib() {
  using namespace ns3;
  using namespace ns3::ndn;

  Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());
  const ::nfd::Fib& fib = thisNode->GetObject<L3Protocol>()->getForwarder()->getFib();
  NS_LOG_DEBUG("FIB Size: " << fib.size());
  for(const ::nfd::fib::Entry& fibEntry : fib) {
    std::string s;
    for (const auto &nh : fibEntry.getNextHops()) s += std::to_string(nh.getFace().getId()) + ",";
    NS_LOG_DEBUG("MyFIB: prefix=" << fibEntry.getPrefix() << " via faceIdList=" << s);
  }
}

void
Nrdv::registerPrefixes() {
  using namespace ns3;
  using namespace ns3::ndn;

  int32_t metric = 0; // should it be 0 or std::numeric_limits<int32_t>::max() ??
  Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());
  NS_LOG_DEBUG("THIS node is: " << thisNode->GetId());

  for (uint32_t deviceId = 0; deviceId < thisNode->GetNDevices(); deviceId++) {
    Ptr<NetDevice> device = thisNode->GetDevice(deviceId);
    Ptr<L3Protocol> ndn = thisNode->GetObject<L3Protocol>();
    NS_ASSERT_MSG(ndn != nullptr, "Ndn stack should be installed on the node");

    auto face = ndn->getFaceByNetDevice(device);
    NS_ASSERT_MSG(face != nullptr, "There is no face associated with the net-device");

    NS_LOG_DEBUG("FibHelper::AddRoute prefix=" << kNrdvPrefix << " via faceId=" << face->getId());
    FibHelper::AddRoute(thisNode, kNrdvPrefix, face, metric);
  }

}

void
Nrdv::SendHelloInterest() {
  Name name = Name(kNrdvHelloPrefix);
  name.append(getRouterPrefix());
  NS_LOG_INFO("Sending Interest " << name);

  Interest interest = Interest();
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setName(name);
  interest.setCanBePrefix(false);
  interest.setInterestLifetime(time::milliseconds(0));

  m_face.expressInterest(interest, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});

  ns3::Simulator::Schedule(ns3::Seconds(m_helloIntervalCur), &Nrdv::SendHelloInterest, this);
}

void
Nrdv::SendDvInfoInterest(NeighborEntry& neighbor) {
  NS_LOG_INFO("Sending DV-Info Interest to neighbor=" << neighbor.GetName());
  Name nameWithSequence = Name(kNrdvDvInfoPrefix);
  nameWithSequence.append(neighbor.GetName());
  nameWithSequence.appendSequenceNumber(neighbor.GetNextVersion());

  Interest interest = Interest();
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setName(nameWithSequence);
  interest.setCanBePrefix(false);
  interest.setInterestLifetime(time::seconds(m_dvinfoTimeout));

  m_face.expressInterest(interest,
    std::bind(&Nrdv::OnDvInfoContent, this, _1, _2),
    std::bind(&Nrdv::OnDvInfoNack, this, _1, _2),
    std::bind(&Nrdv::OnDvInfoTimedOut, this, _1));

  ns3::Simulator::Schedule(ns3::Seconds(m_dvinfoInterval), &Nrdv::SendDvInfoInterest, this, neighbor);
}

void Nrdv::processInterest(const ndn::Interest& interest) {
  const ndn::Name interestName(interest.getName());
  if (kNrdvHelloPrefix.isPrefixOf(interestName))
    return OnHelloInterest(interest);
  else if (kNrdvDvInfoPrefix.isPrefixOf(interestName))
    return OnDvInfoInterest(interest);
  else if (kNrdvKeyPrefix.isPrefixOf(interestName))
    return OnKeyInterest(interest);

  NS_LOG_INFO("Unknown Interest " << interestName);
}

void Nrdv::OnHelloInterest(const ndn::Interest& interest) {
  const ndn::Name interestName(interest.getName());
  std::string routerTag = "\%C1.Router";
  std::string neighName = ExtractNeighborNameFromHello(interestName);

  NS_LOG_INFO("Received HELLO Interest " << interestName);
  //NS_LOG_DEBUG("Nrdv::Hello - Received cmdMarker: " << ExtractRouterTagFromHello(interestName));
  //NS_LOG_DEBUG("Nrdv::Hello - Received neighName: " << neighName);
  //NS_LOG_DEBUG("Nrdv::Hello - Expected cmdMarker: " << routerTag);
  //NS_LOG_DEBUG("Nrdv::Hello - My name: " << m_routerName);

  if (ExtractRouterTagFromHello(interestName) != routerTag) {
    NS_LOG_INFO("Not a router, ignoring...");
    return;
  }
  if (neighName == m_routerName) {
    NS_LOG_INFO("Hello from myself, ignoring...");
    return;
  }
  if (m_neighMap.count(neighName)) {
    NS_LOG_INFO("Already known router, ignoring...");
    // exponetially increase the hellInterval until the maximum allowed
    m_helloIntervalCur = (2*m_helloIntervalCur > m_helloIntervalMax) ? m_helloIntervalMax : 2*m_helloIntervalCur;
    return;
  }
  m_helloIntervalCur = m_helloIntervalIni;
  NeighborEntry neigh(neighName, 0);
  m_neighMap[neighName] = neigh;
  SendDvInfoInterest(neigh);
}

void Nrdv::OnDvInfoInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received DV-Info Interest " << interest.getName());

  // TODO: send our DV-Info
  auto data = std::make_shared<ndn::Data>(interest.getName());
  data->setFreshnessPeriod(ndn::time::milliseconds(1000));
  data->setContent(std::make_shared< ::ndn::Buffer>(1024));
  m_keyChain.sign(*data);
  m_face.put(*data);
}

void Nrdv::OnKeyInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received KEY Interest " << interest.getName());
  // TODO: send our key??
}

void Nrdv::OnDvInfoTimedOut(const ndn::Interest& interest) {
  NS_LOG_DEBUG("Interest timed out for Name: " << interest.getName());
  // TODO: Apply the same logic as in HelloProtocol::processInterestTimedOut (~/mini-ndn/ndn-src/NLSR/src/hello-protocol.cpp)
  // TODO: what if node has moved?
}

void Nrdv::OnDvInfoNack(const ndn::Interest& interest, const ndn::lp::Nack& nack) {
  NS_LOG_DEBUG("Received Nack with reason: " << nack.getReason());
  // should we treat as a timeout? should the Nack represent no changes on neigh DvInfo?
  //m_scheduler.schedule(ndn::time::seconds(m_dvinfoInterval),
  //  [this, interest] { processInterestTimedOut(interest); });
}

void Nrdv::OnDvInfoContent(const ndn::Interest& interest, const ndn::Data& data) {
  NS_LOG_DEBUG("Received content for DV-Info: " << data.getName());

  if (data.getSignature().hasKeyLocator() &&
      data.getSignature().getKeyLocator().getType() == ndn::tlv::Name) {
    NS_LOG_DEBUG("Data signed with: " << data.getSignature().getKeyLocator().getName());
  }

  // TODO: validate data as in HelloProtocol::onContent (~/mini-ndn/ndn-src/NLSR/src/hello-protocol.cpp)
}

} // namespace nrdv
} // namespace ndn
