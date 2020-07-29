/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NRDV_APP_HPP
#define NRDV_APP_HPP

#include "nrdv.hpp"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/application.h"

namespace ns3 {

// Class inheriting from ns3::Application
//class NrdvApp : public ndn::App
class NrdvApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("NrdvApp")
      .SetParent<Application>()
      .AddConstructor<NrdvApp>()
      .AddAttribute("Network", "Name of the network the router belongs to in ndn URI format", StringValue("/ndn"),
                    ndn::MakeNameAccessor(&NrdvApp::network_), ndn::MakeNameChecker())
      .AddAttribute("RouterName", "Name of the router in ndn URI format", StringValue("/\%C1.Router/router1"),
                    ndn::MakeNameAccessor(&NrdvApp::routerName_), ndn::MakeNameChecker());
    return tid;
  }

protected:
  virtual void StartApplication() {
    m_instance.reset(new ::ndn::nrdv::Nrdv(ndn::StackHelper::getKeyChain(), network_, routerName_));
    m_instance->Start();
  }

  virtual void StopApplication() {
    m_instance->Stop();
    m_instance.reset();
  }

private:
  std::unique_ptr<::ndn::nrdv::Nrdv> m_instance;
  ndn::Name network_;
  ndn::Name routerName_;
};

} // namespace ns3

#endif // NRDV_APP_HPP
