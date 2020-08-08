/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NRDV_HPP
#define NRDV_HPP


#include <iostream>
#include <map>
#include <string>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ns3/core-module.h>
#include <ns3/random-variable-stream.h>

#include "dvinfo.hpp"
#include "nrdv-message.pb.h"
#include "nrdv-helper.hpp"

namespace ndn {
namespace nrdv {

static const Name kNrdvPrefix = Name("/nrdv");
static const Name kNrdvHelloPrefix = Name("/nrdv/ehlo");
static const Name kNrdvDvInfoPrefix = Name("/nrdv/dvinfo");
static const Name kNrdvKeyPrefix = Name("/nrdv/key");
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
  void IncVersion() {
    m_version++;
  }
  uint64_t GetVersion() {
    return m_version;
  }
  uint64_t GetNextVersion() {
    return m_version++;
  }
  void SetFaceId(uint64_t faceId) {
    m_faceId = faceId;
  }
  uint64_t GetFaceId() {
    return m_faceId;
  }
private:
  std::string m_name;
  uint64_t m_faceId;
  uint64_t m_version;
  //TODO: lastSeen
  //TODO: key  
};

class Error : public std::exception {
public:
  Error(const std::string& what) : what_(what) {}
  virtual const char* what() const noexcept override { return what_.c_str(); }
private:
  std::string what_;
};

class Nrdv
{
public:
  Nrdv(ndn::KeyChain& keyChain, Name network, Name routerName, std::vector<std::string>& np);
  void run();
  void Start();
  void Stop();

  const ndn::Name&
  getRouterPrefix() const
  {
    return m_routerPrefix;
  }

private:
  typedef std::map<std::string, NeighborEntry> NeighborMap;

  void processInterest(const ndn::Interest& interest);
  void OnHelloInterest(const ndn::Interest& interest, uint64_t inFaceId);
  void OnKeyInterest(const ndn::Interest& interest);
  void OnDvInfoInterest(const ndn::Interest& interest);
  void OnDvInfoContent(const ndn::Interest& interest, const ndn::Data& data);
  void OnDvInfoTimedOut(const ndn::Interest& interest);
  void OnDvInfoNack(const ndn::Interest& interest, const ndn::lp::Nack& nack);
  void SendDvInfoInterest(NeighborEntry& neighbor);
  void SendHelloInterest();
  void registerPrefixes();
  void printFib();
  bool isInfinityCost(uint32_t cost);
  bool isValidCost(uint32_t cost);
  void processDvInfoFromNeighbor(NeighborEntry& neighbor, DvInfoMap& dvinfo_other);
  uint32_t CalculateCostToNeigh(NeighborEntry&, uint32_t cost);
  void IncreaseHelloInterval();
  void ResetHelloInterval();

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
   *    /NRDV/<type>/<network>/%C1.Router/<router_name>
   * @param prefix: The prefix type: Hello, DvInfo or Key
   *
   * Example: 
   *    Input-name: /NRDV/HELLO/ufba/%C1.Router/Router1
   *    Input-prefix: /NRDV/HELLO
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
   *    /NRDV/<type>/<network>/%C1.Router/<router_name>
   * @param prefix: The prefix type: Hello, DvInfo or Key
   *
   * Example 1: 
   *    Input-name: /NRDV/HELLO/ufba/%C1.Router/Router1
   *    Input-prefix: /NRDV/HELLO
   *    Returns: /ufba/%C1.Router/Router1
   */
  std::string ExtractRouterPrefix(const Name& name, const Name& prefix) {
    return name.getSubName(prefix.size(), 3).toUri();
  }

private:
  ndn::KeyChain& m_keyChain;
  ndn::Scheduler m_scheduler;
  uint32_t m_seq;
  ns3::Ptr<ns3::UniformRandomVariable> m_rand; ///< @brief nonce generator
  Name m_network;
  Name m_routerName;
  ndn::Face m_face;

  Name m_routerPrefix;
  NeighborMap m_neighMap;
  DvInfoMap m_dvinfo;
  DvInfoMap m_dvinfoLearned;
  int m_helloIntervalIni;
  int m_helloIntervalCur;
  int m_helloIntervalMax;
  int m_dvinfoInterval;
  int m_dvinfoTimeout;

  scheduler::EventId sendhello_event;  /* async send hello event scheduler */
  scheduler::EventId increasehellointerval_event;  /* increase hello interval event scheduler */
};

} // namespace nrdv
} // namespace ndn

#endif // NRDV
