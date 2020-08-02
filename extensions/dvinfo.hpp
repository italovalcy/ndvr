#ifndef _DVINFO_H_
#define _DVINFO_H_

#include <map>


namespace ndn {
namespace nrdv {

class DvInfoEntry {
public:
  DvInfoEntry()
  {
  }

  DvInfoEntry(std::string name, uint64_t seqNum, uint32_t cost)
    : m_name(name)
    , m_seqNum(seqNum)
    , m_cost(cost)
  {
  }

  ~DvInfoEntry()
  {
  }

  void SetName(std::string name) {
    m_name = name;
  }

  std::string GetName() {
    return m_name;
  }

  void SetSeqNum(uint64_t seqNum) {
    m_seqNum = seqNum;
  }

  uint64_t GetSeqNum() {
    return m_seqNum;
  }

  void SetCost(uint32_t cost) {
    m_cost = cost;
  }

  uint32_t GetCost() {
    return m_cost;
  }

private:
  std::string m_name;
  uint64_t m_seqNum;
  uint32_t m_cost;
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
typedef std::map<std::string, DvInfoEntry> DvInfoMap;

} // namespace nrdv
} // namespace ndn

#endif // _DVINFO_H_
