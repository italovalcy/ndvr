#include "ndvr-api-processor.hpp"

#include "ndvr.hpp"
#include "ndvr-api-commands.hpp"

#include <ndn-cxx/mgmt/control-response.hpp>
#include <ns3/log.h>

namespace ndn {
namespace ndvr {

NS_LOG_COMPONENT_DEFINE("ndn.Ndvr.API");

const ndn::Name::Component NdvrApiProcessor::MODULE_COMPONENT = ndn::Name::Component("api");
const ndn::Name::Component NdvrApiProcessor::ADVERTISE_VERB = ndn::Name::Component("advertise");
const ndn::Name::Component NdvrApiProcessor::WITHDRAW_VERB  = ndn::Name::Component("withdraw");
const ndn::Name::Component NdvrApiProcessor::LIST_VERB  = ndn::Name::Component("list");
const ndn::Name::Component NdvrApiProcessor::WATCH_VERB  = ndn::Name::Component("watch");
const ndn::Name::Component NdvrApiProcessor::UNWATCH_VERB  = ndn::Name::Component("unwatch");
const ndn::Name::Component NdvrApiProcessor::NEIGHBORS_VERB  = ndn::Name::Component("neighbors");

NdvrApiProcessor::NdvrApiProcessor(ndn::Face& face,
                                             NamePrefixList& namePrefixList,
                                             Ndvr& ndvr,
                                             ndn::KeyChain& keyChain)
  : m_face(face)
  , m_namePrefixList(namePrefixList)
  , m_ndvr(ndvr)
  , m_keyChain(keyChain)
  , COMMAND_PREFIX(ndn::Name("/localhost/ndvr").append(MODULE_COMPONENT))
{
}

void
NdvrApiProcessor::startListening()
{
  NS_LOG_DEBUG("Setting Interest filter for: " << COMMAND_PREFIX);

  m_face.setInterestFilter(COMMAND_PREFIX, bind(&NdvrApiProcessor::onInterest, this, _2));
}

void
NdvrApiProcessor::onInterest(const ndn::Interest& request)
{
  NS_LOG_DEBUG("Received Interest: " << request);

  m_validator.validate(request,
                       bind(&NdvrApiProcessor::onCommandValidated, this, _1),
                       bind(&NdvrApiProcessor::onCommandValidationFailed, this, _1, _2));
}

void
NdvrApiProcessor::onCommandValidated(const std::shared_ptr<const ndn::Interest>& request)
{
  const ndn::Name& command = request->getName();
  const ndn::Name::Component& verb = command[COMMAND_PREFIX.size()];
  const ndn::Name::Component& parameterComponent = command[COMMAND_PREFIX.size() + 1];

  if (verb == ADVERTISE_VERB || verb == WITHDRAW_VERB || verb == WATCH_VERB || verb == UNWATCH_VERB) {
    ndn::nfd::ControlParameters parameters;

    if (!extractParameters(parameterComponent, parameters)) {
      sendResponse(request, 400, "Malformed command");
      return;
    }

    if (verb == ADVERTISE_VERB) {
      advertise(request, parameters);
    }
    else if (verb == WITHDRAW_VERB) {
      withdraw(request, parameters);
    }
    else if (verb == WATCH_VERB) {
      addListner(request, parameters);
    }
    else if (verb == UNWATCH_VERB) {
      removeListner(request, parameters);
    }
  }
  else if (verb == LIST_VERB) {
    listPrefixes(request);
  }
  else if (verb == NEIGHBORS_VERB) {
    listNeighbors(request);
  }
  else {
    sendResponse(request, 501, "Unsupported command");
  }
}

void
NdvrApiProcessor::onCommandValidationFailed(const std::shared_ptr<const ndn::Interest>& request,
                                                 const std::string& failureInfo)
{
  sendResponse(request, 403, failureInfo);
}

bool
NdvrApiProcessor::extractParameters(const ndn::Name::Component& parameterComponent,
                                         ndn::nfd::ControlParameters& extractedParameters)
{
  try {
    ndn::Block rawParameters = parameterComponent.blockFromValue();
    extractedParameters.wireDecode(rawParameters);
  }
  catch (const ndn::tlv::Error&) {
    return false;
  }

  return true;
}

void
NdvrApiProcessor::advertise(const std::shared_ptr<const ndn::Interest>& request,
                                 const ndn::nfd::ControlParameters& parameters)
{
  AdvertisePrefixCommand command;

  if (!validateParameters(command, parameters)) {
    sendResponse(request, 400, "Malformed command");
    return;
  }

  NS_LOG_INFO("Advertising name: " << parameters.getName());

  if (m_namePrefixList.insert(parameters.getName())) {
    // Only build a Name LSA if the added name is new
    m_lsdb.buildAndInstallOwnNameLsa();
    m_sync.publishRoutingUpdate();
  }
  sendResponse(request, 200, "Success");
}

void
NdvrApiProcessor::withdraw(const std::shared_ptr<const ndn::Interest>& request,
                                const ndn::nfd::ControlParameters& parameters)
{
  WithdrawPrefixCommand command;

  if (!validateParameters(command, parameters)) {
    sendResponse(request, 400, "Malformed command");
    return;
  }

  NS_LOG_INFO("Withdrawing name: " << parameters.getName());

  if (m_namePrefixList.remove(parameters.getName())) {
    // Only build a Name LSA if a name was actually removed
    m_lsdb.buildAndInstallOwnNameLsa();
    m_sync.publishRoutingUpdate();
  }
}

bool
NdvrApiProcessor::validateParameters(const ndn::nfd::ControlCommand& command,
                                          const ndn::nfd::ControlParameters& parameters)
{
  try {
    command.validateRequest(parameters);
  }
  catch (const ndn::nfd::ControlCommand::ArgumentError&) {
    return false;
  }

  return true;
}

void
NdvrApiProcessor::sendNack(const ndn::Interest& request)
{
  ndn::MetaInfo metaInfo;
  metaInfo.setType(ndn::tlv::ContentType_Nack);

  shared_ptr<ndn::Data> responseData = std::make_shared<ndn::Data>(request.getName());
  responseData->setMetaInfo(metaInfo);

  m_keyChain.sign(*responseData);
  m_face.put(*responseData);
}

void
NdvrApiProcessor::sendResponse(const std::shared_ptr<const ndn::Interest>& request,
                                    uint32_t code,
                                    const std::string& text)
{
  if (request == nullptr) {
    return;
  }

  ndn::nfd::ControlResponse response(code, text);
  const ndn::Block& encodedControl = response.wireEncode();

  std::shared_ptr<ndn::Data> responseData = std::make_shared<ndn::Data>(request->getName());
  responseData->setContent(encodedControl);

  m_keyChain.sign(*responseData);
  m_face.put(*responseData);
}

} // namespace update
} // namespace nlsr
