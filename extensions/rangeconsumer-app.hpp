/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef RANGECONSUMER_APP_HPP
#define RANGECONSUMER_APP_HPP

#include "rangeconsumer.hpp"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/application.h"

namespace ns3 {

// Class inheriting from ns3::Application
class RangeConsumerApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("RangeConsumerApp")
      .SetParent<Application>()
      .AddConstructor<RangeConsumerApp>()
      .AddAttribute("RangePrefix", "Range prefix name", StringValue("/ndn"),
                    MakeStringAccessor(&RangeConsumerApp::prefix_), MakeStringChecker())
      .AddAttribute("RangeFirst", "First element to append to range prefix",
                    IntegerValue(0),
                    MakeIntegerAccessor(&RangeConsumerApp::first_), MakeIntegerChecker<uint32_t>())
      .AddAttribute("RangeLast", "Last element to append to range prefix",
                    IntegerValue(-1),
                    MakeIntegerAccessor(&RangeConsumerApp::last_), MakeIntegerChecker<uint32_t>())
      .AddAttribute("Frequency", "Frequency to send interest",
                    IntegerValue(0),
                    MakeIntegerAccessor(&RangeConsumerApp::frequency_), MakeIntegerChecker<uint32_t>());

    return tid;
  }

protected:
  virtual void StartApplication() {
    m_instance.reset(new ::ndn::ndvr::RangeConsumer(prefix_, first_, last_, frequency_));
    m_instance->Start();
  }

  virtual void StopApplication() {
    m_instance->Stop();
    m_instance.reset();
  }

private:
  std::unique_ptr<::ndn::ndvr::RangeConsumer> m_instance;
  std::string prefix_;
  uint32_t first_;
  uint32_t last_;
  uint32_t frequency_;
};

} // namespace ns3

#endif // RANGECONSUMER_APP_HPP
