#include "admit-localhop-unsolicited-data-policy.hpp"

#include "fw/scope-prefix.hpp"

namespace nfd {
namespace fw {

const std::string AdmitLocalhopUnsolicitedDataPolicy::POLICY_NAME("admit-localhop");
NFD_REGISTER_UNSOLICITED_DATA_POLICY(AdmitLocalhopUnsolicitedDataPolicy);

UnsolicitedDataDecision
AdmitLocalhopUnsolicitedDataPolicy::decide(const Face& inFace, const Data& data) const
{
  if (scope_prefix::LOCALHOP.isPrefixOf(data.getName())) {
    return UnsolicitedDataDecision::CACHE;
  }
  return UnsolicitedDataDecision::DROP;
}

} // namespace fw
} // namespace nfd
