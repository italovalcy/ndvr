// consumer-oninterest.hpp

#ifndef CONSUMER_ONINTEREST_H
#define CONSUMER_ONINTEREST_H

#include "ns3/ndnSIM/apps/ndn-app.hpp"

namespace ns3 {

class ConsumerOnInterest : public ndn::App {
public:
  ConsumerOnInterest();
  static TypeId GetTypeId();
  void OnInterest(std::shared_ptr<const ndn::Interest> interest);

protected:
  virtual void StartApplication();
  virtual void StopApplication();
  void NdvrAddRoute(const std::string& namePrefix);

private:
  ndn::Name interestName_;
};

} // namespace ns3

#endif // CONSUMER_ONINTEREST_H
