/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "rangeconsumer.hpp"
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

NS_LOG_COMPONENT_DEFINE("ndn.RangeConsumer");

namespace ndn {
namespace ndvr {

RangeConsumer::RangeConsumer(std::string prefix, uint32_t first, uint32_t last, uint32_t frequency)
  : m_scheduler(m_face.getIoService())
  , m_validator(m_face)
  , m_rengine(rdevice_())
  , m_rand(ns3::CreateObject<ns3::UniformRandomVariable>())
  , m_prefix(prefix)
  , m_first(first)
  , m_last(last)
  , m_frequency(frequency)
{
  ns3::Ptr<ns3::Node> thisNode = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
  m_nodeid = thisNode->GetId();
}

void RangeConsumer::Start() {
  for(uint32_t i = m_first; i <= m_last; i++) {
    Name namePrefix(m_prefix);
    namePrefix.appendNumber(i);
    RequestSyncData(namePrefix.toUri());
  }
}

void RangeConsumer::Stop() {
}

void RangeConsumer::run() {
  m_face.processEvents();
}

void RangeConsumer::RequestSyncData(const std::string name, uint32_t seq) {
  NS_LOG_DEBUG("Sending Data interest i.name=" << name << " i.seq=" << seq);
  if (seq == std::numeric_limits<uint32_t>::max()) {
    seq = 0;
  }

  Name n = Name(name);
  n.appendSequenceNumber(seq);
  Interest interest = Interest(n);
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setCanBePrefix(false);
  interest.setInterestLifetime(time::seconds(1));

  m_face.expressInterest(interest,
    std::bind(&RangeConsumer::OnSyncDataContent, this, _1, _2),
    std::bind(&RangeConsumer::OnSyncDataNack, this, _1, _2),
    std::bind(&RangeConsumer::OnSyncDataTimedOut, this, _1));

  uint32_t wait = 1000.0 / m_frequency;
  m_scheduler.schedule(time::milliseconds(wait),
      [this, name, seq] { RequestSyncData(name, seq+1); });
}

void RangeConsumer::OnSyncDataTimedOut(const ndn::Interest& interest) {
  NS_LOG_DEBUG("Interest timed out for Name: " << interest.getName());
}

void RangeConsumer::OnSyncDataNack(const ndn::Interest& interest, const ndn::lp::Nack& nack) {
  NS_LOG_DEBUG("Received Nack for " << interest.getName() << " with reason: " << nack.getReason());
}

void RangeConsumer::OnSyncDataContent(const ndn::Interest& interest, const ndn::Data& data) {
  NS_LOG_DEBUG("Received content for SynData: size=" << data.getContent().value_size() << " name=" << data.getName());
}


} // namespace ndvr
} // namespace ndn
