/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NDVR_APP_HPP
#define NDVR_APP_HPP

#include "ndvr.hpp"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/application.h"

namespace ns3 {

// Class inheriting from ns3::Application
class NdvrApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("NdvrApp")
      .SetParent<Application>()
      .AddConstructor<NdvrApp>()
      .AddAttribute("Network", "Name of the network the router belongs to in ndn URI format", StringValue("/ndn"),
                    ndn::MakeNameAccessor(&NdvrApp::network_), ndn::MakeNameChecker())
      .AddAttribute("RouterName", "Name of the router in ndn URI format", StringValue("/\%C1.Router/router1"),
                    ndn::MakeNameAccessor(&NdvrApp::routerName_), ndn::MakeNameChecker())
      .AddAttribute("SyncDataOnAddRoute", "If true, request data upon adding a route", BooleanValue(false),
                    MakeBooleanAccessor(&NdvrApp::syncDataOnAddRoute_), MakeBooleanChecker());
    return tid;
  }

  /* Initial name prefixes to be advertised since the begining */
  void AddNamePrefix(std::string name) {
    namePrefixes_.push_back(name);
  }

  /* Advertise a name prefix any time during operation */
  void AdvNamePrefix(std::string& name) {
    m_instance->AdvNamePrefix(name);
  }

  void AddSigningInfo(::ndn::security::SigningInfo signingInfo) {
    signingInfo_ = signingInfo;
  }

protected:
  virtual void StartApplication() {
    m_instance.reset(new ::ndn::ndvr::Ndvr(signingInfo_, network_, routerName_, namePrefixes_));
    m_instance->EnableSyncDataOnAddRoute(syncDataOnAddRoute_);
    m_instance->Start();
  }

  virtual void StopApplication() {
    m_instance->Stop();
    m_instance.reset();
  }

private:
  std::unique_ptr<::ndn::ndvr::Ndvr> m_instance;
  ::ndn::security::SigningInfo signingInfo_;
  ndn::Name network_;
  ndn::Name routerName_;
  std::vector<std::string> namePrefixes_;
  bool syncDataOnAddRoute_;
};

} // namespace ns3

#endif // NDVR_APP_HPP
