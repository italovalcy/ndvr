/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef RANGECONSUMER_HPP
#define RANGECONSUMER_HPP


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

class Error : public std::exception {
public:
  Error(const std::string& what) : what_(what) {}
  virtual const char* what() const noexcept override { return what_.c_str(); }
private:
  std::string what_;
};

class RangeConsumer
{
public:
  RangeConsumer(std::string prefix, uint32_t first, uint32_t last, uint32_t frequency);
  void run();
  void Start();
  void Stop();

private:
  void RequestSyncData(const std::string name, uint32_t seq = 0);
  void OnSyncDataTimedOut(const ndn::Interest& interest);
  void OnSyncDataNack(const ndn::Interest& interest, const ndn::lp::Nack& nack);
  void OnSyncDataContent(const ndn::Interest& interest, const ndn::Data& data);

private:
  ndn::Scheduler m_scheduler;
  ndn::Face m_face;
  ndn::ValidatorConfig m_validator;
  
  std::random_device rdevice_;
  std::mt19937 m_rengine;
  ns3::Ptr<ns3::UniformRandomVariable> m_rand; ///< @brief nonce generator

  uint32_t m_nodeid;
  ndn::KeyChain m_keyChain;
  std::string m_prefix;
  uint32_t m_first;
  uint32_t m_last;
  uint32_t m_frequency;
};

} // namespace ndvr
} // namespace ndn

#endif // RANGECONSUMER_HPP
