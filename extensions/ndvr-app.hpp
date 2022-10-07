/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NDVR_APP_HPP
#define NDVR_APP_HPP

#include "ndvr.hpp"

#include "ns3/core-module.h"
#include "ns3/application.h"
#include <ns3/simulator.h>
#include <ns3/log.h>
#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/node-list.h>
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"

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
      .AddAttribute("RouterName", "Name of the router in ndn URI format", StringValue("/%C1.Router/router1"),
                    ndn::MakeNameAccessor(&NdvrApp::routerName_), ndn::MakeNameChecker())
      .AddAttribute("ValidationConfig", "Secutiry Validation config file", StringValue("config/validation.conf"),
                    MakeStringAccessor(&NdvrApp::validationConfig_), MakeStringChecker())
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
  
  void getFacesFromNetdev() {
    using namespace ns3;
    using namespace ns3::ndn;
    Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());
    for (uint32_t deviceId = 0; deviceId < thisNode->GetNDevices(); deviceId++) {
      Ptr<NetDevice> device = thisNode->GetDevice(deviceId);
      Ptr<L3Protocol> ndn = thisNode->GetObject<L3Protocol>();
      NS_ASSERT_MSG(ndn != nullptr, "Ndn stack should be installed on the node");
      auto face = ndn->getFaceByNetDevice(device);
      NS_ASSERT_MSG(face != nullptr, "There is no face associated with the net-device");
      faces_.push_back(std::to_string(face->getId()));
    }
  }

protected:
  virtual void StartApplication() {
    getFacesFromNetdev();
    m_instance.reset(new ::ndn::ndvr::Ndvr(signingInfo_, network_, routerName_, namePrefixes_, faces_, validationConfig_));
    m_instance->EnableUnicastFaces(unicastFaces_);
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
  std::string validationConfig_;
  std::vector<std::string> faces_;
};

} // namespace ns3

#endif // NDVR_APP_HPP
