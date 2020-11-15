#ifndef UNICAST_NET_DEVICE_TRANSPORT_HPP
#define UNICAST_NET_DEVICE_TRANSPORT_HPP

#include "model/ndn-net-device-transport.hpp"

namespace ns3 {
namespace ndn {

/**
 * \ingroup ndn-face
 * \brief ndnSIM-specific transport - unicast
 */
class UnicastNetDeviceTransport : public NetDeviceTransport
{
public:
  UnicastNetDeviceTransport(Ptr<Node> node, const Ptr<NetDevice>& netDevice,
                     const std::string& localUri,
                     const std::string& neighMac,
                     ::ndn::nfd::FaceScope scope = ::ndn::nfd::FACE_SCOPE_NON_LOCAL,
                     ::ndn::nfd::FacePersistency persistency = ::ndn::nfd::FACE_PERSISTENCY_PERSISTENT,
                     ::ndn::nfd::LinkType linkType = ::ndn::nfd::LINK_TYPE_POINT_TO_POINT)
    : NetDeviceTransport(node, netDevice, localUri, "netdev://[" + neighMac + "]", scope, persistency, linkType)
    , m_neighMac(neighMac)
    //, m_netDevice(netDevice)
  {
  }

  ~UnicastNetDeviceTransport()
  {
  }

private:
  virtual void
  doSend(const Block& packet, const nfd::EndpointId& endpoint) override;

  //Ptr<NetDevice> m_netDevice; ///< \brief Smart pointer to NetDevice
  std::string m_neighMac;
};

} // namespace ndn
} // namespace ns3

#endif // NDN_NULL_TRANSPORT_HPP
