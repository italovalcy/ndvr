/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "nrdv.hpp"
#include "ns3/simulator.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("ndn.Nrdv");

namespace ndn {
namespace nrdv {

Nrdv::Nrdv(ndn::KeyChain& keyChain) 
  : m_keyChain(keyChain)
  , m_scheduler(m_face.getIoService())
  , m_seq(0)
  , m_rand(CreateObject<UniformRandomVariable>())
  , m_helloName("/nrdv")
{
  m_face.setInterestFilter("/nrdv", std::bind(&Nrdv::OnHelloInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      throw Error("Failed to register sync interest prefix: " + reason);
  });

  // use scheduler to send interest later on consumer face
  //m_scheduler.schedule(ndn::time::seconds(2), [this] {
  //    m_face.expressInterest(ndn::Interest("/nrdv/helloworld"),
  //                                   std::bind([] { std::cout << "Hello!" << std::endl; }),
  //                                   std::bind([] { std::cout << "NACK!" << std::endl; }),
  //                                   std::bind([] { std::cout << "Bye!.." << std::endl; }));
  //  });
}

void Nrdv::Start() {
  SendHello();
}

void Nrdv::Stop() {
}

void Nrdv::run() {
  m_face.processEvents(); // ok (will not block and do nothing)
  // m_faceConsumer.getIoService().run(); // will crash
}

void
Nrdv::ScheduleNextHello() {
  Simulator::Schedule(Seconds(1.0), &Nrdv::SendHello, this);
}

void
Nrdv::SendHello() {
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_helloName);
  nameWithSequence->appendSequenceNumber(m_name);
  nameWithSequence->appendSequenceNumber(m_seq++);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setCanBePrefix(false);
  time::milliseconds interestLifeTime(1000);
  interest->setInterestLifetime(interestLifeTime);

  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
  NS_LOG_INFO("> Interest for " << m_seq);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);

  ScheduleNextHello();
}

void Nrdv::OnHelloInterest(const ndn::Interest& interest) {
  std::cout << "create data" << std::endl;
  auto data = std::make_shared<ndn::Data>(interest.getName());
  std::cout << "set freshness" << std::endl;
  data->setFreshnessPeriod(ndn::time::milliseconds(1000));
  std::cout << "set content" << std::endl;
  data->setContent(std::make_shared< ::ndn::Buffer>(1024));
  std::cout << "sign" << std::endl;
  m_keyChain.sign(*data);
  std::cout << "put" << std::endl;
  m_face.put(*data);
}

} // namespace nrdv
} // namespace ndn
