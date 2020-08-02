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
#include "ns3/core-module.h"
#include "ns3/random-variable-stream.h"

namespace ndn {
namespace nrdv {

static const Name kNrdvPrefix = Name("/nrdv");
static const Name kNrdvHelloPrefix = Name("/nrdv/hello");
static const Name kNrdvDataPrefix = Name("/nrdv/data");
static const Name kNrdvKeyPrefix = Name("/nrdv/key");


class NeighborEntry {
public:
  NeighborEntry()
  {
  }

  NeighborEntry(std::string name, uint64_t ver)
    : m_name(name)
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
private:
  std::string m_name;
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
  Nrdv(ndn::KeyChain& keyChain, Name network, Name routerName);
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
  void OnHelloInterest(const ndn::Interest& interest);
  void OnDataInterest(const ndn::Interest& interest);
  void OnKeyInterest(const ndn::Interest& interest);
  void SendHello();
  void ScheduleNextHello();
  void registerPrefixes();
  void printFib();

  void
  buildRouterPrefix()
  {
    m_routerPrefix = m_network;
    m_routerPrefix.append(m_routerName);
  }

  /** @brief Extracts the neighbor name from a Hello Interest packet.
   *
   * \param interestName The interest name received on a Hello packet from
   * a neighbor. It should be formatted:
   *    /NRDV/HELLO/<network>/%C1.Router/<router_name>
   *
   * Example: 
   *    Input: /NRDV/HELLO/ufba/%C1.Router/Router1
   *    Returns: /%C1.Router/Router1
   */
  std::string ExtractNeighborNameFromHello(const Name& n) {
    return n.getSubName(kNrdvHelloPrefix.size()+1, 2).toUri();
  }
  
  /** @brief Extracts the router tag (command marker) from Interest name
   *
   * \param interestName The interest name received on a Hello packet from
   * a neighbor. It should be formatted:
   *    /NRDV/HELLO/<network>/%C1.Router/<router_name>
   *
   * Example: 
   *    Input: /NRDV/HELLO/ufba/%C1.Router/Router1
   *    Returns: %C1.Router
   *
   */
  std::string ExtractRouterTagFromHello(const Name& n) {
    return n.get(-2).toUri();
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
  int m_helloIntervalIni;
  int m_helloIntervalCur;
  int m_helloIntervalMax;
};

} // namespace nrdv
} // namespace ndn

#endif // NRDV
