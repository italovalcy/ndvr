#include "ndvr-api-commands.hpp"

namespace ndn {
namespace ndvr {

WithdrawPrefixCommand::WithdrawPrefixCommand()
  : ControlCommand("ndvr", "withdraw")
{
  m_requestValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
  m_responseValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
}

AdvertisePrefixCommand::AdvertisePrefixCommand()
  : ControlCommand("ndvr", "advertise")
{
  m_requestValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
  m_responseValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
}

ListPrefixesCommand::ListPrefixesCommand()
  : ControlCommand("ndvr", "list")
{
  m_requestValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
  m_responseValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
}

WatchUpdatesCommand::WatchUpdatesCommand()
  : ControlCommand("ndvr", "watch")
{
  m_requestValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
  m_responseValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
}

UnwatchUpdatesCommand::UnwatchUpdatesCommand()
  : ControlCommand("ndvr", "unwatch")
{
  m_requestValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
  m_responseValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
}

NeighborsCommand::NeighborsCommand()
  : ControlCommand("ndvr", "neighbors")
{
  m_requestValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
  m_responseValidator.required(ndn::nfd::CONTROL_PARAMETER_NAME);
}

} // namespace ndvr
} // namespace ndn
