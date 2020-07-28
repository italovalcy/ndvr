/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NRDV_HPP
#define NRDV_HPP

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>

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

  Nrdv(ndn::KeyChain& keyChain) 
    : m_keyChain(keyChain)
    , m_scheduler(m_face.getIoService())
  {
    // register prefix and set interest filter on producer face
    //m_face.setInterestFilter("/hello", std::bind(&Nrdv::respondToAnyInterest, this, _2),
    //                                 std::bind([]{}), std::bind([]{}));
    m_face.setInterestFilter("/nrdv", std::bind(&Nrdv::OnHelloInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        throw Error("Failed to register sync interest prefix: " + reason);
    });

    // use scheduler to send interest later on consumer face
    m_scheduler.schedule(ndn::time::seconds(2), [this] {
        m_face.expressInterest(ndn::Interest("/nrdv/helloworld"),
                                       std::bind([] { std::cout << "Hello!" << std::endl; }),
                                       std::bind([] { std::cout << "NACK!" << std::endl; }),
                                       std::bind([] { std::cout << "Bye!.." << std::endl; }));
      });
  }
  void run() {
    m_face.processEvents(); // ok (will not block and do nothing)
    // m_faceConsumer.getIoService().run(); // will crash
  }

  void Start();
//  {
//  }
  void Stop() {
  }

private:
  void OnHelloInterest(const ndn::Interest& interest) {
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

private:
  ndn::KeyChain& m_keyChain;
  ndn::Face m_face;
  ndn::Scheduler m_scheduler;
};

} // namespace nrdv
} // namespace ndn


#endif // NRDV
