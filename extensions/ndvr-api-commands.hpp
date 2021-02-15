#ifndef NDVR_API_COMMANDS_HPP
#define NDVR_API_COMMANDS_HPP

#include <ndn-cxx/mgmt/nfd/control-command.hpp>

namespace ndn {
namespace ndvr {

class WithdrawPrefixCommand : public ndn::nfd::ControlCommand
{
public:
  WithdrawPrefixCommand();
};

class AdvertisePrefixCommand : public ndn::nfd::ControlCommand
{
public:
  AdvertisePrefixCommand();
};

class ListPrefixesCommand : public ndn::nfd::ControlCommand
{
public:
  ListPrefixesCommand();
};

class WatchUpdatesCommand : public ndn::nfd::ControlCommand
{
public:
  WatchUpdatesCommand();
};

class UnwatchUpdatesCommand : public ndn::nfd::ControlCommand
{
public:
  UnwatchUpdatesCommand();
};

class NeighborsCommand : public ndn::nfd::ControlCommand
{
public:
  NeighborsCommand();
};

} // namespace ndvr
} // namespace ndn

#endif // NDVR_API_COMMANDS_HPP
