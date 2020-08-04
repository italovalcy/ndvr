#include "nrdv-strategy.hpp"
#include "common/logger.hpp"

namespace nfd {
namespace fw {

NFD_REGISTER_STRATEGY(NrdvMulticastStrategy);

NFD_LOG_INIT(NrdvMulticastStrategy);

NrdvMulticastStrategy::NrdvMulticastStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
{
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
NrdvMulticastStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/nrdv-multicast/%FD%01");
  return strategyName;
}

void
NrdvMulticastStrategy::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  NFD_LOG_DEBUG("I: " << interest << " inFaceId=" << ingress.face.getId());

  for (const auto& nexthop : nexthops) {
    Face& outFace = nexthop.getFace();

    if ((outFace.getId() == ingress.face.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC) ||
        wouldViolateScope(ingress.face, interest, outFace)) {
      continue;
    }

    this->sendInterest(pitEntry, FaceEndpoint(outFace, 0), interest);
  }
}
} // namespace fw
} // namespace nfd
