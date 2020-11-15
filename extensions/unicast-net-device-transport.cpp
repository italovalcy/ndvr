#include "unicast-net-device-transport.hpp"

#include "model/ndn-block-header.hpp"
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.UnicastNetDeviceTransport");

namespace ns3 {
namespace ndn {

void
UnicastNetDeviceTransport::doSend(const Block& packet, const nfd::EndpointId& endpoint)
{
  NS_LOG_FUNCTION(this << "Sending packet from netDevice with URI"
                  << this->getLocalUri());

  // convert NFD packet to NS3 packet
  BlockHeader header(packet);

  Ptr<ns3::Packet> ns3Packet = Create<ns3::Packet>();
  ns3Packet->AddHeader(header);

  // send the NS3 packet
  NetDeviceTransport::GetNetDevice()->Send(ns3Packet, Mac48Address(m_neighMac.c_str()),
                    L3Protocol::ETHERNET_FRAME_TYPE);
}


} // namespace ndn
} // namespace ns3
