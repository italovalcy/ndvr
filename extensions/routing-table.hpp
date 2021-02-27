#ifndef _ROUTINGTABLE_H_
#define _ROUTINGTABLE_H_

#include <map>


namespace ndn {
namespace ndvr {

class RoutingEntry {
public:
  RoutingEntry()
  {
  }

  RoutingEntry(std::string name, uint64_t seqNum, uint32_t cost, uint64_t faceId)
    : m_name(name)
    , m_seqNum(seqNum)
    , m_cost(cost)
    , m_faceId(faceId)
  {
  }

  RoutingEntry(std::string name, uint64_t seqNum, uint32_t cost)
    : RoutingEntry(name, seqNum, cost, 0)
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

  void SetSeqNum(uint64_t seqNum) {
    m_seqNum = seqNum;
  }

  void IncSeqNum(uint64_t i) {
    m_seqNum += i;
  }

  uint64_t GetSeqNum() {
    return m_seqNum;
  }

  void SetCost(uint32_t cost) {
    m_cost = cost;
  }

  void SetCost(uint64_t faceId, uint32_t cost) {
    // TODO: set cost only for this faceId
    m_cost = cost;
  }

  uint32_t GetCost() {
    return m_cost;
  }

  void SetFaceId(uint64_t faceId) {
    m_faceId = faceId;
  }

  uint64_t GetFaceId() {
    return m_faceId;
  }

  bool isNextHop(uint64_t faceId) {
    // TODO: in case of multipath, we may need to compare with a list of faces
    return faceId == m_faceId;
  }

  bool isDirectRoute() {
    return m_faceId == 0;
  }

private:
  std::string m_name;
  uint64_t m_seqNum;
  uint32_t m_cost;
  uint64_t m_faceId;
};

/**
 * @brief represents the Distance Vector information
 *
 *   The Distance Vector Information contains a collection of Routes (ie,
 *   Name Prefixes), Cost and Sequence Number, each represents a piece of 
 *   dynamic routing information learned from neighbors.
 *
 *   TODO: Routes associated with the same namespace are collected 
 *   into a RIB entry.
 */
//class RoutingTable : public std::map<std::string, RoutingEntry> {
class RoutingTable {
public:
  /* The fact that the elements in a map are always sorted by its key 
   * is important for us for the digest calculation */
  std::map<std::string, RoutingEntry> m_rt;

  RoutingTable()
    : m_version(1)
    , m_digest("0")
  {
  }

  ~RoutingTable() {}

  void UpdateRoute(RoutingEntry& e, uint64_t new_nh);
  void AddRoute(RoutingEntry& e);
  void DeleteRoute(RoutingEntry& e, uint64_t nh);
  bool isDirectRoute(std::string n);
  bool LookupRoute(std::string n);
  bool LookupRoute(std::string n, RoutingEntry& e);
  void insert(RoutingEntry& e);
  void UpdateDigest();
  void unregisterPrefix(std::string name, uint64_t faceId);
  void registerPrefix(std::string name, uint64_t faceId, uint32_t cost);

  uint32_t GetVersion() {
    return m_version;
  }
  void IncVersion() {
    m_version++;
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
  uint32_t m_version;
  std::string m_digest;
};

} // namespace ndvr
} // namespace ndn

#endif // _ROUTINGTABLE_H_
