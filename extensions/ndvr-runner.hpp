/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NDVR_RUNNER_HPP
#define NDVR_RUNNER_HPP

#include "ndvr.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>

// boost needs to be included after ndn-cxx, otherwise there will be conflict with _1, _2, ...
#include <boost/asio.hpp>

namespace ndn {
namespace ndvr {

class NdvrRunner
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

  NdvrRunner(std::string& networkName, std::string& routerName, int helloInterval, std::string& validationConfig, std::vector<std::string>& namePrefixes, std::vector<std::string>& faces, std::vector<std::string>& monitorFaces);

  void
  run();

  static void
  printUsage(const std::string& programName);

private:
  std::shared_ptr<Ndvr> m_ndvr;
  ndn::security::SigningInfo m_signingInfo;
};

} // namespace ndvr
} // namespace ndn

#endif // NDVR_RUNNER_HPP
