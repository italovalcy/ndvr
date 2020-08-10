#ifndef _NDVR_HELPER_HPP_
#define _NDVR_HELPER_HPP_

#include "ndvr-message.pb.h"

namespace ndn {
namespace ndvr {

inline void EncodeDvInfo(RoutingTable& v, proto::DvInfo* dvinfo_proto) {
  for (auto it = v.begin(); it != v.end(); ++it) {
    auto* entry = dvinfo_proto->add_entry();
    entry->set_prefix(it->first);
    entry->set_seq(it->second.GetSeqNum());
    entry->set_cost(it->second.GetCost());
  }
}

inline void EncodeDvInfo(RoutingTable& v, std::string& out) {
  proto::DvInfo dvinfo_proto;
  EncodeDvInfo(v, &dvinfo_proto);
  dvinfo_proto.AppendToString(&out);
}

inline RoutingTable DecodeDvInfo(const proto::DvInfo& dvinfo_proto) {
  RoutingTable dvinfo;
  for (int i = 0; i < dvinfo_proto.entry_size(); ++i) {
    const auto& entry = dvinfo_proto.entry(i);
    auto prefix = entry.prefix();
    auto seq = entry.seq();
    auto cost = entry.cost();
    RoutingEntry re = RoutingEntry(prefix, seq, cost);
    dvinfo.insert(re);
  }
  return dvinfo;
}

inline RoutingTable DecodeDvInfo(const void* buf, size_t buf_size) {
  proto::DvInfo dvinfo_proto;
  if (!dvinfo_proto.ParseFromArray(buf, buf_size)) {
    RoutingTable res;
    return res;
  }
  return DecodeDvInfo(dvinfo_proto);
}
}  // namespace ndvr
}  // namespace ndn

#endif  // _NDVR_HELPER_HPP_
