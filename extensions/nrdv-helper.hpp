#ifndef _NRDV_HELPER_HPP_
#define _NRDV_HELPER_HPP_

#include "nrdv-message.pb.h"

namespace ndn {
namespace nrdv {

inline void EncodeDvInfo(const DvInfoMap& v, proto::DvInfo* dvinfo_proto) {
  for (auto item: v) {
    auto* entry = dvinfo_proto->add_entry();
    entry->set_prefix(item.first);
    entry->set_seq(item.second.GetSeqNum());
    entry->set_cost(item.second.GetCost());
  }
}

inline void EncodeDvInfo(const DvInfoMap& v, std::string& out) {
  proto::DvInfo dvinfo_proto;
  EncodeDvInfo(v, &dvinfo_proto);
  dvinfo_proto.AppendToString(&out);
}

inline DvInfoMap DecodeDvInfo(const proto::DvInfo& dvinfo_proto) {
  DvInfoMap dvinfo;
  for (int i = 0; i < dvinfo_proto.entry_size(); ++i) {
    const auto& entry = dvinfo_proto.entry(i);
    auto prefix = entry.prefix();
    auto seq = entry.seq();
    auto cost = entry.cost();
    dvinfo[prefix] = DvInfoEntry(prefix, seq, cost);
  }
  return dvinfo;
}

inline DvInfoMap DecodeDvInfo(const void* buf, size_t buf_size) {
  proto::DvInfo dvinfo_proto;
  if (!dvinfo_proto.ParseFromArray(buf, buf_size)) {
    DvInfoMap res;
    return res;
  }
  return DecodeDvInfo(dvinfo_proto);
}
}  // namespace nrdv
}  // namespace ndn

#endif  // _NRDV_HELPER_HPP_
