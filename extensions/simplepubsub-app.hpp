/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef SIMPLEPUBSUB_APP_HPP
#define SIMPLEPUBSUB_APP_HPP

#include "simplepubsub.hpp"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/application.h"

namespace ns3 {

// Class inheriting from ns3::Application
class SimplePubSubApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("SimplePubSubApp")
      .SetParent<Application>()
      .AddConstructor<SimplePubSubApp>()
      .AddAttribute("SyncDataRounds", "Number of rounds to run the sync data process", IntegerValue(0),
                    MakeIntegerAccessor(&SimplePubSubApp::syncDataRounds_), MakeIntegerChecker<int32_t>());
    return tid;
  }

protected:
  virtual void StartApplication() {
    m_instance.reset(new ::ndn::ndvr::SimplePubSub());
    m_instance->SetSyncDataRounds(syncDataRounds_);
    m_instance->Start();
  }

  virtual void StopApplication() {
    m_instance->Stop();
    m_instance.reset();
  }

private:
  std::unique_ptr<::ndn::ndvr::SimplePubSub> m_instance;
  uint32_t syncDataRounds_;      // number of rounds to sync data (for data sync experiment)
};

} // namespace ns3

#endif // SIMPLEPUBSUB_APP_HPP
