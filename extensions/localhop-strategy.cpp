#include "localhop-strategy.hpp"
#include "common/logger.hpp"

namespace nfd {
namespace fw {

NFD_REGISTER_STRATEGY(LocalhopStrategy);

NFD_LOG_INIT(LocalhopStrategy);

const time::milliseconds LocalhopStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds LocalhopStrategy::RETX_SUPPRESSION_MAX(250);

LocalhopStrategy::LocalhopStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
LocalhopStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/localhop/%FD%01");
  return strategyName;
}

bool
my_wouldViolateScope(const Face& inFace, const Interest& interest, const Face& outFace)
{
  if (outFace.getScope() == ndn::nfd::FACE_SCOPE_LOCAL) {
    // forwarding to a local face is always allowed
    return false;
  }

  // localhop Interests can be forwarded to a non-local face only if it comes from a local face
  return inFace.getScope() != ndn::nfd::FACE_SCOPE_LOCAL;
}

void
LocalhopStrategy::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  NFD_LOG_DEBUG("I: " << interest << " inFaceId=" << ingress.face.getId());

  int nEligibleNextHops = 0;

  bool isSuppressed = false;

  for (const auto& nexthop : nexthops) {
    Face& outFace = nexthop.getFace();

    RetxSuppressionResult suppressResult = m_retxSuppression.decidePerUpstream(*pitEntry, outFace);

    if ((outFace.getId() == ingress.face.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC) ||
        my_wouldViolateScope(ingress.face, interest, outFace)) {
      // even when it violete the scope, we need to check the supress result in order to decide
      // whether remove the Pit entry or just the InRecord
      if (suppressResult == RetxSuppressionResult::SUPPRESS)
        isSuppressed = true;
      continue;
    }

    ++nEligibleNextHops;

    if (suppressResult == RetxSuppressionResult::SUPPRESS) {
      NFD_LOG_DEBUG(interest << " from=" << ingress << " to=" << outFace.getId() << " suppressed");
      isSuppressed = true;
      continue;
    }

    this->sendInterest(pitEntry, FaceEndpoint(outFace, 0), interest);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " pitEntry-to=" << outFace.getId());

    if (suppressResult == RetxSuppressionResult::FORWARD) {
      m_retxSuppression.incrementIntervalForOutRecord(*pitEntry->getOutRecord(outFace));
    }
  }
  if (nEligibleNextHops > 0)
    return;

  if (isSuppressed) {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " noNextHop: deleteInRecord");
    pitEntry->deleteInRecord(ingress.face);
  } else {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " noNextHop: rejectPendingInterest");
    this->rejectPendingInterest(pitEntry);
  }
}
} // namespace fw
} // namespace nfd
