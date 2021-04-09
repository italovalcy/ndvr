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
      .AddAttribute("SyncDataRounds", "Deprecated: Number of rounds to run the sync data process", IntegerValue(0),
                    MakeIntegerAccessor(&NdvrApp::syncDataRounds_), MakeIntegerChecker<int32_t>())
      .AddAttribute("EnableUnicastFace", "Enable dynamic creating unicast faces", BooleanValue(false),
                    MakeBooleanAccessor(&NdvrApp::unicastFaces_), MakeBooleanChecker());
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

  void EnableDSKMaxSecs(uint32_t x) {
    maxSecsDSK_ = x;
  }
  void EnableDSKMaxSize(uint32_t x) {
    maxSizeDSK_ = x;
  }

protected:
  virtual void StartApplication() {
    m_instance.reset(new ::ndn::ndvr::Ndvr(signingInfo_, network_, routerName_, namePrefixes_));
    m_instance->EnableUnicastFaces(unicastFaces_);
    if (maxSecsDSK_!=0 || maxSizeDSK_!=0) {
      m_instance->EnableDSK(true);
      m_instance->SetMaxSizeDSK(maxSizeDSK_);
      m_instance->SetMaxSecsDSK(maxSecsDSK_);
    }
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
  uint32_t syncDataRounds_;      // number of rounds to sync data (for data sync experiment)
  bool unicastFaces_;
  uint32_t maxSecsDSK_ = 0;
  uint32_t maxSizeDSK_ = 0;
};

} // namespace ns3

#endif // NDVR_APP_HPP
