/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ndvr.hpp"
#include <limits>
#include <cmath>
#include <boost/algorithm/string.hpp> 
#include <algorithm>
#include <boost/uuid/sha1.hpp>
#include <ns3/simulator.h>
#include <ns3/log.h>
#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/node-list.h>
#include <ns3/ndnSIM/helper/ndn-stack-helper.hpp>
#include <ndn-cxx/lp/tags.hpp>

#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"
#include "ns3/ndnSIM/NFD/daemon/face/generic-link-service.hpp"
#include "unicast-net-device-transport.hpp"

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
  , m_pivot(m_neighMap.end())
{
  buildRouterPrefix();

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
}

void Ndvr::Start() {
  SendHelloInterest();
  ManageSigningInfo();
}

void Ndvr::Stop() {
}

void Ndvr::run() {
  m_face.processEvents();
}

void Ndvr::registerNeighborPrefix(NeighborEntry& neighbor, uint64_t oldFaceId, uint64_t newFaceId) {
  using namespace ns3;
  using namespace ns3::ndn;
  NS_LOG_DEBUG("AddNeighPrefix=" << neighbor.GetName() << " oldFaceId=" << oldFaceId << " newFaceId=" << newFaceId);

  int32_t metric = CalculateCostToNeigh(neighbor, 0);

  if (oldFaceId != 0) {
    m_routingTable.unregisterPrefix(neighbor.GetName(), oldFaceId);
  }
  m_routingTable.registerPrefix(neighbor.GetName(), newFaceId, metric);
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

    auto addr = device->GetAddress();
    if (m_enableUnicastFaces && Mac48Address::IsMatchingType(addr)) {
      m_macaddr = boost::lexical_cast<std::string>(Mac48Address::ConvertFrom(addr));
      NS_LOG_DEBUG("Save mac =" << m_macaddr);
    }
  }

}

const ndn::security::SigningInfo&
Ndvr::getSigningInfo()
{
  return (m_enableDSK) ? m_signingInfoDSK : m_signingInfo;
}

void
Ndvr::ManageSigningInfo() {
  managesigninginfo_event.cancel();

  /* Sanity check */
  if (!m_enableDSK)
    return;
  if (m_maxSecsDSK==0 && m_maxSizeDSK==0) {
    m_enableDSK = false;
    return;
  }

  /* Whenever needed, create a new DSK certificate based on its validity 
   * period or amount of signed data */
  if (m_signingInfoDSK.getSignerType() == ndn::security::SigningInfo::SIGNER_TYPE_NULL) {
    NS_LOG_DEBUG("createDSK due to bootstrap process");
    createDSK(m_routerPrefix.toUri());
  } else if (m_maxSizeDSK!=0 && m_signedDataAmountDSK >= m_maxSizeDSK) {
    NS_LOG_DEBUG("createDSK due to amountSignedData exceeded signedDataAmount=" << m_signedDataAmountDSK << " maxSize=" << m_maxSizeDSK);
    createDSK(m_routerPrefix.toUri());
  } else if (m_maxSecsDSK!=0 && getSecsSinceLastDSKCert() >= time::seconds(m_maxSecsDSK)) {
    NS_LOG_DEBUG("createDSK due to maxSecs exceeded secsSinceLastDSK=" << getSecsSinceLastDSKCert() << " maxSecs=" << m_maxSecsDSK);
    createDSK(m_routerPrefix.toUri());
  }

  managesigninginfo_event = m_scheduler.schedule(time::seconds(60), [this] { ManageSigningInfo(); });
}

void
Ndvr::createDSK(std::string subjectName) {
  ndn::security::Key dsk = m_keyChain.createKey(m_keyChain.getPib().getIdentity(m_signingInfo.getSignerName()));

  ndn::Name certificateName = dsk.getName();
  certificateName.append("DSK");
  certificateName.appendVersion();

  ndn::security::v2::Certificate cert;
  cert.setName(certificateName);
  cert.setContentType(ndn::tlv::ContentType_Key);
  cert.setFreshnessPeriod(time::hours(1));
  cert.setContent(dsk.getPublicKey().data(), dsk.getPublicKey().size());

  ndn::SignatureInfo signInfoParams;
  signInfoParams.setValidityPeriod(ndn::security::ValidityPeriod(ndn::time::system_clock::now() - time::seconds(1),
                                                                ndn::time::system_clock::now() + ndn::time::days(365)));

  m_keyChain.sign(cert, ndn::security::SigningInfo(m_signingInfo).setSignatureInfo(signInfoParams));

  auto id = m_keyChain.getPib().getIdentity(cert.getIdentity());
  m_keyChain.addCertificate(dsk, cert);
  m_keyChain.setDefaultCertificate(dsk, cert);

  NS_LOG_DEBUG("New-DSK id=" << id.getName() << " cert=" << cert.getName() << " key=" << dsk.getName() << " signingKSK=" << m_signingInfo.getSignerName());
  m_signingInfoDSK = ndn::security::signingByCertificate(cert);
  m_signedDataAmountDSK = 0;
  m_lastDSKCert = time::steady_clock::now();
}

std::string
Ndvr::GetNeighborToken() {
  std::string res;

  if (!m_neighMap.size()) {
    m_pivot = m_neighMap.end();
    return res;
  }

  if (m_pivot == m_neighMap.end() || ++m_pivot == m_neighMap.end()) {
    m_pivot = m_neighMap.begin();
    /* go to a random position */
    std::uniform_int_distribution<int> rand(0, m_neighMap.size() - 1);
    int r = rand(m_rengine);
    for (;r>0;r--)
      m_pivot++;
  }
  res.append(m_pivot->first);

  if (m_neighMap.size() == 1)
    return res;

  if (++m_pivot == m_neighMap.end()) {
    m_pivot = m_neighMap.begin();
  }
  res.append("&");
  res.append(m_pivot->first);

  return res;
}

void
Ndvr::SendHelloInterest() {
  /* First of all, cancel any previously scheduled events */
  sendhello_event.cancel();

  Name name = Name(kNdvrHelloPrefix);
  name.append(getRouterPrefix());
  name.appendNumber(m_routingTable.size());
  name.append(m_routingTable.GetDigest());
  name.appendNumber(m_routingTable.GetVersion());
  NS_LOG_INFO("Sending Interest " << name);

  Interest interest = Interest();
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setName(name);
  interest.setCanBePrefix(false);
  interest.setInterestLifetime(time::milliseconds(0));
  std::string params;
  if (m_neighMap.size() > 0) {
    auto neighbors_token = GetNeighborToken();
    NS_LOG_INFO("neighbors_token " << neighbors_token);
    params = neighbors_token;
  }
  if (m_enableUnicastFaces) {
    params = params + "&" + m_macaddr;
  }
  if (!params.empty()) {
    interest.setApplicationParameters(reinterpret_cast<const uint8_t*>(params.data()), params.size());
  }

  m_face.expressInterest(interest, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});

  sendhello_event = m_scheduler.schedule(time::seconds(m_helloIntervalCur),
                                        [this] { SendHelloInterest(); });
}

void
Ndvr::UpdateNeighHelloTimeout(NeighborEntry& neighbor) {
  auto diff = neighbor.GetLastSeenDelta();
  time::seconds timeout = time::seconds(3) + 2*std::max(time::seconds(m_helloIntervalIni), diff);
  neighbor.SetHelloTimeout(timeout);
}

void
Ndvr::RescheduleNeighRemoval(NeighborEntry& neighbor) {
  neighbor.removal_event.cancel();
  const std::string neigh_prefix = neighbor.GetName();
  neighbor.removal_event = m_scheduler.schedule(neighbor.GetHelloTimeout(),
                            [this, neigh_prefix] { RemoveNeighbor(neigh_prefix); });
  neighbor.UpdateLastSeen();
}

void
Ndvr::RemoveNeighbor(const std::string neigh) {
  NS_LOG_INFO("Remove neighbor=" << neigh);

  auto neigh_it = m_neighMap.find(neigh);
  if (neigh_it == m_neighMap.end()) {
    return;
  }

  //bool need_adv = false;

  // remove all routes whose next-hop is this neighbor (instead of remove, we increase the cost)
  for (auto it = m_routingTable.begin(); it != m_routingTable.end(); ++it) {
    if (it->second.isNextHop(neigh_it->second.GetFaceId())) {
      it->second.SetCost(neigh_it->second.GetFaceId(),
          std::numeric_limits<uint32_t>::max());
      it->second.IncSeqNum(1);
      //need_adv = true;
    }
    /* For local routes, increment the seqNum by 2 */
    if (it->second.isDirectRoute())
      it->second.IncSeqNum(2);
  }

  // remove from neighbor map
  m_neighMap.erase(neigh);
  m_pivot = m_neighMap.end();

  // insert into recently removed
  // TODO

  //if (need_adv) {
  //  /* schedule a immediate ehlo message to notify neighbors about a new DvInfo */
  //  ResetHelloInterval();
  //  SendHelloInterest();
  //}
}

void Ndvr::SchedDvInfoInterest(NeighborEntry& neighbor, bool wait, uint32_t retx) {
  auto n = neighbor.GetName();

  /* is there any other DvInfo interest scheduled? if so, skip */
  auto n_event = dvinfointerest_event.find(n);
  if (n_event != dvinfointerest_event.end() && n_event->second)
    return;

  /* exponential Backoff */
  /*
  uint32_t max_rand = pow(2, m_c) - 1;
  std::uniform_int_distribution<int> rand(0, max_rand);
  int r = rand(m_rengine);
  int backoffTime = r*m_slotTime;

  if (backoffTime > 500000)
    backoffTime = 500000;

  // random factor to avoid tx collisions
  backoffTime += 10000*m_nodeid;

  NS_LOG_INFO("SchedDvInfoInterest name=" << n << " rand=" << r << " backoffTime=" << backoffTime);
  */

  /* token based Backoff */
  int backoffTime = 0;
  if (wait)
    backoffTime = 750000;
  /* workaround to avoid wifi collisions */
  backoffTime += 10*m_rand->GetValue(0, 19999);
  NS_LOG_INFO("SchedDvInfoInterest name=" << n << " wait=" << wait << " backoffTime=" << backoffTime);

  dvinfointerest_event[n] = m_scheduler.schedule(
                                time::microseconds(backoffTime),
                                [this, n, retx] { SendDvInfoInterest(n, retx); });
}

void
Ndvr::SendDvInfoInterest(const std::string& neighbor_name, uint32_t retx) {
  /* cleanup scheduled event */
  dvinfointerest_event.erase(neighbor_name);

  auto neigh_it = m_neighMap.find(neighbor_name);
  if (neigh_it == m_neighMap.end()) {
    return;
  }
  auto neighbor = neigh_it->second;

  NS_LOG_INFO("Sending DV-Info Interest retx=" << retx << " to neighbor=" << neighbor_name);
  Name name = Name(kNdvrDvInfoPrefix);
  name.append(neighbor_name);
  name.appendNumber(neighbor.GetVersion());

  Interest interest = Interest();
  interest.setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest.setName(name);
  interest.setCanBePrefix(false);
  interest.setMustBeFresh(true);
  interest.setInterestLifetime(time::seconds(m_localRTTimeout));

  m_face.expressInterest(interest,
    std::bind(&Ndvr::OnDvInfoContent, this, _1, _2),
    std::bind(&Ndvr::OnDvInfoNack, this, _1, _2),
    std::bind(&Ndvr::OnDvInfoTimedOut, this, _1, retx));
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

  uint32_t numPrefixes = ExtractNumPrefixesFromAnnounce(interestName);
  std::string digest = ExtractDigestFromAnnounce(interestName);
  uint32_t version = ExtractVersionFromAnnounce(interestName);
  std::vector<std::string> params;
  if (interest.hasApplicationParameters() && interest.getApplicationParameters().value_size() > 0) {
    std::string s;
    s.assign((char *)interest.getApplicationParameters().value(), interest.getApplicationParameters().value_size());
    NS_LOG_INFO("Neighbor=" << neighPrefix <<  " params=" << s);
    boost::split(params, s, boost::is_any_of("&"));
  }
  std::string neigh_mac;
  if (m_enableUnicastFaces) {
    neigh_mac = params.back();
    params.pop_back();
    NS_LOG_INFO("Neighbor_mac == " << neigh_mac);
  }

  auto neigh = m_neighMap.find(neighPrefix);
  bool newNeigh = false;
  if (neigh == m_neighMap.end()) {
    //ResetHelloInterval();
    uint64_t neighFaceId = 0;
    if (m_enableUnicastFaces && !neigh_mac.empty()) {
      auto neighFaceId_it = m_neighToFaceId.find(neighPrefix);
      if (neighFaceId_it == m_neighToFaceId.end()) {
        neighFaceId = CreateUnicastFace(neigh_mac);
        NS_LOG_INFO("NEW-Neighbor_FaceId == " << neighFaceId << " mac = " << neigh_mac);
        m_neighToFaceId.emplace(neighPrefix, neighFaceId);
      } else {
        neighFaceId = neighFaceId_it->second;
        NS_LOG_INFO("EXISTING-Neighbor_FaceId == " << neighFaceId << " mac = " << neigh_mac);
      }
    }
    if (neighFaceId == 0)
      neighFaceId = inFaceId;
    neigh = m_neighMap.emplace(neighPrefix, NeighborEntry(neighPrefix, neighFaceId, version)).first;
    uint64_t oldFaceId = 0;
    registerNeighborPrefix(neigh->second, oldFaceId, neighFaceId);
    newNeigh = true;
  } else {
    NS_LOG_INFO("Already known router, increasing the hello interval");
    if (neigh->second.GetFaceId() != inFaceId) {
      /* Issue #2: TODO: We need to be careful about this because since we are using default multicast forward strategy,
       * mean that one node can forward ndvr messages from other nodes, so the interest might be received from
       * the direct face or from the intermediate forwarder face! Not necessarily means the node moved! */
      //NS_LOG_INFO("Neighbor moved from faceId=" << neigh->second.GetFaceId() << " to faceId=" << inFaceId << " neigh=" << neighPrefix);
      //registerNeighborPrefix(neigh->second, neigh->second.GetFaceId(), inFaceId);
    }
    //IncreaseHelloInterval();
    //return;
  }
  UpdateNeighHelloTimeout(neigh->second);
  RescheduleNeighRemoval(neigh->second);
  if (numPrefixes > 0 && (newNeigh || version > neigh->second.GetVersion()) && numPrefixes >= m_routingTable.size()) {
    neigh->second.SetVersion(version);

    /* does we really have a change? */
    if (digest != "0" && digest == m_routingTable.GetDigest()) {
      NS_LOG_INFO("Same digest, so there was no change! digest=" << digest);
      return;
    }

    /* should we request immediatly or wait? */
    bool wait = true;
    auto it = std::find(params.begin(), params.end(), m_routerPrefix);
    if (it != params.end())
      wait = false;
    SchedDvInfoInterest(neigh->second, wait);
  } else {
    NS_LOG_INFO("Skipped DvInfoInterest numPrefixes=" << numPrefixes << " m_routingTable.size()=" << m_routingTable.size() << " newNeigh=" << newNeigh << " version=" << version << " saved_version=" << neigh->second.GetVersion());
  }
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
  m_signedDataAmountDSK += dvinfo_str.size();
  // Sign and send
  m_keyChain.sign(*data, getSigningInfo());
  m_face.put(*data);
}

void Ndvr::OnKeyInterest(const ndn::Interest& interest) {
  NS_LOG_INFO("Received KEY Interest " << interest.getName());
  std::string nameStr = interest.getName().toUri();
  std::size_t pos = nameStr.find("/KEY/");
  ndn::Name identityName = ndn::Name(nameStr.substr(0, pos));

  try {
    // Create Data packet
    auto cert = m_keyChain.getPib().getIdentity(identityName).getKey(interest.getName()).getDefaultCertificate();

    // Return Data packet to the requester
    m_face.put(cert);
  }
  catch (const std::exception& ) {
    NS_LOG_DEBUG("The certificate: " << interest.getName() << " does not exist! I was looking for Identity=" << identityName);
  }
}

void Ndvr::OnDvInfoTimedOut(const ndn::Interest& interest, uint32_t retx) {
  // TODO: Apply the same logic as in HelloProtocol::processInterestTimedOut (~/mini-ndn/ndn-src/NLSR/src/hello-protocol.cpp)
  // TODO: what if node has moved?
  NS_LOG_DEBUG("Interest timed out for Name: " << interest.getName()<< " retx=" << retx);
  return;

  /* what is the maximum retransmission? Just 1?*/
  if (retx > 1)
    return;

  /* Sanity checks */
  std::string neighPrefix = ExtractRouterPrefix(interest.getName(), kNdvrDvInfoPrefix);
  if (!isValidRouter(interest.getName(), kNdvrDvInfoPrefix) || neighPrefix == m_routerPrefix) {
    return;
  }
  auto neigh_it = m_neighMap.find(neighPrefix);
  if (neigh_it == m_neighMap.end()) {
    return;
  }
  uint32_t version = ExtractVersionFromAnnounce(interest.getName());
  if (version < neigh_it->second.GetVersion()) {
    /* there is a newer version, so no sense retransmite this */
    return;
  }

  /* is it worth to send a DvInfo now instead of waiting the next hello/dvinfo exchange cycle? */
  // if (neigh_it->second.GetLastSeenDelta() >= m_helloIntervalCur -1)
  //   return

  SchedDvInfoInterest(neigh_it->second, retx+1);
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
    NS_LOG_DEBUG("Data signed with: " << data.getSignature().getKeyLocator().getName() << " type=" << data.getSignature().getType());
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

  /* Update lastSeen and reschedule neighbor removal */
  RescheduleNeighRemoval(neigh_it->second);

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

void Ndvr::UpdateRoutingTableDigest() {
  proto::DvInfo to_calc_digest;
  std::string str_digest;
  for (auto it = m_routingTable.begin(); it != m_routingTable.end(); ++it) {
    auto* e2 = to_calc_digest.add_entry();
    e2->set_prefix(it->first);
    e2->set_seq(it->second.GetSeqNum());
    e2->set_cost(0);
  }
  to_calc_digest.AppendToString(&str_digest);
  boost::uuids::detail::sha1 sha1;
  unsigned int hash[5];
  sha1.process_bytes(str_digest.c_str(), str_digest.size());
  sha1.get_digest(hash);
  std::stringstream stream;
  for(std::size_t i=0; i<sizeof(hash)/sizeof(hash[0]); ++i) {
      stream << std::hex << hash[i];
  }
  std::string res = stream.str();
  NS_LOG_DEBUG("RoutingTable_digest: " << res);
  m_routingTable.SetDigest(res);
}

void Ndvr::EncodeDvInfo(std::string& out) {
  proto::DvInfo dvinfo_proto;
  for (auto it = m_routingTable.begin(); it != m_routingTable.end(); ++it) {
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
  bool has_changed = false;

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

      has_changed = true;
      continue;
    }

    /* cost is "infinity", so remove it */
    if (isInfinityCost(neigh_cost)) {
      /* Delete route only if update was received from my nexthop neighbor */
      if (!localRE.isNextHop(neighbor.GetFaceId()))
        continue;

      NS_LOG_INFO("======>> Infinity cost! Remove name prefix" << neigh_prefix);
      m_routingTable.DeleteRoute(localRE, neighbor.GetFaceId());

      has_changed = true;
      continue;
    }

    /* compare the Received and Local SeqNum (in Routing Entry)*/
    neigh_cost = CalculateCostToNeigh(neighbor, neigh_cost);
    if (neigh_seq > localRE.GetSeqNum()) {
      // TODO:
      //   - Recv_Cost == Local_cost: update Local_SeqNum
      //   - Recv_Cost != Local_cost: wait SettlingTime, then update Local_Cost / Local_SeqNum
      if (localRE.GetCost() == neigh_cost) {
        NS_LOG_INFO("======>> New SeqNum same cost, update name prefix! local_seqNum=" << localRE.GetSeqNum() << " neigh_seqNum=" << neigh_seq);
        if (!localRE.isNextHop(neighbor.GetFaceId())) {
          /* TODO: if they have the same cost but from different faces, could save for multipath */
        }
        localRE.SetSeqNum(neigh_seq);
      } else {
        NS_LOG_INFO("======>> New SeqNum diff cost, update name prefix! local_seqNum=" << localRE.GetSeqNum() << " neigh_seqNum=" << neigh_seq << " local_cost=" << localRE.GetCost() << " neigh_cost=" << neigh_cost);
        /* Cost change will be handle by periodic updates */
        localRE.SetCost(neigh_cost);
        localRE.SetSeqNum(neigh_seq);
        m_routingTable.UpdateRoute(localRE, neighbor.GetFaceId());
        has_changed = true;
      }
    } else if (neigh_seq == localRE.GetSeqNum() && neigh_cost < localRE.GetCost()) {
      NS_LOG_INFO("======>> Equal SeqNum but Better Cost, update name prefix! local_cost=" << localRE.GetCost());
      /* Cost change will be handle by periodic updates */
      // TODO: wait SettlingTime, then update Local_Cost
      localRE.SetCost(neigh_cost);
      m_routingTable.UpdateRoute(localRE, neighbor.GetFaceId());
      has_changed = true;
    } else if (neigh_seq == localRE.GetSeqNum() && neigh_cost >= localRE.GetCost()) {
      //NS_LOG_INFO("======>> Equal SeqNum and (Equal or Worst Cost), however learn name prefix! local_cost=" << localRE.GetCost());
      // TODO: save this new prefix as well to multipath
    } else {
      /* Recv_SeqNum < Local_SeqNu: discard/next, we already have a most recent update */
      continue;
    }
  }

  if (has_changed) {
    m_routingTable.IncVersion();
    //UpdateRoutingTableDigest();
    /* schedule a immediate ehlo message to notify neighbors about a new DvInfo */
    //ResetHelloInterval();
    //SendHelloInterest();
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
  m_routingTable.IncVersion();
  //if (sendhello_event) {
  //  ResetHelloInterval();
  //  SendHelloInterest();
  //}
}

uint64_t Ndvr::CreateUnicastFace(std::string mac) {
  ns3::Ptr<ns3::Node> thisNode = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
  ns3::Ptr<ns3::NetDevice> netDevice = thisNode->GetDevice(0);
  ns3::Ptr<ns3::ndn::L3Protocol> ndn = thisNode->GetObject<ns3::ndn::L3Protocol>();
  std::string myUrl = "netdev://[" + boost::lexical_cast<std::string>(ns3::Mac48Address::ConvertFrom(netDevice->GetAddress())) + "]";
  //std::string neighUri = "netdev://[" + mac + "]";

  // Create an ndnSIM-specific transport instance
  ::nfd::face::GenericLinkService::Options opts;
  opts.allowFragmentation = true;
  opts.allowReassembly = true;
  opts.allowCongestionMarking = true;

  auto linkService = std::make_unique<::nfd::face::GenericLinkService>(opts);


  auto transport = std::make_unique<ns3::ndn::UnicastNetDeviceTransport>(thisNode, netDevice,
                                                   myUrl,
                                                   //neighUri);
                                                   mac);
  auto face = std::make_shared<::nfd::face::Face>(std::move(linkService), std::move(transport));
  face->setMetric(1);

  ndn->addFace(face);

  return face->getId();
}
} // namespace ndvr
} // namespace ndn
