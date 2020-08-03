/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "nrdv.hpp"
#include <limits>
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
    DvInfoEntry dvinfoEntry(*it, 1, 0);
    m_dvinfo[*it] = dvinfoEntry;
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
  if (m_neighMap.count(neighPrefix)) {
    NS_LOG_INFO("Already known router, ignoring...");
    /* exponetially increase the helloInterval until the maximum allowed */
    m_helloIntervalCur = (2*m_helloIntervalCur > m_helloIntervalMax) ? m_helloIntervalMax : 2*m_helloIntervalCur;
    return;
  }
  m_helloIntervalCur = m_helloIntervalIni;
  NeighborEntry neigh(neighPrefix, 0);
  m_neighMap[neighPrefix] = neigh;
  SendDvInfoInterest(neigh);
}

void Nrdv::OnDvInfoInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received DV-Info Interest " << interest.getName());

  // Sanity check
  std::string routerPrefix = ExtractRouterPrefix(interest.getName(), kNrdvDvInfoPrefix);
  if (routerPrefix != m_routerPrefix) {
    NS_LOG_INFO("Interest is not to me, ignoring.. received_name=" << routerPrefix << " my_name=" << m_routerPrefix);
    return;
  }

  // TODO: send our DV-Info
  auto data = std::make_shared<ndn::Data>(interest.getName());
  data->setFreshnessPeriod(ndn::time::milliseconds(1000));
  // Set dvinfo
  std::string dvinfo_str;
  EncodeDvInfo(m_dvinfo, dvinfo_str);
  NS_LOG_INFO("Sending DV-Info encoded: size=" << dvinfo_str.size());
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
  if (!m_neighMap.count(neighPrefix)) {
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
  processDvInfoFromNeighbor(neighPrefix, dvinfo_other);
  //NS_LOG_INFO("Done");
}

void
Nrdv::processDvInfoFromNeighbor(std::string neighbor, DvInfoMap& dvinfo_other) {
  NS_LOG_INFO("Process DvInfo from neighbor=" << neighbor);
  for (auto entry : dvinfo_other) {
    std::string neigh_prefix = entry.first;
    uint64_t neigh_seq = entry.second.GetSeqNum();
    uint32_t neigh_cost = entry.second.GetCost();
    NS_LOG_INFO("===>> prefix=" << neigh_prefix << " seqNum=" << neigh_seq << " cost=" << neigh_cost);

    /* ignore our own name prefixes */
    if (m_dvinfo.count(neigh_prefix))
      continue;

    /* insert new prefix with valid seqNum */
    if (!m_dvinfoLearned.count(neigh_prefix) && neigh_seq > 0 && isValidCost(neigh_cost)) {
      m_dvinfoLearned[neigh_prefix] = entry.second;
      continue;
    }

    /* cost is "infinity", so remove it */
    if (isInfinityCost(neigh_cost)) {
      NS_LOG_INFO("======>> Infinity cost! Remove name prefix" << neigh_prefix);
      // TODO: we may have other faces to this neighbor!
      m_dvinfoLearned.erase(neigh_prefix);
      // TODO: update fib
    }

    /* compare the Received and Local SeqNum */
    auto& dvinfo_local = m_dvinfoLearned[neigh_prefix];
    if (neigh_seq > dvinfo_local.GetSeqNum()) {
      NS_LOG_INFO("======>> New SeqNum, update name prefix! local_seqNum=" << dvinfo_local.GetSeqNum());
      // TODO:
      //   - Recv_Cost == Local_cost: update Local_SeqNum
      //   - Recv_Cost != Local_cost: wait SettlingTime, then update Local_Cost / Local_SeqNum
    } else if (neigh_seq == dvinfo_local.GetSeqNum() && neigh_cost < dvinfo_local.GetCost()) {
      NS_LOG_INFO("======>> Equal SeqNum but Better Cost, update name prefix! local_cost=" << dvinfo_local.GetCost());
      // TODO: wait SettlingTime, then update Local_Cost
    } else if (neigh_seq == dvinfo_local.GetSeqNum() && neigh_cost >= dvinfo_local.GetCost()) {
      NS_LOG_INFO("======>> Equal SeqNum and (Equal or Worst Cost), however learn name prefix! local_cost=" << dvinfo_local.GetCost());
      // TODO: save this new prefix as well to multipath
    } else {
      /* Recv_SeqNum < Local_SeqNu: discard/next, we already have a most recent update */
      continue;
    }
  }
}

bool
Nrdv::isValidCost(uint32_t cost) {
  return cost > 0 && cost < std::numeric_limits<uint32_t>::max();
}

bool
Nrdv::isInfinityCost(uint32_t cost) {
  return cost == std::numeric_limits<uint32_t>::max();
}

} // namespace nrdv
} // namespace ndn
