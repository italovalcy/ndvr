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
  return it->second.GetFaceId() == 0;
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

void RoutingTable::UpdateRoute(RoutingEntry& e) {
}

void RoutingTable::AddRoute(RoutingEntry& e) {
  registerPrefix(e.GetName(), e.GetFaceId(), e.GetCost());
  m_rt[e.GetName()] = e;
}

void RoutingTable::DeleteRoute(RoutingEntry& e, uint64_t nh) {
  // TODO: we may have other faces to this name prefix (multipath).
  // In that case, we should only remove the nexthop
  unregisterPrefix(e.GetName(), nh);
  m_rt.erase(e.GetName());
}

void RoutingTable::insert(RoutingEntry& e) {
  m_rt[e.GetName()] = e;
}

} // namespace ndvr
} // namespace ndn
