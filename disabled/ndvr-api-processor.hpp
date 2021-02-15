#ifndef NDVR_API_PROCESSOR_HPP
#define NDVR_API_PROCESSOR_HPP

#include "ndvr-api-commands.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/mgmt/nfd/control-command.hpp>
#include <ndn-cxx/mgmt/nfd/control-parameters.hpp>
#include <ndn-cxx/security/certificate-cache-ttl.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#include <boost/noncopyable.hpp>
#include <boost/property_tree/ptree.hpp>

namespace ndn {

namespace ndvr {


class NdvrApiProcessor : boost::noncopyable
{
public:
  class Error : public std::runtime_error
  {
  public:
    explicit
    Error(const std::string& what)
      : std::runtime_error(what)
    {
    }
  };

public:
  NdvrApiProcessor(ndn::Face& face,
                        NamePrefixList& namePrefixList,
                        Lsdb& ndvr,
                        const ndn::Name broadcastPrefix,
                        ndn::KeyChain& keyChain);

  void
  startListening();

private:
  void
  onInterest(const ndn::Interest& request);

  void
  sendResponse(const std::shared_ptr<const ndn::Interest>& request,
               uint32_t code,
               const std::string& text);

  /** \brief adds desired name prefix to the advertised name prefix list
   */
  void
  advertise(const std::shared_ptr<const ndn::Interest>& request,
            const ndn::nfd::ControlParameters& parameters);

  /** \brief removes desired name prefix from the advertised name prefix list
   */
  void
  withdraw(const std::shared_ptr<const ndn::Interest>& request,
           const ndn::nfd::ControlParameters& parameters);

  void
  onCommandValidated(const std::shared_ptr<const ndn::Interest>& request);

  void
  onCommandValidationFailed(const std::shared_ptr<const ndn::Interest>& request,
                            const std::string& failureInfo);

  static bool
  extractParameters(const ndn::name::Component& parameterComponent,
                    ndn::nfd::ControlParameters& extractedParameters);

  bool
  validateParameters(const ndn::nfd::ControlCommand& command,
                     const ndn::nfd::ControlParameters& parameters);

private:
  ndn::Face& m_face;
  NamePrefixList& m_namePrefixList;
  Ndvr& m_ndvr;
  ndn::KeyChain& m_keyChain;
  Validator m_validator;

  const ndn::Name COMMAND_PREFIX; // /localhost/ndvr/api

  static const ndn::Name::Component MODULE_COMPONENT;
  static const ndn::Name::Component LIST_VERB;
  static const ndn::Name::Component ADVERTISE_VERB;
  static const ndn::Name::Component WITHDRAW_VERB;
  static const ndn::Name::Component WATCH_VERB;
  static const ndn::Name::Component UNWATCH_VERB;
  static const ndn::Name::Component NEIGHBORS_VERB;
};

} // namespace ndvr
} // namespace ndn

#endif // NDVR_API_PROCESSOR_HPP
