/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ndvr.hpp"
#include <limits>
#include <ns3/simulator.h>
#include <ns3/log.h>
#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/node-list.h>
#include <ns3/ndnSIM/helper/ndn-stack-helper.hpp>
#include <ndn-cxx/lp/tags.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.Ndvr");

namespace ndn {
namespace ndvr {

Ndvr::Ndvr(ndn::KeyChain& keyChain, Name network, Name routerName, std::vector<std::string>& npv)
  : m_keyChain(keyChain)
  , m_scheduler(m_face.getIoService())
  , m_seq(0)
  , m_rand(ns3::CreateObject<ns3::UniformRandomVariable>())
  , m_network(network)
  , m_routerName(routerName)
  , m_helloIntervalIni(1)
  , m_helloIntervalCur(1)
  , m_helloIntervalMax(60)
  , m_localRTInterval(1)
  , m_localRTTimeout(1)
{
  for (std::vector<std::string>::iterator it = npv.begin() ; it != npv.end(); ++it) {
    RoutingEntry routingEntry;
    routingEntry.SetName(*it);
    routingEntry.SetSeqNum(1);
    routingEntry.SetCost(0);
    routingEntry.SetFaceId(0); /* directly connected */
    m_routingTable.insert(routingEntry);
  }
  m_face.setInterestFilter(kNdvrPrefix, std::bind(&Ndvr::processInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      throw Error("Failed to register sync interest prefix: " + reason);
  });

  buildRouterPrefix();
  registerPrefixes();
  m_scheduler.schedule(ndn::time::seconds(1),
                        [this] { printFib(); });
}

void Ndvr::Start() {
  SendHelloInterest();
}

void Ndvr::Stop() {
}

void Ndvr::run() {
  m_face.processEvents();
}

void Ndvr::printFib() {
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
Ndvr::registerPrefixes() {
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

    NS_LOG_DEBUG("FibHelper::AddRoute prefix=" << kNdvrPrefix << " via faceId=" << face->getId());
    FibHelper::AddRoute(thisNode, kNdvrPrefix, face, metric);
  }

}

void
Ndvr::SendHelloInterest() {
  /* First of all, cancel any previously scheduled events */
  sendhello_event.cancel();

  Name name = Name(kNdvrHelloPrefix);
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

  sendhello_event = m_scheduler.schedule(time::seconds(m_helloIntervalCur),
                                        [this] { SendHelloInterest(); });
}

void
Ndvr::SendDvInfoInterest(NeighborEntry& neighbor) {
  NS_LOG_INFO("Sending DV-Info Interest to neighbor=" << neighbor.GetName());
  Name nameWithSequence = Name(kNdvrDvInfoPrefix);
  nameWithSequence.append(neighbor.GetName());
  nameWithSequence.appendSequenceNumber(neighbor.GetNextVersion());

  Interest interest = Interest();
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setName(nameWithSequence);
  interest.setCanBePrefix(false);
  interest.setInterestLifetime(time::seconds(m_localRTTimeout));

  m_face.expressInterest(interest,
    std::bind(&Ndvr::OnDvInfoContent, this, _1, _2),
    std::bind(&Ndvr::OnDvInfoNack, this, _1, _2),
    std::bind(&Ndvr::OnDvInfoTimedOut, this, _1));

  // Not necessary anymore, since the DvInfo is sent on demand (when receiving an ehlo)
  //ns3::Simulator::Schedule(ns3::Seconds(m_localRTInterval), &Ndvr::SendDvInfoInterest, this, neighbor);
}

void Ndvr::processInterest(const ndn::Interest& interest) {
  /** Incoming Face Indication
   * NDNLPv2 says "Incoming face indication feature allows the forwarder to inform local applications
   * about the face on which a packet is received." and also warns "application MUST be prepared to
   * receive a packet without IncomingFaceId field". From our tests, only internal faces seems to not
   * initialize it and this can even be used to filter out our own interests. See more:
   *  - FibManager::setFaceForSelfRegistration (ndnSIM/NFD/daemon/mgmt/fib-manager.cpp)
   *  - https://redmine.named-data.net/projects/nfd/wiki/NDNLPv2#Incoming-Face-Indication
   */
  shared_ptr<lp::IncomingFaceIdTag> incomingFaceIdTag = interest.getTag<lp::IncomingFaceIdTag>();
  if (incomingFaceIdTag == nullptr) {
    //NS_LOG_DEBUG("Discarding Interest from internal face: " << interest);
    return;
  }
  uint64_t inFaceId = *incomingFaceIdTag;
  //NS_LOG_INFO("Interest: " << interest << " inFaceId=" << inFaceId);

  const ndn::Name interestName(interest.getName());
  if (kNdvrHelloPrefix.isPrefixOf(interestName))
    return OnHelloInterest(interest, inFaceId);
  else if (kNdvrDvInfoPrefix.isPrefixOf(interestName))
    return OnDvInfoInterest(interest);
  else if (kNdvrKeyPrefix.isPrefixOf(interestName))
    return OnKeyInterest(interest);

  NS_LOG_INFO("Unknown Interest " << interestName);
}

void Ndvr::IncreaseHelloInterval() {
  /* exponetially increase the helloInterval until the maximum allowed */
  if (increasehellointerval_event)
    return;
  increasehellointerval_event = m_scheduler.schedule(time::seconds(1),
      [this] {
        m_helloIntervalCur = (2*m_helloIntervalCur > m_helloIntervalMax) ? m_helloIntervalMax : 2*m_helloIntervalCur;
      });
}

void Ndvr::ResetHelloInterval() {
  increasehellointerval_event.cancel();
  m_helloIntervalCur = m_helloIntervalIni;
}

void Ndvr::OnHelloInterest(const ndn::Interest& interest, uint64_t inFaceId) {
  const ndn::Name interestName(interest.getName());
  NS_LOG_INFO("Received HELLO Interest " << interestName);

  std::string neighPrefix = ExtractRouterPrefix(interestName, kNdvrHelloPrefix);
  if (!isValidRouter(interestName, kNdvrHelloPrefix)) {
    NS_LOG_INFO("Not a router, ignoring...");
    return;
  }
  if (neighPrefix == m_routerPrefix) {
    NS_LOG_INFO("Hello from myself, ignoring...");
    return;
  }
  auto neigh = m_neighMap.find(neighPrefix);
  if (neigh == m_neighMap.end()) {
    ResetHelloInterval();
    neigh = m_neighMap.emplace(neighPrefix, NeighborEntry(neighPrefix, inFaceId, 0)).first;
  } else {
    NS_LOG_INFO("Already known router, increasing the hello interval");
    IncreaseHelloInterval();
    //return;
  }
  SendDvInfoInterest(neigh->second);
}

void Ndvr::OnDvInfoInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received DV-Info Interest " << interest.getName());

  // Sanity check
  std::string routerPrefix = ExtractRouterPrefix(interest.getName(), kNdvrDvInfoPrefix);
  if (routerPrefix != m_routerPrefix) {
    //NS_LOG_INFO("Interest is not to me, ignoring.. received_name=" << routerPrefix << " my_name=" << m_routerPrefix);
    return;
  }

  // TODO: send our DV-Info
  auto data = std::make_shared<ndn::Data>(interest.getName());
  data->setFreshnessPeriod(ndn::time::milliseconds(1000));
  // Set dvinfo
  std::string dvinfo_str;
  EncodeDvInfo(m_routingTable, dvinfo_str);
  NS_LOG_INFO("Replying DV-Info with encoded data: size=" << dvinfo_str.size());
  //NS_LOG_INFO("Sending DV-Info encoded: str=" << dvinfo_str);
  data->setContent(reinterpret_cast<const uint8_t*>(dvinfo_str.data()), dvinfo_str.size());
  // Sign and send
  m_keyChain.sign(*data);
  m_face.put(*data);
}

void Ndvr::OnKeyInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received KEY Interest " << interest.getName());
  // TODO: send our key??
}

void Ndvr::OnDvInfoTimedOut(const ndn::Interest& interest) {
  NS_LOG_DEBUG("Interest timed out for Name: " << interest.getName());
  // TODO: Apply the same logic as in HelloProtocol::processInterestTimedOut (~/mini-ndn/ndn-src/NLSR/src/hello-protocol.cpp)
  // TODO: what if node has moved?
}

void Ndvr::OnDvInfoNack(const ndn::Interest& interest, const ndn::lp::Nack& nack) {
  NS_LOG_DEBUG("Received Nack with reason: " << nack.getReason());
  // should we treat as a timeout? should the Nack represent no changes on neigh DvInfo?
  //m_scheduler.schedule(ndn::time::seconds(m_localRTInterval),
  //  [this, interest] { processInterestTimedOut(interest); });
}

void Ndvr::OnDvInfoContent(const ndn::Interest& interest, const ndn::Data& data) {
  NS_LOG_DEBUG("Received content for DV-Info: " << data.getName());

  /* Sanity checks */
  std::string neighPrefix = ExtractRouterPrefix(data.getName(), kNdvrDvInfoPrefix);
  if (!isValidRouter(data.getName(), kNdvrDvInfoPrefix)) {
    NS_LOG_INFO("Not a router, ignoring...");
    return;
  }
  if (neighPrefix == m_routerPrefix) {
    NS_LOG_INFO("DvInfo from myself, ignoring...");
    return;
  }

  /* Overheard DvInfo */
  auto neigh_it = m_neighMap.find(neighPrefix);
  if (neigh_it == m_neighMap.end()) {
    NS_LOG_INFO("Overheard DvInfo!");
    // TODO: mark some flag for future use
    // TODO: insert on the neighborMap?
  }

  /* Security validation */
  if (data.getSignature().hasKeyLocator() &&
      data.getSignature().getKeyLocator().getType() == ndn::tlv::Name) {
    NS_LOG_DEBUG("Data signed with: " << data.getSignature().getKeyLocator().getName());
  }
  // TODO: validate data as in HelloProtocol::onContent (~/mini-ndn/ndn-src/NLSR/src/hello-protocol.cpp)

  /* Extract DvInfo and process Distance Vector update */
  const auto& content = data.getContent();
  proto::DvInfo dvinfo_proto;
  //NS_LOG_DEBUG("Content: size=" << content.value_size());
  //NS_LOG_INFO("Trying to parser  DV-Info...");
  if (!dvinfo_proto.ParseFromArray(content.value(), content.value_size())) {
    NS_LOG_INFO("Invalid DvInfo content!!! Abort processing..");
    return;
  }
  //NS_LOG_INFO("Parser complete! dvinfo_proto content is:");
  //for (int i = 0; i < dvinfo_proto.entry_size(); ++i) {
  //  const auto& entry = dvinfo_proto.entry(i);
  //  NS_LOG_INFO("DV-Info from neighbor prefix=" << entry.prefix() << " seqNum=" << entry.seq() << " cost=" << entry.cost());
  //}
  //NS_LOG_INFO("Decoding...");
  auto otherRT = DecodeDvInfo(dvinfo_proto);
  processDvInfoFromNeighbor(neigh_it->second, otherRT);
  //NS_LOG_INFO("Done");
}

void
Ndvr::processDvInfoFromNeighbor(NeighborEntry& neighbor, RoutingTable& otherRT) {
  NS_LOG_INFO("Process DvInfo from neighbor=" << neighbor.GetName());
  for (auto entry : otherRT) {
    std::string neigh_prefix = entry.first;
    uint64_t neigh_seq = entry.second.GetSeqNum();
    uint32_t neigh_cost = entry.second.GetCost();
    NS_LOG_INFO("===>> prefix=" << neigh_prefix << " seqNum=" << neigh_seq << " recvCost=" << neigh_cost);

    /* Sanity checks: 1) ignore our own name prefixes (direct route); 2) ignore invalid seqNum; 3) ignore invalid Cost */
    if (m_routingTable.isDirectRoute(neigh_prefix) || neigh_seq <= 0 || !isValidCost(neigh_cost))
      continue;

    /* insert new prefix */
    RoutingEntry localRE;
    if (!m_routingTable.LookupRoute(neigh_prefix, localRE)) {
      NS_LOG_INFO("======>> New prefix! Just insert it");
      entry.second.SetCost(CalculateCostToNeigh(neighbor, neigh_cost));
      entry.second.SetFaceId(neighbor.GetFaceId());
      m_routingTable.AddRoute(entry.second);

      /* schedule a immediate ehlo message to notify neighbors about a new DvInfo */
      ResetHelloInterval();
      SendHelloInterest();
      continue;
    }

    /* cost is "infinity", so remove it */
    if (isInfinityCost(neigh_cost)) {
      /* Delete route only if update was received from my nexthop neighbor */
      if (!localRE.isNextHop(neighbor.GetFaceId()))
        continue;

      NS_LOG_INFO("======>> Infinity cost! Remove name prefix" << neigh_prefix);
      m_routingTable.DeleteRoute(localRE, neighbor.GetFaceId());

      /* schedule a immediate ehlo message to notify neighbors about a new DvInfo */
      ResetHelloInterval();
      SendHelloInterest();
      continue;
    }

    /* compare the Received and Local SeqNum (in Routing Entry)*/
    neigh_cost = CalculateCostToNeigh(neighbor, neigh_cost);
    if (neigh_seq > localRE.GetSeqNum()) {
      NS_LOG_INFO("======>> New SeqNum, update name prefix! local_seqNum=" << localRE.GetSeqNum());
      // TODO:
      //   - Recv_Cost == Local_cost: update Local_SeqNum
      //   - Recv_Cost != Local_cost: wait SettlingTime, then update Local_Cost / Local_SeqNum
      if (localRE.GetCost() == neigh_cost) {
        // TODO: what if they have the same cost by differente faces?
        localRE.SetSeqNum(neigh_seq);
      } else {
        localRE.SetCost(neigh_cost);
        localRE.SetFaceId(neighbor.GetFaceId());
        localRE.SetSeqNum(neigh_seq);
      }
    } else if (neigh_seq == localRE.GetSeqNum() && neigh_cost < localRE.GetCost()) {
      NS_LOG_INFO("======>> Equal SeqNum but Better Cost, update name prefix! local_cost=" << localRE.GetCost());
      // TODO: wait SettlingTime, then update Local_Cost
      localRE.SetCost(neigh_cost);
      localRE.SetFaceId(neighbor.GetFaceId());
    } else if (neigh_seq == localRE.GetSeqNum() && neigh_cost >= localRE.GetCost()) {
      //NS_LOG_INFO("======>> Equal SeqNum and (Equal or Worst Cost), however learn name prefix! local_cost=" << localRE.GetCost());
      // TODO: save this new prefix as well to multipath
    } else {
      /* Recv_SeqNum < Local_SeqNu: discard/next, we already have a most recent update */
      continue;
    }
  }
}

uint32_t
Ndvr::CalculateCostToNeigh(NeighborEntry& neighbor, uint32_t cost) {
  return cost+1;
}

bool
Ndvr::isValidCost(uint32_t cost) {
  return cost < std::numeric_limits<uint32_t>::max();
}

bool
Ndvr::isInfinityCost(uint32_t cost) {
  return cost == std::numeric_limits<uint32_t>::max();
}

} // namespace ndvr
} // namespace ndn
