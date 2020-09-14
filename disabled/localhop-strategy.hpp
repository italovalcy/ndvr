#ifndef LOCALHOP_STRATEGY_HPP
#define LOCALHOP_STRATEGY_HPP

#include "face/face.hpp"
#include "fw/strategy.hpp"
#include "fw/algorithm.hpp"
#include "fw/retx-suppression-exponential.hpp"

namespace nfd {
namespace fw {

/** @brief a forwarding strategy similar to scope=LOCALHOP and strategy=Multicast, but
 * enforcing the scope violation validation
 */
class LocalhopStrategy : public Strategy
{
public:
  explicit
  LocalhopStrategy(Forwarder& forwarder, const Name& name = getStrategyName());

  static const Name&
  getStrategyName();

  void
  afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;
private:
  RetxSuppressionExponential m_retxSuppression;
  static const time::milliseconds RETX_SUPPRESSION_INITIAL;
  static const time::milliseconds RETX_SUPPRESSION_MAX;
};

} // namespace fw
} // namespace nfd

#endif // LOCALHOP_STRATEGY_HPP
