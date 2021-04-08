/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NDVR_HPP
#define NDVR_HPP


#include <iostream>
#include <map>
#include <unordered_map>
#include <string>
#include <random>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/security/validator-config.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/time.hpp>
#include <ns3/core-module.h>
#include <ns3/random-variable-stream.h>

#include "routing-table.hpp"
#include "ndvr-message.pb.h"
#include "ndvr-message-helper.hpp"

namespace ndn {
namespace ndvr {

static const Name kNdvrPrefix = Name("/localhop/ndvr");
static const Name kNdvrHelloPrefix = Name("/localhop/ndvr/dvannc");
static const Name kNdvrDvInfoPrefix = Name("/localhop/ndvr/dvinfo");
static const std::string kRouterTag = "\%C1.Router";


class NeighborEntry {
public:
  NeighborEntry()
  {
  }

  NeighborEntry(std::string name, uint64_t faceId, uint64_t ver)
    : m_name(name)
    , m_faceId(faceId)
    , m_version(ver)
    , m_lastSeen(time::steady_clock::now())
  {
  }

  ~NeighborEntry()
  {
  }

  void SetName(std::string name) {
    m_name = name;
  }
  std::string GetName() {
    return m_name;
  }

  void SetVersion(uint64_t ver) {
    m_version = ver;
  }
  uint64_t GetVersion() {
    return m_version;
  }

  void SetFaceId(uint64_t faceId) {
    m_faceId = faceId;
  }
  uint64_t GetFaceId() {
    return m_faceId;
  }
  void UpdateLastSeen() {
    m_lastSeen = time::steady_clock::now();
  }
  time::seconds GetLastSeenDelta() {
    return time::duration_cast<time::seconds>(time::steady_clock::now() - m_lastSeen);
  }
  void SetHelloTimeout(time::seconds t) {
    m_helloTimeout = t;
  }
  time::seconds GetHelloTimeout() {
    return m_helloTimeout;;
  }
public:
  scheduler::EventId removal_event;
private:
  std::string m_name;
  uint64_t m_faceId;
  uint64_t m_version;
  time::steady_clock::TimePoint m_lastSeen;
  time::seconds m_helloTimeout;
  //TODO: key  
};

class Error : public std::exception {
public:
  Error(const std::string& what) : what_(what) {}
  virtual const char* what() const noexcept override { return what_.c_str(); }
private:
  std::string what_;
};

class Ndvr
{
public:
  Ndvr(const ndn::security::SigningInfo& signingInfo, Name network, Name routerName, std::vector<std::string>& np);
  void run();
  void Start();
  void Stop();
  void AdvNamePrefix(std::string name);

  const ndn::Name&
  getRouterPrefix() const
  {
    return m_routerPrefix;
  }

  void EnableUnicastFaces(bool flag) {
    m_enableUnicastFaces = flag;
  }

  void EnableDSK(bool flag) {
    m_enableDSK = flag;
  }

  void SetMaxSecsDSK(uint32_t x) {
    m_maxSecsDSK = x;
  }

  void SetMaxSizeDSK(uint32_t x) {
    m_maxSizeDSK = x;
  }

private:
  typedef std::map<std::string, NeighborEntry> NeighborMap;

  void processInterest(const ndn::Interest& interest);
  void OnHelloInterest(const ndn::Interest& interest, uint64_t inFaceId);
  void OnKeyInterest(const ndn::Interest& interest);
  void OnDvInfoInterest(const ndn::Interest& interest);
  void ReplyDvInfoInterest(const ndn::Interest& interest);
  void OnDvInfoContent(const ndn::Interest& interest, const ndn::Data& data);
  void OnDvInfoTimedOut(const ndn::Interest& interest, uint32_t retx);
  void OnDvInfoNack(const ndn::Interest& interest, const ndn::lp::Nack& nack);
  void SchedDvInfoInterest(NeighborEntry& neighbor, bool wait = false, uint32_t retx = 0);
  void SendDvInfoInterest(const std::string& neighbor_name, uint32_t retx = 0);
  void OnValidatedDvInfo(const ndn::Data& data);
  void OnDvInfoValidationFailed(const ndn::Data& data, const ndn::security::v2::ValidationError& ve);
  void SendHelloInterest();
  void registerPrefixes();
  void registerNeighborPrefix(NeighborEntry& neighbor, uint64_t oldFaceId, uint64_t newFaceId);
  bool isInfinityCost(uint32_t cost);
  bool isValidCost(uint32_t cost);
  void EncodeDvInfo(std::string& out);
  void processDvInfoFromNeighbor(NeighborEntry& neighbor, RoutingTable& dvinfo_other);
  uint32_t CalculateCostToNeigh(NeighborEntry&, uint32_t cost);
  void IncreaseHelloInterval();
  void ResetHelloInterval();
  uint64_t ExtractIncomingFace(const ndn::Interest& interest);
  uint64_t ExtractIncomingFace(const ndn::Data& data);
  void UpdateNeighHelloTimeout(NeighborEntry& neighbor);
  void RescheduleNeighRemoval(NeighborEntry& neighbor);
  void RemoveNeighbor(const std::string neigh);
  uint64_t CreateUnicastFace(std::string mac);
  std::string GetNeighborToken();
  void UpdateRoutingTableDigest();
  void ManageSigningInfo();
  void createDSK(std::string subjectName);
  const ndn::security::SigningInfo& getSigningInfo();

  void
  buildRouterPrefix()
  {
    m_routerPrefix = m_network;
    m_routerPrefix.append(m_routerName);
  }
  
  /** @brief check if it is a valid router by extracting the router tag 
   * (command marker) from Interest name and comparing with %C1.Router
   *
   * @param name: The interest name received from a neighbor. It 
   * should be formatted:
   *    /NDVR/<type>/<network>/%C1.Router/<router_name>
   * @param prefix: The prefix type: Hello, DvInfo or Key
   *
   * Example: 
   *    Input-name: /NDVR/HELLO/ufba/%C1.Router/Router1
   *    Input-prefix: /NDVR/HELLO
   *    Returns: true
   */
  bool isValidRouter(const Name& name, const Name& prefix) {
    return name.get(prefix.size()+1).toUri() == kRouterTag;
  }

  /** @brief Extracts the router prefix from Interest packet
   * of a specifc type.
   *
   * @param name: The interest name received from a neighbor. It 
   * should be formatted:
   *    /NDVR/<type>/<network>/%C1.Router/<router_name>
   * @param prefix: The prefix type: Hello, DvInfo or Key
   *
   * Example 1: 
   *    Input-name: /NDVR/HELLO/ufba/%C1.Router/Router1
   *    Input-prefix: /NDVR/HELLO
   *    Returns: /ufba/%C1.Router/Router1
   */
  std::string ExtractRouterPrefix(const Name& name, const Name& prefix) {
    return name.getSubName(prefix.size(), 3).toUri();
  }

  /** @brief Extracts the number of prefixes annouced by the neighbor
   *
   * @param name: The interest name received from a neighbor. It 
   * should be formatted:
   *    <NDVR_HELLO_PREFIX>/<network>/%C1.Router/<router_name>/<num_prefix>/<digest>/<version>(/<params>?)
   */
  uint32_t ExtractNumPrefixesFromAnnounce(const Name& name) {
    return name.get(kNdvrHelloPrefix.size()+3).toNumber();
  }

  /** @brief Extracts the digest annouced by the neighbor
   *
   * @param name: The interest name received from a neighbor. It 
   * should be formatted:
   *    <NDVR_HELLO_PREFIXX>/<network>/%C1.Router/<router_name>/<num_prefix>/<digest>/<version>(/<params>?)
   */
  std::string ExtractDigestFromAnnounce(const Name& name) {
    return name.get(kNdvrHelloPrefix.size()+3+1).toUri();
  }

  /** @brief Extracts the version annouced by the neighbor
   *
   * @param name: The interest name received from a neighbor. It 
   * should be formatted:
   *    <NDVR_HELLO_PREFIXX>/<network>/%C1.Router/<router_name>/<num_prefix>/<digest>/<version>(/<params>?)
   */
  uint32_t ExtractVersionFromAnnounce(const Name& name) {
    return name.get(kNdvrHelloPrefix.size()+3+2).toNumber();
  }

  time::seconds getSecsSinceLastDSKCert() {
    return time::duration_cast<time::seconds>(time::steady_clock::now() - m_lastDSKCert);
  }

private:
  const ndn::security::SigningInfo& m_signingInfo;
  ndn::Scheduler m_scheduler;
  ndn::ValidatorConfig m_validator;
  uint32_t m_seq;
  ns3::Ptr<ns3::UniformRandomVariable> m_rand; ///< @brief nonce generator
  Name m_network;
  Name m_routerName;
  ndn::Face m_face;

  ndn::KeyChain m_keyChain;
  Name m_routerPrefix;
  NeighborMap m_neighMap;
  std::map<std::string, uint64_t> m_neighToFaceId;
  RoutingTable m_routingTable;
  int m_helloIntervalIni;
  int m_helloIntervalCur;
  int m_helloIntervalMax;
  int m_localRTInterval;
  int m_localRTTimeout;
  bool m_enableUnicastFaces = true;
  std::string m_macaddr;
  /* m_slotTime (microseconds)
   * SlotTime is the time to transmit a frame on the physical medium
   * (e.g., 802.3 100Mbps is 51us, 802.11b is 20us, 802.11ac is 9us,
   * etc.). Should be a configuration parameter */
  uint32_t m_slotTime = 40000;
  /* m_c
   * Let m_c be the cth try to avoid sending a redundant DvInfo interest.
   * It starts with 1 and can increase if we still detect redundant
   * DvInfo interest.
   * */
  uint32_t m_c = 4;
  /* For DvInfo interest suppression */
  std::unordered_map<std::string, scheduler::EventId> dvinfointerest_event;
  /* Signing Key separation into long term and short term keys (i.e.,
   * KSK - Key signing key and DSK - Data signing key) */
  bool m_enableDSK = false;
  uint32_t m_maxSecsDSK = 0;
  uint32_t m_maxSizeDSK = 0;
  ndn::security::SigningInfo m_signingInfoDSK = ndn::security::SigningInfo();
  time::steady_clock::TimePoint m_lastDSKCert = time::steady_clock::TimePoint::max();
  uint64_t m_signedDataAmountDSK = 0;

  scheduler::EventId sendhello_event;  /* async send hello event scheduler */
  scheduler::EventId increasehellointerval_event;  /* increase hello interval event scheduler */
  scheduler::EventId replydvinfo_event;  /* group dvinfo replies to avoid duplicate */
  scheduler::EventId managesigninginfo_event;  /* manage signing info (check and update if needed) */
  std::random_device rdevice_;
  std::mt19937 m_rengine;
  std::uniform_int_distribution<> replydvinfo_dist = std::uniform_int_distribution<>(100, 150);   /* milliseconds */
  int data_generation_rate_mean = 40000;
  std::poisson_distribution<> m_data_gen_dist = std::poisson_distribution<>(data_generation_rate_mean);
  std::uniform_int_distribution<> packet_dist = std::uniform_int_distribution<>(10000, 15000);   /* microseconds */

  /* m_pivot (int) - iteractor for the circular list of neighbors
   * which points to the next neighbors with priority to get DvInfo */
  NeighborMap::iterator m_pivot;
};

} // namespace ndvr
} // namespace ndn

#endif // NDVR
