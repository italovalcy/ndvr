/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "nrdv.hpp"
#include <limits>
#include <ns3/simulator.h>
#include <ns3/log.h>
#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/node-list.h>
#include <ns3/ndnSIM/helper/ndn-stack-helper.hpp>
#include <ndn-cxx/lp/tags.hpp>

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
    DvInfoEntry dvinfoEntry(*it, 1, 0, 0);
    m_dvinfo[*it] = dvinfoEntry;
    m_dvinfoLearned[*it] = dvinfoEntry;
  }
  m_face.setInterestFilter(kNrdvPrefix, std::bind(&Nrdv::processInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      throw Error("Failed to register sync interest prefix: " + reason);
  });

  buildRouterPrefix();
  registerPrefixes();
  m_scheduler.schedule(ndn::time::seconds(1),
                        [this] { printFib(); });
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
  /* First of all, cancel any previously scheduled events */
  sendhello_event.cancel();

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

  sendhello_event = m_scheduler.schedule(time::seconds(m_helloIntervalCur),
                                        [this] { SendHelloInterest(); });
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

  // Not necessary anymore, since the DvInfo is sent on demand (when receiving an ehlo)
  //ns3::Simulator::Schedule(ns3::Seconds(m_dvinfoInterval), &Nrdv::SendDvInfoInterest, this, neighbor);
}

void Nrdv::processInterest(const ndn::Interest& interest) {
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
  if (kNrdvHelloPrefix.isPrefixOf(interestName))
    return OnHelloInterest(interest, inFaceId);
  else if (kNrdvDvInfoPrefix.isPrefixOf(interestName))
    return OnDvInfoInterest(interest);
  else if (kNrdvKeyPrefix.isPrefixOf(interestName))
    return OnKeyInterest(interest);

  NS_LOG_INFO("Unknown Interest " << interestName);
}

void Nrdv::IncreaseHelloInterval() {
  /* exponetially increase the helloInterval until the maximum allowed */
  if (increasehellointerval_event)
    return;
  increasehellointerval_event = m_scheduler.schedule(time::seconds(1),
      [this] {
        m_helloIntervalCur = (2*m_helloIntervalCur > m_helloIntervalMax) ? m_helloIntervalMax : 2*m_helloIntervalCur;
      });
}

void Nrdv::ResetHelloInterval() {
  increasehellointerval_event.cancel();
  m_helloIntervalCur = m_helloIntervalIni;
}

void Nrdv::OnHelloInterest(const ndn::Interest& interest, uint64_t inFaceId) {
  const ndn::Name interestName(interest.getName());
  NS_LOG_INFO("Received HELLO Interest " << interestName);

  std::string neighPrefix = ExtractRouterPrefix(interestName, kNrdvHelloPrefix);
  if (!isValidRouter(interestName, kNrdvHelloPrefix)) {
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

void Nrdv::OnDvInfoInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received DV-Info Interest " << interest.getName());

  // Sanity check
  std::string routerPrefix = ExtractRouterPrefix(interest.getName(), kNrdvDvInfoPrefix);
  if (routerPrefix != m_routerPrefix) {
    //NS_LOG_INFO("Interest is not to me, ignoring.. received_name=" << routerPrefix << " my_name=" << m_routerPrefix);
    return;
  }

  // TODO: send our DV-Info
  auto data = std::make_shared<ndn::Data>(interest.getName());
  data->setFreshnessPeriod(ndn::time::milliseconds(1000));
  // Set dvinfo
  std::string dvinfo_str;
  EncodeDvInfo(m_dvinfoLearned, dvinfo_str);
  NS_LOG_INFO("Replying DV-Info with encoded data: size=" << dvinfo_str.size());
  //NS_LOG_INFO("Sending DV-Info encoded: str=" << dvinfo_str);
  data->setContent(reinterpret_cast<const uint8_t*>(dvinfo_str.data()), dvinfo_str.size());
  // Sign and send
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

  /* Sanity checks */
  std::string neighPrefix = ExtractRouterPrefix(data.getName(), kNrdvDvInfoPrefix);
  if (!isValidRouter(data.getName(), kNrdvDvInfoPrefix)) {
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
  auto dvinfo_other = DecodeDvInfo(dvinfo_proto);
  processDvInfoFromNeighbor(neigh_it->second, dvinfo_other);
  //NS_LOG_INFO("Done");
}

void
Nrdv::processDvInfoFromNeighbor(NeighborEntry& neighbor, DvInfoMap& dvinfo_other) {
  NS_LOG_INFO("Process DvInfo from neighbor=" << neighbor.GetName());
  bool dvinfoUpdated = false;
  for (auto entry : dvinfo_other) {
    std::string neigh_prefix = entry.first;
    uint64_t neigh_seq = entry.second.GetSeqNum();
    uint32_t neigh_cost = entry.second.GetCost();
    NS_LOG_INFO("===>> prefix=" << neigh_prefix << " seqNum=" << neigh_seq << " recvCost=" << neigh_cost);

    /* ignore our own name prefixes */
    if (m_dvinfo.count(neigh_prefix))
      continue;

    /* insert new prefix with valid seqNum */
    if (!m_dvinfoLearned.count(neigh_prefix) && neigh_seq > 0 && isValidCost(neigh_cost)) {
      NS_LOG_INFO("======>> New prefix! Just insert it");
      entry.second.SetCost(updateCostToNeigh(neighbor, neigh_cost));
      entry.second.SetFaceId(neighbor.GetFaceId());
      //m_dvinfoLearned[neigh_prefix] = DvInfoEntry(neigh_prefix, neigh_seq, neigh_cost, neighbor.GetFaceId());
      m_dvinfoLearned[neigh_prefix] = entry.second;

      /* schedule a immediate ehlo message to notify neighbors about a new DvInfo */
      ResetHelloInterval();
      SendHelloInterest();
      continue;
    }

    /* cost is "infinity", so remove it */
    if (isInfinityCost(neigh_cost)) {
      NS_LOG_INFO("======>> Infinity cost! Remove name prefix" << neigh_prefix);
      // TODO: we may have other faces to this neighbor!
      m_dvinfoLearned.erase(neigh_prefix);
      // TODO: update fib

      /* schedule a immediate ehlo message to notify neighbors about a new DvInfo */
      ResetHelloInterval();
      SendHelloInterest();
      continue;
    }

    /* compare the Received and Local SeqNum */
    auto& dvinfo_local = m_dvinfoLearned[neigh_prefix];
    neigh_cost = updateCostToNeigh(neighbor, neigh_cost);
    if (neigh_seq > dvinfo_local.GetSeqNum()) {
      NS_LOG_INFO("======>> New SeqNum, update name prefix! local_seqNum=" << dvinfo_local.GetSeqNum());
      // TODO:
      //   - Recv_Cost == Local_cost: update Local_SeqNum
      //   - Recv_Cost != Local_cost: wait SettlingTime, then update Local_Cost / Local_SeqNum
      if (dvinfo_local.GetCost() == neigh_cost) {
        // TODO: what if they have the same cost by differente faces?
        dvinfo_local.SetSeqNum(neigh_seq);
      } else {
        dvinfo_local.SetCost(neigh_cost);
        dvinfo_local.SetFaceId(neighbor.GetFaceId());
        dvinfo_local.SetSeqNum(neigh_seq);
        dvinfoUpdated = true;
      }
    } else if (neigh_seq == dvinfo_local.GetSeqNum() && neigh_cost < dvinfo_local.GetCost()) {
      NS_LOG_INFO("======>> Equal SeqNum but Better Cost, update name prefix! local_cost=" << dvinfo_local.GetCost());
      // TODO: wait SettlingTime, then update Local_Cost
      dvinfo_local.SetCost(neigh_cost);
      dvinfo_local.SetFaceId(neighbor.GetFaceId());
      dvinfoUpdated = true;
    } else if (neigh_seq == dvinfo_local.GetSeqNum() && neigh_cost >= dvinfo_local.GetCost()) {
      //NS_LOG_INFO("======>> Equal SeqNum and (Equal or Worst Cost), however learn name prefix! local_cost=" << dvinfo_local.GetCost());
      // TODO: save this new prefix as well to multipath
    } else {
      /* Recv_SeqNum < Local_SeqNu: discard/next, we already have a most recent update */
      continue;
    }
  }

  if (dvinfoUpdated) {
    // TODO: apply changes to the FIB
    NS_LOG_INFO("==> Update FIB from Distance Vector Updates");
  }
}

uint32_t
Nrdv::updateCostToNeigh(NeighborEntry& neighbor, uint32_t cost) {
  return cost+1;
}

bool
Nrdv::isValidCost(uint32_t cost) {
  return cost < std::numeric_limits<uint32_t>::max();
}

bool
Nrdv::isInfinityCost(uint32_t cost) {
  return cost == std::numeric_limits<uint32_t>::max();
}

} // namespace nrdv
} // namespace ndn
