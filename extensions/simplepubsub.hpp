/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef SIMPLEPUBSUB_HPP
#define SIMPLEPUBSUB_HPP


#include <iostream>
#include <unordered_set>
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

namespace ndn {
namespace ndvr {

class SimplePubSub
{
public:
  SimplePubSub();
  void run();
  void Start();
  void Stop();

  void SetSyncDataRounds(uint32_t x) {
    m_syncDataRounds = x;
  }

private:
  void registerPrefixes();
  void RequestSyncData(const std::string name, uint32_t retx = 0);
  void AddNewNamePrefix(uint32_t round);
  void OnSyncDataTimedOut(const ndn::Interest& interest, uint32_t retx);
  void OnSyncDataNack(const ndn::Interest& interest, const ndn::lp::Nack& nack);
  void OnSyncDataContent(const ndn::Interest& interest, const ndn::Data& data);
  void SendSyncNotify(const std::string nameStr);
  void OnSyncNotify(const ndn::Interest& interest);
  void CheckPendingSync();

private:
  ndn::Scheduler m_scheduler;
  ndn::Face m_face;
  ndn::ValidatorConfig m_validator;

  uint32_t m_nodeid;
  ndn::KeyChain m_keyChain;
  uint32_t m_syncDataRounds = 0;

  std::random_device rdevice_;
  std::mt19937 m_rengine;
  int data_generation_rate_mean = 40000;
  std::poisson_distribution<> m_data_gen_dist = std::poisson_distribution<>(data_generation_rate_mean);
  ns3::Ptr<ns3::UniformRandomVariable> m_rand; ///< @brief nonce generator

  std::unordered_set<std::string> m_pendingSync;
  std::unordered_set<std::string> m_satisfiedSync;
};

} // namespace ndvr
} // namespace ndn

#endif // SIMPLEPUBSUB_HPP
