#ifndef _ROUTINGTABLE_H_
#define _ROUTINGTABLE_H_

#include <map>
#include <ndn-cxx/mgmt/nfd/controller.hpp>


namespace ndn {
namespace ndvr {

class RoutingEntry {
public:
  RoutingEntry()
  {
  }

  RoutingEntry(std::string name, uint64_t seqNum, uint64_t bestFaceId, uint32_t bestCost, uint32_t secBestCost)
    : m_name(name)
    , m_seqNum(seqNum)
    , m_bestFaceId(bestFaceId)
    , m_bestCost(bestCost)
    , m_secBestCost(secBestCost)
  {
  }

  RoutingEntry(std::string name, std::string originator, uint64_t seqNum, uint32_t bestCost, std::string learnedFrom, uint32_t secBestCost)
    : m_name(name)
    , m_originator(originator)
    , m_seqNum(seqNum)
    , m_bestFaceId(0)
    , m_bestCost(bestCost)
    , m_learnedFrom(learnedFrom)
    , m_secBestCost(secBestCost)
  {
  }

  RoutingEntry(std::string name, uint64_t seqNum)
    : RoutingEntry(name, seqNum, 0, std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max())
  {
  }

  ~RoutingEntry()
  {
  }

  void SetName(std::string name) {
    m_name = name;
  }

  std::string GetName() const {
    return m_name;
  }

  void SetOriginator(std::string originator) {
    m_originator = originator;
  }

  std::string GetOriginator() const {
    return m_originator;
  }

  void SetSeqNum(uint64_t seqNum) {
    m_seqNum = seqNum;
  }

  void IncSeqNum(uint64_t i) {
    m_seqNum += i;
  }

  uint64_t GetSeqNum() {
    return m_seqNum;
  }

  void UpsertNextHop(uint64_t faceId, uint32_t cost, std::string neighName) {
    std::cerr << "====> begin UpsertNextHop faceid=" << faceId << std::endl;
    m_nextHops[faceId] = std::make_tuple(cost, neighName);
    UpdateBestCost();
    //if (m_bestFaceId == 0 || cost < m_bestCost) {
    //  m_bestFaceId = faceId;
    //  m_bestCost = cost;
    //}
  }

  void SetNextHopCost(uint64_t faceId, uint32_t cost) {
    auto it = m_nextHops.find(faceId);
    if (it == m_nextHops.end())
      return;
    std::get<0>(it->second) = cost;
    // this will send the infinity cost to neighbors even if we have other routes
    //if (faceId == m_bestFaceId)
    //  m_bestCost = cost;
    UpdateBestCost();
  }

  uint32_t GetCost(uint64_t faceId) {
    auto it = m_nextHops.find(faceId);
    if (it != m_nextHops.end())
       return std::get<0>(it->second);
    return std::numeric_limits<uint32_t>::max();
  }

  std::string GetNextHopName(uint64_t faceId) {
    auto it = m_nextHops.find(faceId);
    if (it != m_nextHops.end())
       return std::get<1>(it->second);
    return "";
  }

  uint32_t GetBestCost() {
    return m_bestCost;
  }

  uint32_t GetSecondBestCost() {
    return m_secBestCost;
  }

  uint64_t GetBestFaceId() {
    return m_bestFaceId;
  }

  void DeleteNextHop(uint64_t faceId) {
    std::cerr << "====> begin DeleteNextHop" << std::endl;
    m_nextHops.erase(faceId);
    UpdateBestCost();
  }

  void UpdateBestCost() {
    std::cerr << "====> begin UpdateBestCost " << m_name << std::endl;
    m_bestFaceId = 0;
    m_bestCost = std::numeric_limits<uint32_t>::max();
    m_secBestCost = std::numeric_limits<uint32_t>::max();
    for (auto it = m_nextHops.begin(); it != m_nextHops.end(); ++it) {
      std::cerr << "====> step UpdateBestCost faceid=" << it->first << std::endl;
      if (std::get<0>(it->second) < m_bestCost) {
        m_secBestCost = m_bestCost;
        m_bestCost = std::get<0>(it->second);
        m_bestFaceId = it->first;
        std::cerr << "====> new faceId=" << m_bestFaceId << " m_bestCost=" << m_bestCost << " m_secBestCost=" << m_secBestCost << std::endl; 
      } else if (std::get<0>(it->second) < m_secBestCost) {
        m_secBestCost = std::get<0>(it->second);
        std::cerr << "====> new  m_secBestCost=" << m_secBestCost << std::endl; 
      }
    }
    if (m_bestFaceId!=0) {
      SetLearnedFrom(GetNextHopName(m_bestFaceId));
      std::cerr << "====> SetLearnedFrom=" << GetNextHopName(m_bestFaceId) << std::endl; 
    }
    std::cerr << "====> end UpdateBestCost" << std::endl;
  }

  size_t GetNextHopsSize() {
    //we cannot just return the map size, because some next hops are actualy
    //invalid, i.e. infinity cost
    //return m_nextHops.size();
    size_t validHopsSize = 0;
    for (auto it = m_nextHops.begin(); it != m_nextHops.end(); ++it)
      if (std::get<0>(it->second) != std::numeric_limits<uint32_t>::max())
        validHopsSize += 1;
    return validHopsSize;
  }

  bool isNextHop(uint64_t faceId) {
    return m_nextHops.find(faceId) != m_nextHops.end();
  }

  std::string getNextHopsStr() {
    std::string result;
    for (auto it = m_nextHops.begin(); it != m_nextHops.end(); ++it)
      result.append("faceid=" + std::to_string(it->first) + " (cost=" + std::to_string(std::get<0>(it->second)) +"), ");
    return result.substr(0, result.size()-2);
  }

  bool isDirectRoute() {
    return isNextHop(0);
  }

  uint64_t GetFaceId() {
    return m_bestFaceId;
  }

  void SetLearnedFrom(std::string learnedFrom) {
    m_learnedFrom = learnedFrom;
  }

  std::string GetLearnedFrom() const {
    return m_learnedFrom;
  }

private:
  std::string m_name;
  std::string m_originator;
  uint64_t m_seqNum;
  uint64_t m_bestFaceId;
  uint32_t m_bestCost;
  /* nextHops map is indexed by faceId and has as values a tuple
   * of <cost, neighName>. The cost is used to rank reachability to
   * that neighbor. The neighName is used together with m_learnedFrom
   * when processing the DvInfo and avoid local loops (i.e., learn a
   * route from a neighbor who learned only from ourselves) 
   */
  std::map<uint64_t, std::tuple<uint32_t, std::string>> m_nextHops;
  /* variables used when processing the dvinfo */
  std::string m_learnedFrom;
  uint32_t m_secBestCost;
};

/**
 * @brief represents the Distance Vector information
 *
 *   The Distance Vector Information contains a collection of Routes (ie,
 *   Name Prefixes), Cost and Sequence Number, each represents a piece of 
 *   dynamic routing information learned from neighbors.
 */
typedef std::map<std::string, RoutingEntry> RoutingTable;

//class RoutingTable : public std::map<std::string, RoutingEntry> {
class RoutingManager {
public:
  /* The fact that the elements in a map are always sorted by its key 
   * is important for us for the digest calculation */
  RoutingTable m_rt;

  RoutingManager()
    : m_version(1)
    , m_digest("0")
  {
  }

  RoutingManager(ndn::Face& face, ndn::KeyChain& keyChain)
    : m_version(1)
    , m_digest("0")
    , m_face(face.getIoService())
  {
    m_controller = new ndn::nfd::Controller(face, keyChain);
    //m_netmon = make_shared<ndn::net::NetworkMonitor>(face.getIoService());
  }

  ~RoutingManager() {}

  //void UpdateRoute(RoutingEntry& e, uint64_t new_nh);
  //void AddRoute(RoutingEntry& e);
  void DeleteRoute(std::string name, uint64_t nh);
  bool isDirectRoute(std::string n);
  RoutingEntry* LookupRoute(std::string n);
  void UpsertNextHop(RoutingEntry& e, uint64_t faceId, uint32_t cost, std::string neighName);
  void DeleteNextHop(RoutingEntry& e, uint64_t nh);
  void insert(RoutingEntry& e);
  void UpdateDigest();
  void unregisterPrefix(const std::string name, const uint64_t faceId);
  void registerPrefix(std::string name, uint64_t faceId, uint32_t cost, uint8_t retry = 0);
  uint64_t createFace(std::string faceUri);
  void enableLocalFields();
  void setMulticastStrategy(std::string name);

  uint32_t GetVersion() {
    return m_version;
  }
  void IncVersion() {
    m_version++;
    UpdateDigest();
  }

  std::string GetDigest() const {
    return m_digest;
  }
  void SetDigest(std::string s) {
    m_digest = s;
  }

  // just forward some methods
  decltype(m_rt.begin()) begin() { return m_rt.begin(); }
  decltype(m_rt.end()) end() { return m_rt.end(); }
  decltype(m_rt.size()) size() { return m_rt.size(); }

private:
  /*! \brief Log registration success.
   */
  void onRegistrationSuccess(const ndn::nfd::ControlParameters& param);

  /*! \brief Retry a prefix (next-hop) registration up to three (3) times.
   */
  void onRegistrationFailure(const ndn::nfd::ControlResponse& resp, const ndn::nfd::ControlParameters& param, uint8_t retry);

private:
  uint32_t m_version;
  std::string m_digest;
  ndn::Face m_face;
  ndn::nfd::Controller *m_controller;
  //shared_ptr<ndn::net::NetworkMonitor> m_netmon;
};

} // namespace ndvr
} // namespace ndn

#endif // _ROUTINGTABLE_H_
