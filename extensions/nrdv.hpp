/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NRDV_HPP
#define NRDV_HPP

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include "ns3/random-variable-stream.h"

#include <iostream>

namespace ndn {
namespace nrdv {

class Nrdv
{
public:
  class Error : public std::exception {
   public:
    Error(const std::string& what) : what_(what) {}
    virtual const char* what() const noexcept override { return what_.c_str(); }
   private:
    std::string what_;
  };

  Nrdv(ndn::KeyChain& keyChain);
  void run();
  void Start();
  void Stop();

private:
  void OnHelloInterest(const ndn::Interest& interest);
  void SendHello();
  void ScheduleNextHello();

private:
  ndn::KeyChain& m_keyChain;
  ndn::Face m_face;
  ndn::Scheduler m_scheduler;
  Ptr<UniformRandomVariable> m_rand; ///< @brief nonce generator
  uint32_t m_seq;
  Name m_name;
  Name m_helloName;
};

} // namespace nrdv
} // namespace ndn

#endif // NRDV
