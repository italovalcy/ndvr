/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ndvr-runner.hpp"
#include <ndn-cxx/security/key-chain.hpp>

namespace ndn {
namespace ndvr {

NdvrRunner::NdvrRunner(std::string& networkName, std::string& routerName, int helloInterval, std::string& validationConfig, std::vector<std::string>& namePrefixes, std::vector<std::string>& faces, std::vector<std::string>& monitorFaces)
{
  m_signingInfo = ndn::security::SigningInfo(ndn::security::SigningInfo::SIGNER_TYPE_ID,
                                             networkName + routerName);
  m_ndvr = std::make_shared<Ndvr>(m_signingInfo, networkName, routerName, namePrefixes, faces, monitorFaces, validationConfig);
  if (helloInterval != 0)
    m_ndvr->SetHelloInterval(helloInterval);
}

void
NdvrRunner::run()
{
  m_ndvr->Start();
  try {
    m_ndvr->run();
  }
  catch (std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;

    m_ndvr->cleanup();
  }
}

void
NdvrRunner::printUsage(const std::string& programName)
{
  std::cout << "Usage: " << programName << " [OPTIONS...]" << std::endl;
  std::cout << "   NDN Distance Vector Routing" << std::endl;
  std::cout << "       -n <NAME>   Specify the network name (e.g., /ndn)" << std::endl;
  std::cout << "       -r <NAME>   Specify the router name (e.g., /%C1.Router/Router0)." << std::endl;
  std::cout << "       -v <FILE>   Specify validation config file" << std::endl;
  std::cout << "       -p <NAME>   Specify the name prefix to be announced (can be used multiple times)" << std::endl;
  std::cout << "       -f <FACE>   Specify the face ID in which NDVR will work (can be used multiple times)" << std::endl;
  std::cout << "       -m <FACE>   Specify the face URI (remoteUri) in which NDVR will monitor for nfd/faces/events (can be used multiple times)" << std::endl;
  std::cout << "       -h          Display usage " << std::endl;
  std::cout << "" << std::endl;
  std::cout << "SECURITY IDENTITY" << std::endl;
  std::cout << "   In order to NDVR sign and secure NDN Data packets, it is" << std::endl;
  std::cout << "   mandatory to create the router certificate signed by the" << std::endl;
  std::cout << "   network certificate in advance! NDVR will try to load" << std::endl;
  std::cout << "   the certificate from NDN default key chain. To create" << std::endl;
  std::cout << "   certificates you can run:" << std::endl;
  std::cout << "      ndnsec-key-gen -n /ndn" << std::endl;
  std::cout << "      ndnsec-key-gen -n /ndn/%C1.Router/Router0 > router0-unsigned.cert" << std::endl;
  std::cout << "      ndnsec-cert-gen -s /ndn -r router0-unsigned.cert > router0.cert" << std::endl;
  std::cout << "      ndnsec-cert-install router0.cert" << std::endl;
  std::cout << "" << std::endl;
  std::cout << "FACES" << std::endl;
  std::cout << "   You can specify many faces in which NDVR will discover neighbors" << std::endl;
  std::cout << "   by providing the faceId (face already created) or face Uri (to be" << std::endl;
  std::cout << "   created by NDVR). Examplei (more in nfd wiki FaceMgmt):" << std::endl;
  std::cout << "      -f ether://[08:00:27:01:01:01] -f dev://wlan0 ..." << std::endl;
}

} // namespace ndvr
} // namespace ndn
