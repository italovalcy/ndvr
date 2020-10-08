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

Ndvr::Ndvr(const ndn::security::SigningInfo& signingInfo, Name network, Name routerName, std::vector<std::string>& npv)
  : m_signingInfo(signingInfo)
  , m_scheduler(m_face.getIoService())
  , m_validator(m_face)
  , m_seq(0)
  , m_rand(ns3::CreateObject<ns3::UniformRandomVariable>())
  , m_network(network)
  , m_routerName(routerName)
  , m_helloIntervalIni(1)
  , m_helloIntervalCur(1)
  , m_helloIntervalMax(5)
  //, m_helloIntervalMax(60)
  , m_localRTInterval(1)
  , m_localRTTimeout(1)
  , m_rengine(rdevice_())
{
  buildRouterPrefix();

  ns3::Ptr<ns3::Node> thisNode = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
  m_nodeid = thisNode->GetId();

  // TODO: this should be a conf parameter
  std::string fileName = "config/validation.conf";
  try {
    m_validator.load(fileName);
  }
  catch (const std::exception &e ) {
    throw Error("Failed to load validation rules file=" + fileName + " Error=" + e.what());
  }

  for (std::vector<std::string>::iterator it = npv.begin() ; it != npv.end(); ++it) {
    RoutingEntry routingEntry;
    routingEntry.SetName(*it);
    routingEntry.SetSeqNum(1);
    routingEntry.SetCost(0);
    routingEntry.SetFaceId(0); /* directly connected */
    m_routingTable.insert(routingEntry);
  }
  m_face.setInterestFilter(kNdvrHelloPrefix, std::bind(&Ndvr::processInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      throw Error("Failed to register sync interest prefix: " + reason);
  });
  Name routerDvInfoPrefix = kNdvrDvInfoPrefix;
  routerDvInfoPrefix.append(m_routerPrefix);
  m_face.setInterestFilter(routerDvInfoPrefix, std::bind(&Ndvr::processInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      throw Error("Failed to register sync interest prefix: " + reason);
  });
  Name routerKey = m_routerPrefix;
  routerKey.append("KEY");
  m_face.setInterestFilter(routerKey, std::bind(&Ndvr::OnKeyInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      throw Error("Failed to register sync interest prefix: " + reason);
  });

  registerPrefixes();
//  m_scheduler.schedule(ndn::time::seconds(1),
//                        [this] { printFib(); });
}

void Ndvr::Start() {
  SendHelloInterest();
  if (m_syncDataRounds) {
    m_scheduler.schedule(ndn::time::milliseconds(4000 + 10*m_nodeid),
                        [this] { AddNewNamePrefix(1); });
  }
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

void Ndvr::registerNeighborPrefix(NeighborEntry& neighbor, uint64_t oldFaceId, uint64_t newFaceId) {
  using namespace ns3;
  using namespace ns3::ndn;
  NS_LOG_DEBUG("AddNeighPrefix=" << neighbor.GetName() << " oldFaceId=" << oldFaceId << " newFaceId=" << newFaceId);

  int32_t metric = CalculateCostToNeigh(neighbor, 0);
  Name namePrefix = Name(neighbor.GetName());
  Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());

  if (oldFaceId != 0) {
    FibHelper::RemoveRoute(thisNode, namePrefix, oldFaceId);
  }
  FibHelper::AddRoute(thisNode, namePrefix, newFaceId, metric);
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
    FibHelper::AddRoute(thisNode, kNdvrHelloPrefix, face, metric);
    FibHelper::AddRoute(thisNode, kNdvrDvInfoPrefix, face, metric);
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
  Name name = Name(kNdvrDvInfoPrefix);
  name.append(neighbor.GetName());
  name.appendNumber(ns3::Simulator::Now().GetSeconds()); /* XXX: another way would be the
                                                            node send the version on the 
                                                            hello message */

  Interest interest = Interest();
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setName(name);
  interest.setCanBePrefix(false);
  interest.setMustBeFresh(true);
  interest.setInterestLifetime(time::seconds(m_localRTTimeout));

  m_face.expressInterest(interest,
    std::bind(&Ndvr::OnDvInfoContent, this, _1, _2),
    std::bind(&Ndvr::OnDvInfoNack, this, _1, _2),
    std::bind(&Ndvr::OnDvInfoTimedOut, this, _1));

  // Not necessary anymore, since the DvInfo is sent on demand (when receiving an ehlo)
  //ns3::Simulator::Schedule(ns3::Seconds(m_localRTInterval), &Ndvr::SendDvInfoInterest, this, neighbor);
}

uint64_t Ndvr::ExtractIncomingFace(const ndn::Interest& interest) {
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
    return 0;
  }
  return *incomingFaceIdTag;
}

uint64_t Ndvr::ExtractIncomingFace(const ndn::Data& data) {
  shared_ptr<lp::IncomingFaceIdTag> incomingFaceIdTag = data.getTag<lp::IncomingFaceIdTag>();
  if (incomingFaceIdTag == nullptr) {
    return 0;
  }
  return *incomingFaceIdTag;
}

void Ndvr::processInterest(const ndn::Interest& interest) {
  uint64_t inFaceId = ExtractIncomingFace(interest);
  if (!inFaceId) {
    //NS_LOG_DEBUG("Discarding Interest from internal face: " << interest);
    return;
  }
  NS_LOG_INFO("Interest: " << interest << " inFaceId=" << inFaceId);

  const ndn::Name interestName(interest.getName());
  if (kNdvrHelloPrefix.isPrefixOf(interestName))
    return OnHelloInterest(interest, inFaceId);
  else if (kNdvrDvInfoPrefix.isPrefixOf(interestName))
    return OnDvInfoInterest(interest);

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
    uint64_t oldFaceId = 0;
    registerNeighborPrefix(neigh->second, oldFaceId, inFaceId);
  } else {
    NS_LOG_INFO("Already known router, increasing the hello interval");
    if (neigh->second.GetFaceId() != inFaceId) {
      /* Issue #2: TODO: We need to be careful about this because since we are using default multicast forward strategy,
       * mean that one node can forward ndvr messages from other nodes, so the interest might be received from
       * the direct face or from the intermediate forwarder face! Not necessarily means the node moved! */
      //NS_LOG_INFO("Neighbor moved from faceId=" << neigh->second.GetFaceId() << " to faceId=" << inFaceId << " neigh=" << neighPrefix);
      //registerNeighborPrefix(neigh->second, neigh->second.GetFaceId(), inFaceId);
    }
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

  /* group DvInfo replies to avoid duplicates */
  if (replydvinfo_event)
    return;
  replydvinfo_event = m_scheduler.schedule(time::milliseconds(replydvinfo_dist(m_rengine)),
      [this, interest] {
        ReplyDvInfoInterest(interest);
      });
}

void Ndvr::ReplyDvInfoInterest(const ndn::Interest& interest) {
  auto data = std::make_shared<ndn::Data>(interest.getName());
  data->setFreshnessPeriod(ndn::time::milliseconds(1000));
  // Set dvinfo
  std::string dvinfo_str;
  EncodeDvInfo(dvinfo_str);
  NS_LOG_INFO("Replying DV-Info with encoded data: size=" << dvinfo_str.size() << " I=" << interest.getName());
  //NS_LOG_INFO("Sending DV-Info encoded: str=" << dvinfo_str);
  data->setContent(reinterpret_cast<const uint8_t*>(dvinfo_str.data()), dvinfo_str.size());
  // Sign and send
  m_keyChain.sign(*data, m_signingInfo);
  m_face.put(*data);
}

void Ndvr::OnKeyInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received KEY Interest " << interest.getName());
  std::string nameStr = interest.getName().toUri();
  std::size_t pos = nameStr.find("/KEY/");
  ndn::Name identityName = ndn::Name(nameStr.substr(0, pos));

  try {
    // Create Data packet
    ndn::security::v2::Certificate cert = m_keyChain.getPib().getIdentity(identityName).getDefaultKey().getDefaultCertificate();

    // Return Data packet to the requester
    m_face.put(cert);
  }
  catch (const std::exception& ) {
    NS_LOG_DEBUG("The certificate: " << interest.getName() << " does not exist! I was looking for Identity=" << identityName);
  }
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

  /* Security validation */
  if (data.getSignature().hasKeyLocator()) {
    NS_LOG_DEBUG("Data signed with: " << data.getSignature().getKeyLocator().getName());
  }

  /* Check for overheard DvInfo */
  auto neigh_it = m_neighMap.find(neighPrefix);
  if (neigh_it == m_neighMap.end()) {
    uint64_t oldFaceId = 0;
    uint64_t inFaceId = ExtractIncomingFace(data);
    if (inFaceId) {
      NS_LOG_INFO("Overheard DvInfo neigh=" << neighPrefix << " inFaceId=" << inFaceId);
      neigh_it = m_neighMap.emplace(neighPrefix, NeighborEntry(neighPrefix, inFaceId, 0)).first;
      registerNeighborPrefix(neigh_it->second, oldFaceId, inFaceId);
    } else {
      NS_LOG_INFO("Discarding Overheard DvInfo neigh=" << neighPrefix << " inFaceId=" << inFaceId << " (invalid IncomingFace)");
      return;
    }
  } else {
    //NS_LOG_DEBUG("Neighbor already known: neighbor=" << neighPrefix << " faceId=" << neigh_it->second.GetFaceId());
  }

  // Validating data
  m_validator.validate(data,
                       std::bind(&Ndvr::OnValidatedDvInfo, this, _1),
                       std::bind(&Ndvr::OnDvInfoValidationFailed, this, _1, _2));
}

void Ndvr::OnValidatedDvInfo(const ndn::Data& data) {
  NS_LOG_DEBUG("Validated data: " << data.getName());
  std::string neighPrefix = ExtractRouterPrefix(data.getName(), kNdvrDvInfoPrefix);

  /* Sanity check: at this point the neighbor should be known */
  auto neigh_it = m_neighMap.find(neighPrefix);
  if (neigh_it == m_neighMap.end()) {
    NS_LOG_INFO("Discard DvInfo from unknonw neighbor=" << neighPrefix);
    return;
  }

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

void Ndvr::OnDvInfoValidationFailed(const ndn::Data& data, const ndn::security::v2::ValidationError& ve) {
  NS_LOG_DEBUG("Not validated data: " << data.getName() << ". The failure info: " << ve);
}

void Ndvr::EncodeDvInfo(std::string& out) {
  proto::DvInfo dvinfo_proto;
  for (auto it = m_routingTable.begin(); it != m_routingTable.end(); ++it) {
    /* For local routes, increment the seqNum */
    if (it->second.isDirectRoute())
      it->second.IncSeqNum();

    auto* entry = dvinfo_proto.add_entry();
    entry->set_prefix(it->first);
    entry->set_seq(it->second.GetSeqNum());
    entry->set_cost(it->second.GetCost());
  }
  dvinfo_proto.AppendToString(&out);
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
      NS_LOG_INFO("======>> New prefix! Just insert it " << neigh_prefix);
      entry.second.SetCost(CalculateCostToNeigh(neighbor, neigh_cost));
      entry.second.SetFaceId(neighbor.GetFaceId());
      m_routingTable.AddRoute(entry.second);
      if (m_syncDataRounds) {
        m_scheduler.schedule(ndn::time::microseconds(packet_dist(m_rengine)),
                        [this, neigh_prefix] { RequestSyncData(ndn::Name(neigh_prefix)); });
      }

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
        if (!localRE.isNextHop(neighbor.GetFaceId())) {
          /* TODO: if they have the same cost but from different faces, could save for multipath */
        }
        localRE.SetSeqNum(neigh_seq);
      } else {
        /* Cost change will be handle by periodic updates */
        localRE.SetCost(neigh_cost);
        localRE.SetSeqNum(neigh_seq);
        m_routingTable.UpdateRoute(localRE, neighbor.GetFaceId());
      }
    } else if (neigh_seq == localRE.GetSeqNum() && neigh_cost < localRE.GetCost()) {
      NS_LOG_INFO("======>> Equal SeqNum but Better Cost, update name prefix! local_cost=" << localRE.GetCost());
      /* Cost change will be handle by periodic updates */
      // TODO: wait SettlingTime, then update Local_Cost
      localRE.SetCost(neigh_cost);
      m_routingTable.UpdateRoute(localRE, neighbor.GetFaceId());
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

void Ndvr::AdvNamePrefix(std::string name) {
  RoutingEntry routingEntry;
  routingEntry.SetName(name);
  routingEntry.SetSeqNum(1);
  routingEntry.SetCost(0);
  routingEntry.SetFaceId(0); /* directly connected */

  /* If the application already started (ie., there is a Hello Event), then
   * update the routing table and schedule a immediate ehlo message to notify
   * neighbors about a new DvInfo; otherwise, just insert on the initial
   * routing table
   * */
  m_routingTable.insert(routingEntry);
  if (sendhello_event) {
    ResetHelloInterval();
    SendHelloInterest();
  }
}

void Ndvr::AddNewNamePrefix(uint32_t round) {
  /* stop data gen process after the configured max rounds */
  if (round > m_syncDataRounds) 
    return;

  ndn::Name namePrefix("/ndn/ndvrSync");
  namePrefix.appendNumber(m_nodeid).appendNumber(round).appendNumber(0);

  NS_LOG_DEBUG("AdvName = " << namePrefix);

  AdvNamePrefix(namePrefix.toUri());

  m_scheduler.schedule(ndn::time::milliseconds(m_data_gen_dist(m_rengine)),
                      [this, round] { AddNewNamePrefix(round+1); });
}

void Ndvr::RequestSyncData(const ndn::Name name, uint32_t retx) {
  NS_LOG_DEBUG("Sending Data interest i.name=" << name << " i.retx=" << retx);

  Interest interest = Interest(name);
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setCanBePrefix(false);
  interest.setInterestLifetime(time::seconds(m_localRTTimeout));

  m_face.expressInterest(interest,
    std::bind(&Ndvr::OnSyncDataContent, this, _1, _2),
    std::bind(&Ndvr::OnSyncDataNack, this, _1, _2),
    std::bind(&Ndvr::OnSyncDataTimedOut, this, _1, retx));
}

void Ndvr::OnSyncDataTimedOut(const ndn::Interest& interest, uint32_t retx) {
  NS_LOG_DEBUG("Interest timed out for Name: " << interest.getName() << " retx=" << retx);
  RequestSyncData(interest.getName(), retx+1);
}

void Ndvr::OnSyncDataNack(const ndn::Interest& interest, const ndn::lp::Nack& nack) {
  NS_LOG_DEBUG("Received Nack for " << interest.getName() << " with reason: " << nack.getReason());
}

void Ndvr::OnSyncDataContent(const ndn::Interest& interest, const ndn::Data& data) {
  NS_LOG_DEBUG("Received content for SynData: " << data.getName());
}


} // namespace ndvr
} // namespace ndn
