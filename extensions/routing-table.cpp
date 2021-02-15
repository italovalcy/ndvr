#include <sstream> 
#include <string>
#include <boost/uuid/sha1.hpp>

#include "routing-table.hpp"

#include <ns3/ndnSIM/helper/ndn-fib-helper.hpp>
#include <ns3/simulator.h>
#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/node-list.h>

namespace ndn {
namespace ndvr {

void RoutingTable::registerPrefix(std::string name, uint64_t faceId, uint32_t cost) {
  using namespace ns3;
  using namespace ns3::ndn;

  Name namePrefix = Name(name);
  Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());
  FibHelper::AddRoute(thisNode, namePrefix, faceId, cost);
}

void RoutingTable::unregisterPrefix(std::string name, uint64_t faceId) {
  using namespace ns3;
  using namespace ns3::ndn;

  Name namePrefix = Name(name);
  Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());
  FibHelper::RemoveRoute(thisNode, namePrefix, faceId);
}

bool RoutingTable::isDirectRoute(std::string n) {
  auto it = m_rt.find(n);
  if (it == m_rt.end())
    return false;
  return it->second.isDirectRoute();
}

bool RoutingTable::LookupRoute(std::string n) {
  return m_rt.count(n);
}

bool RoutingTable::LookupRoute(std::string n, RoutingEntry& e) {
  auto it = m_rt.find(n);
  if (it == m_rt.end())
    return false;
  e = it->second;
  return true;
}

void RoutingTable::UpdateRoute(RoutingEntry& e, uint64_t new_nh) {
  if (e.GetFaceId() != new_nh) {
    unregisterPrefix(e.GetName(), e.GetFaceId());
  }
  e.SetFaceId(new_nh);
  AddRoute(e);
}

void RoutingTable::AddRoute(RoutingEntry& e) {
  registerPrefix(e.GetName(), e.GetFaceId(), e.GetCost());
  m_rt[e.GetName()] = e;
  UpdateDigest();
}

void RoutingTable::DeleteRoute(RoutingEntry& e, uint64_t nh) {
  // TODO: we may have other faces to this name prefix (multipath).
  // In that case, we should only remove the nexthop
  unregisterPrefix(e.GetName(), nh);
  m_rt.erase(e.GetName());
  UpdateDigest();
}

void RoutingTable::insert(RoutingEntry& e) {
  m_rt[e.GetName()] = e;
  UpdateDigest();
}

void RoutingTable::UpdateDigest() {
  std::stringstream rt_str, out;
  boost::uuids::detail::sha1 sha1;
  unsigned int hash[5];
  for (auto it = m_rt.begin(); it != m_rt.end(); ++it)
    rt_str << it->first << it->second.GetSeqNum();
  sha1.process_bytes(rt_str.str().c_str(), rt_str.str().size());
  sha1.get_digest(hash);
  for(std::size_t i=0; i<sizeof(hash)/sizeof(hash[0]); ++i) {
      out << std::hex << hash[i];
  }
  m_digest = out.str();
}

} // namespace ndvr
} // namespace ndn
