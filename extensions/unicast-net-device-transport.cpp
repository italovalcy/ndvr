#include "unicast-net-device-transport.hpp"

#include "model/ndn-block-header.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.UnicastNetDeviceTransport");

namespace ns3 {
namespace ndn {

void
UnicastNetDeviceTransport::doSend(const Block& packet, const nfd::EndpointId& endpoint)
{
  NS_LOG_DEBUG("face=" << this->getFace()->getId() << " LocalURI=" << this->getLocalUri() << " RemoteUri=" << this->getRemoteUri());

  // convert NFD packet to NS3 packet
  BlockHeader header(packet);

  Ptr<ns3::Packet> ns3Packet = Create<ns3::Packet>();
  ns3Packet->AddHeader(header);

  // send the NS3 packet
  NetDeviceTransport::GetNetDevice()->Send(ns3Packet, Mac48Address(m_neighMac.c_str()),
                    L3Protocol::ETHERNET_FRAME_TYPE);
}


// callback
void
UnicastNetDeviceTransport::receiveFromNetDevice(Ptr<NetDevice> device,
                                         Ptr<const ns3::Packet> p,
                                         uint16_t protocol,
                                         const Address& from, const Address& to,
                                         NetDevice::PacketType packetType)
{
  NS_LOG_DEBUG("face=" << this->getFace()->getId() << " from="<< from << " to=" << to);

  if (to != device->GetAddress()) {
    //return {nullptr, "Received frame addressed to another host or multicast group: " + dhost.toString()};
    NS_LOG_DEBUG("Received frame addressed to another host or multicast group: " << to);
    return;
  }

  // Convert NS3 packet to NFD packet
  Ptr<ns3::Packet> packet = p->Copy();

  BlockHeader header;
  packet->RemoveHeader(header);

  this->receive(std::move(header.getBlock()));
}


} // namespace ndn
} // namespace ns3
