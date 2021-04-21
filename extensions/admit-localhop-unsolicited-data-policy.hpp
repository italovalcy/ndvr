#ifndef ADMIT_LOCALHOP_UNSOLICITED_DATA_POLICY_HPP
#define ADMIT_LOCALHOP_UNSOLICITED_DATA_POLICY_HPP

//#include "face/face.hpp"
#include <ndn-cxx/face.hpp>
#include "fw/unsolicited-data-policy.hpp"

namespace nfd {
namespace fw {

/** \brief admits unsolicited Data from Localhop namespace scope
 */
class AdmitLocalhopUnsolicitedDataPolicy : public UnsolicitedDataPolicy
{
public:
  UnsolicitedDataDecision
  decide(const Face& inFace, const Data& data) const final;

public:
  static const std::string POLICY_NAME;
};
} // namespace fw
} // namespace nfd

#endif // ADMIT_LOCALHOP_UNSOLICITED_DATA_POLICY_HPP
