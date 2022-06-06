/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "simplepubsub.hpp"
#include <limits>
#include <cmath>
#include <boost/algorithm/string.hpp> 
#include <algorithm>
#include <boost/uuid/detail/sha1.hpp>
#include <ns3/simulator.h>
#include <ns3/log.h>
#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/node-list.h>
#include <ns3/ndnSIM/helper/ndn-stack-helper.hpp>
#include <ndn-cxx/lp/tags.hpp>

#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"
#include "ns3/ndnSIM/NFD/daemon/face/generic-link-service.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.SimplePubSub");

namespace ndn {
namespace ndvr {

SimplePubSub::SimplePubSub()
  : m_scheduler(m_face.getIoService())
  , m_validator(m_face)
  , m_rengine(rdevice_())
  , m_rand(ns3::CreateObject<ns3::UniformRandomVariable>())
{
  ns3::Ptr<ns3::Node> thisNode = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
  m_nodeid = thisNode->GetId();
  registerPrefixes();
}

void SimplePubSub::Start() {
  m_face.setInterestFilter("/simplepubsub/syncNotify", std::bind(&SimplePubSub::OnSyncNotify, this, _2),
                                     std::bind([]{}), std::bind([]{}));
  if (m_syncDataRounds) {
    m_scheduler.schedule(ndn::time::milliseconds(4000 + 10*m_nodeid),
                        [this] { AddNewNamePrefix(1); });
    m_scheduler.schedule(ndn::time::milliseconds(4000 + 11*m_nodeid),
                        [this] { CheckPendingSync(); });
  }
}

void SimplePubSub::Stop() {
}

void SimplePubSub::run() {
  m_face.processEvents();
}

void SimplePubSub::registerPrefixes() {
  using namespace ns3;
  using namespace ns3::ndn;

  Name namePrefix("/simplepubsub/syncNotify");

  int32_t metric = 0; // should it be 0 or std::numeric_limits<int32_t>::max() ??
  Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());
  NS_LOG_DEBUG("THIS node is: " << thisNode->GetId());

  for (uint32_t deviceId = 0; deviceId < thisNode->GetNDevices(); deviceId++) {
    Ptr<NetDevice> device = thisNode->GetDevice(deviceId);
    Ptr<L3Protocol> ndn = thisNode->GetObject<L3Protocol>();
    NS_ASSERT_MSG(ndn != nullptr, "Ndn stack should be installed on the node");

    auto face = ndn->getFaceByNetDevice(device);
    NS_ASSERT_MSG(face != nullptr, "There is no face associated with the net-device");

    NS_LOG_DEBUG("FibHelper::AddRoute prefix=" << namePrefix << " via faceId=" << face->getId());
    FibHelper::AddRoute(thisNode, namePrefix, face, metric);
  }
}

void SimplePubSub::AddNewNamePrefix(uint32_t round) {
  /* stop data gen process after the configured max rounds */
  if (round > m_syncDataRounds) 
    return;

  ndn::Name namePrefix("/ndn/ndvrSync");
  namePrefix.appendNumber(m_nodeid).appendNumber(round).appendNumber(0);

  NS_LOG_DEBUG("AdvName = " << namePrefix);

  SendSyncNotify(namePrefix.toUri());

  m_scheduler.schedule(ndn::time::milliseconds(m_data_gen_dist(m_rengine)),
                      [this, round] { AddNewNamePrefix(round+1); });
}

void SimplePubSub::CheckPendingSync() {
  for (const std::string& p: m_pendingSync) {
    RequestSyncData(p);
  }

  m_scheduler.schedule(ndn::time::seconds(1),
                      [this] { CheckPendingSync(); });
}

void SimplePubSub::SendSyncNotify(const std::string nameStr) {
  Name name = Name("/simplepubsub/syncNotify");
  NS_LOG_INFO("Sending SyncNotify " << name);

  Interest interest = Interest();
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setName(name);
  interest.setCanBePrefix(false);
  interest.setInterestLifetime(time::milliseconds(1));

  interest.setApplicationParameters(reinterpret_cast<const uint8_t*>(nameStr.data()), nameStr.size());

  m_face.expressInterest(interest, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});

  m_scheduler.schedule(time::seconds(1),
      [this, nameStr] { SendSyncNotify(nameStr); });
}

void
SimplePubSub::OnSyncNotify(const ndn::Interest& interest)
{
  if (!interest.hasApplicationParameters() || interest.getApplicationParameters().value_size() <= 0) {
    NS_LOG_INFO("SyncNotify with invalid parameters=" << interest);
    return;
  }
  std::string prefix;
  prefix.assign((char *)interest.getApplicationParameters().value(), interest.getApplicationParameters().value_size());
  NS_LOG_INFO("SyncNotify i.name=" << interest <<  " prefix=" << prefix);
  
  if (m_satisfiedSync.find(prefix) != m_satisfiedSync.end()) {
    return;
  }

  if (m_pendingSync.find(prefix) != m_pendingSync.end()) {
    return;
  }

  NS_LOG_INFO("SyncNotify add pending sync prefix=" << prefix);
  m_pendingSync.insert(prefix);
}

void SimplePubSub::RequestSyncData(const std::string name, uint32_t retx) {
  NS_LOG_DEBUG("Sending Data interest i.name=" << name << " i.retx=" << retx);

  Interest interest = Interest(name);
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setCanBePrefix(false);
  interest.setInterestLifetime(time::seconds(1));

  m_face.expressInterest(interest,
    std::bind(&SimplePubSub::OnSyncDataContent, this, _1, _2),
    std::bind(&SimplePubSub::OnSyncDataNack, this, _1, _2),
    std::bind(&SimplePubSub::OnSyncDataTimedOut, this, _1, retx));
}

void SimplePubSub::OnSyncDataTimedOut(const ndn::Interest& interest, uint32_t retx) {
  NS_LOG_DEBUG("Interest timed out for Name: " << interest.getName() << " retx=" << retx);
  //RequestSyncData(interest.getName().toUri(), retx+1);
}

void SimplePubSub::OnSyncDataNack(const ndn::Interest& interest, const ndn::lp::Nack& nack) {
  NS_LOG_DEBUG("Received Nack for " << interest.getName() << " with reason: " << nack.getReason());
}

void SimplePubSub::OnSyncDataContent(const ndn::Interest& interest, const ndn::Data& data) {
  NS_LOG_DEBUG("Received content for SynData: " << data.getName());
  std::string prefix = data.getName().toUri();
  m_satisfiedSync.insert(prefix);
  if (m_pendingSync.find(prefix) != m_pendingSync.end()) {
    m_pendingSync.erase(prefix);
  }
}


} // namespace ndvr
} // namespace ndn
