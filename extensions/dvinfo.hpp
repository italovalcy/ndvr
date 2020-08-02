#ifndef _NAMEPREFIXES_H_
#define _NAMEPREFIXES_H_

#include <map>


namespace ndn {
namespace nrdv {

class NamePrefix {
public:
  NamePrefix()
  {
  }

  NamePrefix(std::string name, uint64_t seqNum, uint32_t cost)
    : m_name(name)
    , m_seqNum(seqNum)
    , m_cost(cost)
  {
  }

  ~NamePrefix()
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

typedef std::map<std::string, NamePrefix> NamePrefixMap;

} // namespace nrdv
} // namespace ndn

#endif // _NAMEPREFIXES_H_
