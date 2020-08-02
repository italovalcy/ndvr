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

Nrdv::Nrdv(ndn::KeyChain& keyChain, Name network, Name routerName)
  : m_keyChain(keyChain)
  , m_scheduler(m_face.getIoService())
  , m_seq(0)
  , m_rand(ns3::CreateObject<ns3::UniformRandomVariable>())
  , m_network(network)
  , m_routerName(routerName)
  , m_helloIntervalIni(1)
  , m_helloIntervalCur(1)
  , m_helloIntervalMax(60)
{
  m_face.setInterestFilter(kNrdvPrefix, std::bind(&Nrdv::processInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      throw Error("Failed to register sync interest prefix: " + reason);
  });

  buildRouterPrefix();
  registerPrefixes();
  ns3::Simulator::Schedule(ns3::Seconds(1.0), &Nrdv::printFib, this);

  // use scheduler to send interest later on consumer face
  //m_scheduler.schedule(ndn::time::seconds(2), [this] {
  //    m_face.expressInterest(ndn::Interest("/nrdv/helloworld"),
  //                                   std::bind([] { std::cout << "Hello!" << std::endl; }),
  //                                   std::bind([] { std::cout << "NACK!" << std::endl; }),
  //                                   std::bind([] { std::cout << "Bye!.." << std::endl; }));
  //  });
}

void Nrdv::Start() {
  SendHello();
}

void Nrdv::Stop() {
}

void Nrdv::run() {
  m_face.processEvents(); // ok (will not block and do nothing)
  // m_faceConsumer.getIoService().run(); // will crash
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
Nrdv::ScheduleNextHello() {
  using namespace ns3;
  Simulator::Schedule(Seconds(m_helloIntervalCur), &Nrdv::SendHello, this);
}

void
Nrdv::SendHello() {
  Name nameWithSequence = Name(kNrdvHelloPrefix);
  nameWithSequence.append(getRouterPrefix());
  // TODO: hello should not include the version
  //nameWithSequence.appendSequenceNumber(m_seq++);


  Interest interest = Interest();
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setName(nameWithSequence);
  interest.setCanBePrefix(false);
  interest.setInterestLifetime(time::milliseconds(0));

  NS_LOG_INFO("Sending Interest " << nameWithSequence);

  m_face.expressInterest(interest, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});

  ScheduleNextHello();
}

void Nrdv::processInterest(const ndn::Interest& interest) {
  const ndn::Name interestName(interest.getName());
  if (kNrdvHelloPrefix.isPrefixOf(interestName))
    return OnHelloInterest(interest);
  else if (kNrdvDataPrefix.isPrefixOf(interestName))
    return OnDataInterest(interest);
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
}
void Nrdv::OnDataInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received DATA Interest " << interest.getName());
  auto data = std::make_shared<ndn::Data>(interest.getName());
  data->setFreshnessPeriod(ndn::time::milliseconds(1000));
  data->setContent(std::make_shared< ::ndn::Buffer>(1024));
  m_keyChain.sign(*data);
  m_face.put(*data);
}

void Nrdv::OnKeyInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received KEY Interest " << interest.getName());
}

} // namespace nrdv
} // namespace ndn
