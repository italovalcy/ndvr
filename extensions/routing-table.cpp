#include <iostream>
#include <sstream> 
#include <string>
#include <future>         // std::promise, std::future
#include <boost/uuid/sha1.hpp>

#include "routing-table.hpp"
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/net/face-uri.hpp>

//#include <ns3/ndnSIM/helper/ndn-fib-helper.hpp>
//#include <ns3/simulator.h>
//#include <ns3/ptr.h>
//#include <ns3/node.h>
//#include <ns3/node-list.h>

namespace ndn {
namespace ndvr {

//uint64_t RoutingManager::createFace(std::string ifName, std::string linkTypeStr) {
//  auto netif = m_netmon->getNetworkInterface(ifName);
//  if (netif == nullptr) {
//    std::cerr << "Fail to canonize remote face: " << error << std::endl;
//    return 0;
//  }
//  ndn::nfd::LinkType linkType;
//  if (linkTypeStr == "adhoc")
//    linkType = ndn::nfd::LINK_TYPE_AD_HOC;
//  else if (linkTypeStr == "ma")
//    linkType = ndn::nfd::LINK_TYPE_MULTI_ACCESS;
//  else
//    linkType = ndn::nfd::LINK_TYPE_POINT_TO_POINT;
//
//  auto addr = ethernet::Address::fromString("ff:ff:ff:ff:ff:ff");
//
//  GenericLinkService::Options opts;
//  opts.allowFragmentation = true;
//  opts.allowReassembly = true;
//
//  auto linkService = make_unique<GenericLinkService>(opts);
//  auto transport = make_unique<MulticastEthernetTransport>(netif, address, m_mcastConfig.linkType);
//  auto face = make_shared<Face>(std::move(linkService), std::move(transport));
//}

void RoutingManager::setMulticastStrategy(std::string name) {
  ::ndn::nfd::ControlParameters parameters;
  parameters.setName(name)
            .setStrategy("/localhost/nfd/strategy/multicast");

  ::ndn::nfd::CommandOptions options;
  options.setTimeout(time::duration_cast<time::milliseconds>(time::seconds(1)));

  m_controller->start<nfd::StrategyChoiceSetCommand>(
    parameters,
    [this, name] (const ::ndn::nfd::ControlParameters&) {
      std::cerr << "Set Multicast Strategy success for name=" << name << std::endl;
    },
    [this, name] (const ::ndn::nfd::ControlResponse& resp) {
      std::cerr << "Fail to set multicast strategy (name=" << name << "): code=" << resp.getCode() << " error=" << resp.getText() << std::endl;
    },
    options);
}

void RoutingManager::enableLocalFields() {
  ndn::nfd::ControlParameters faceParameters;
  faceParameters.setFlagBit(ndn::nfd::BIT_LOCAL_FIELDS_ENABLED, true);

  ::ndn::nfd::CommandOptions options;
  options.setTimeout(time::duration_cast<time::milliseconds>(time::seconds(1)));
  
  m_controller->start<::ndn::nfd::FaceUpdateCommand>(
      faceParameters,
      [&] (const ::ndn::nfd::ControlParameters& resp) {
        std::cerr << "Local fields enabled for faceId=" << resp.getFaceId() << std::endl;
      },
      [&] (const ::ndn::nfd::ControlResponse& resp) {
        std::cerr << "Fail to enable Local Fields: status=" << resp.getCode() << " error=" << resp.getText() << std::endl;
      },
      options);
}

uint64_t RoutingManager::createFace(std::string faceUri) {
  uint64_t faceId;
  //bool hasError = false;
  ndn::FaceUri localUri(faceUri);
  ndn::FaceUri remoteUri("ether://[ff:ff:ff:ff:ff:ff]");
  ndn::FaceUri canonicalRemote, canonicalLocal;

  std::string error;
  remoteUri.canonize(
    [&canonicalRemote] (const FaceUri& canonicalUri) { canonicalRemote = canonicalUri; },
    [&error] (const std::string& errorReason) { error = errorReason; },
    m_face.getIoService(), time::seconds(1));

  if (!error.empty()) {
    std::cerr << "Fail to canonize remote face: " << error << std::endl;
    return 0;
  }

  error.clear();
  localUri.canonize(
    [&canonicalLocal] (const FaceUri& canonicalUri) { canonicalLocal = canonicalUri; },
    [&error] (const std::string& errorReason) { error = errorReason; },
    m_face.getIoService(), time::seconds(1));

  if (!error.empty()) {
    std::cerr << "Fail to canonize local face: " << error << std::endl;
    return 0;
  }

  //remoteUri.canonize(
  //  [&] (const FaceUri& canonicalUri) {
  //    canonicalRemote = canonicalUri;
  //    localUri.canonize(
  //      [&] (const FaceUri& canonicalUri) {
  //        canonicalLocal = canonicalUri;
  //      },
  //      [&] (const std::string& error) {
  //        std::cerr << "Fail to canonize face: " << error << std::endl;
  //        hasError = true;
  //      },
  //      m_face.getIoService(), time::seconds(1));
  //  },
  //  [&] (const std::string& error) {
  //    std::cerr << "Fail to canonize face: " << error << std::endl;
  //    hasError = true;
  //  },
  //  m_face.getIoService(), time::seconds(1));

  //if (hasError) {
  //  return 0;
  //}

  ndn::nfd::ControlParameters faceParameters;
  faceParameters.setUri(canonicalRemote.toString());
  faceParameters.setLocalUri(canonicalLocal.toString());
  faceParameters.setFacePersistency(::ndn::nfd::FacePersistency::FACE_PERSISTENCY_PERSISTENT);
  faceId = 0;

  ::ndn::nfd::CommandOptions options;
  options.setTimeout(time::duration_cast<time::milliseconds>(time::seconds(1)));
  
  std::cerr << "creating face uri=" << faceUri << std::endl;
  m_controller->start<::ndn::nfd::FaceCreateCommand>(
      faceParameters,
      [&faceId] (const ::ndn::nfd::ControlParameters& resp) {
        std::cerr << "New face created: faceId=" << resp.getFaceId() << " faceLocalUri=" << resp.getLocalUri() << std::endl;
        faceId = resp.getFaceId();
      },
      [&] (const ::ndn::nfd::ControlResponse& resp) {
        std::cerr << "Fail to create face: status=" << resp.getCode() << " error=" << resp.getText() << std::endl;
      },
      options);
  // TODO: it is async, how to wait until the face is created ?
  // TODO: Fail to create face: status=406 error=Cannot create multicast Ethernet faces
  //m_face.processEvents();
  std::cerr << "creating face faceId=" << faceId << std::endl;
  return faceId;
}

void RoutingManager::registerPrefix(std::string name, uint64_t faceId, uint32_t cost, uint8_t retry) {
  //using namespace ns3;
  //using namespace ns3::ndn;

  Name namePrefix = Name(name);
  //Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());
  //FibHelper::AddRoute(thisNode, namePrefix, faceId, cost);

  ::ndn::nfd::ControlParameters controlParameters;
  controlParameters
    .setName(namePrefix)
    .setFaceId(faceId)
    .setCost(cost);
  ::ndn::nfd::CommandOptions options;
  options.setTimeout(time::duration_cast<time::milliseconds>(time::seconds(1)));
  m_controller->start<::ndn::nfd::RibRegisterCommand>(controlParameters,
      [this, name, faceId] (const ::ndn::nfd::ControlParameters&) {
        std::cerr << "register rib success name=" << name << " faceId=" << faceId << std::endl;
      },
      [this, name, faceId, cost, retry] (const ::ndn::nfd::ControlResponse& resp) {
        std::cerr << "Fail to create FIB entry (name=" << name << " faceId=" << faceId << " retry=" << retry << "): code=" << resp.getCode() << " error=" << resp.getText() << std::endl;
        if (retry < 3) {
          registerPrefix(name, faceId, cost, retry+1);
        }
      },
      options);
}

void RoutingManager::unregisterPrefix(std::string name, uint64_t faceId) {
  //using namespace ns3;
  //using namespace ns3::ndn;

  Name namePrefix = Name(name);
  //Ptr<Node> thisNode = NodeList::GetNode(Simulator::GetContext());
  //FibHelper::RemoveRoute(thisNode, namePrefix, faceId);

  ::ndn::nfd::ControlParameters controlParameters;
  controlParameters
    .setName(namePrefix)
    .setFaceId(faceId);
  ::ndn::nfd::CommandOptions options;
  options.setTimeout(time::duration_cast<time::milliseconds>(time::seconds(1)));
  m_controller->start<::ndn::nfd::RibUnregisterCommand>(controlParameters,
      [this, name, faceId] (const ::ndn::nfd::ControlParameters&) {
        std::cerr << "unregister rib success name=" << name << " faceId=" << faceId << std::endl;
      },
      [this, name, faceId] (const ::ndn::nfd::ControlResponse& resp) {
        std::cerr << "unregister rib fail (name=" << name << " faceId=" << faceId << "): code=" << resp.getCode() << " error=" << resp.getText() << std::endl;
      },
      options);
}

bool RoutingManager::isDirectRoute(std::string n) {
  auto it = m_rt.find(n);
  if (it == m_rt.end())
    return false;
  return it->second.isDirectRoute();
}

RoutingEntry* RoutingManager::LookupRoute(std::string n) {
  auto it = m_rt.find(n);
  if (it == m_rt.end())
    return nullptr;
  return &it->second;
}

void RoutingManager::UpdateRoute(RoutingEntry& e, uint64_t new_nh) {
  if (e.GetFaceId() != new_nh) {
    unregisterPrefix(e.GetName(), e.GetFaceId());
  }
  e.SetFaceId(new_nh);
  AddRoute(e);
}

void RoutingManager::AddRoute(RoutingEntry& e) {
  registerPrefix(e.GetName(), e.GetFaceId(), e.GetCost());
  m_rt[e.GetName()] = e;
  UpdateDigest();
}

void RoutingManager::DeleteRoute(std::string name, uint64_t nh) {
  // TODO: we may have other faces to this name prefix (multipath).
  // In that case, we should only remove the nexthop
  unregisterPrefix(name, nh);
  m_rt.erase(name);
  UpdateDigest();
}

void RoutingManager::insert(RoutingEntry& e) {
  m_rt[e.GetName()] = e;
  UpdateDigest();
}

void RoutingManager::UpdateDigest() {
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
