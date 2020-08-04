#ifndef NRDV_STRATEGY_HPP
#define NRDV_STRATEGY_HPP

#include "face/face.hpp"
#include "fw/strategy.hpp"
#include "fw/algorithm.hpp"

namespace nfd {
namespace fw {

/** @brief a forwarding strategy similar to MulticastStrategy, but capture the ingress face
 *
 */
class NrdvMulticastStrategy : public Strategy
{
public:
  explicit
  NrdvMulticastStrategy(Forwarder& forwarder, const Name& name = getStrategyName());

  static const Name&
  getStrategyName();

  void
  afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;
};

} // namespace fw
} // namespace nfd

#endif // NRDV_STRATEGY_HPP
