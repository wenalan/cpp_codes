#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace hft {

constexpr uint16_t kProtocolMagic = 0xA11C;
constexpr uint8_t kMsgNewOrder = 1;
constexpr uint8_t kMsgExecReport = 2;

struct WireHeader {
    uint16_t magic = kProtocolMagic;
    uint8_t msg_type = 0;
    uint8_t reserved = 0;
};

struct WireNewOrder {
    WireHeader hdr{.magic = kProtocolMagic, .msg_type = kMsgNewOrder, .reserved = 0};
    uint64_t cl_ord_id = 0;
    uint64_t md_event_id = 0;
    uint8_t side = 0;
    uint8_t ord_type = 0;
    uint8_t tif = 0;
    uint8_t _pad = 0;
    int64_t px = 0;
    int64_t qty = 0;
    int64_t t_oms_send_ns = 0;
};

struct WireExecReport {
    WireHeader hdr{.magic = kProtocolMagic, .msg_type = kMsgExecReport, .reserved = 0};
    uint64_t cl_ord_id = 0;
    uint64_t md_event_id = 0;
    uint8_t exec_type = 0;
    uint8_t _pad[3]{};
    int64_t fill_px = 0;
    int64_t fill_qty = 0;
    int64_t t_sim_recv_ns = 0;
    int64_t t_sim_send_ns = 0;
    int32_t reason = 0;
    int32_t _pad2 = 0;
};

inline std::vector<uint8_t> pack_with_length(const void* data, size_t len) {
    std::vector<uint8_t> out(sizeof(uint32_t) + len);
    uint32_t be_len = htonl(static_cast<uint32_t>(len));
    std::memcpy(out.data(), &be_len, sizeof(be_len));
    std::memcpy(out.data() + sizeof(be_len), data, len);
    return out;
}

inline bool unpack_frame(const std::vector<uint8_t>& buf, size_t& offset, std::vector<uint8_t>& payload) {
    if (offset + sizeof(uint32_t) > buf.size()) return false;
    uint32_t be_len = 0;
    std::memcpy(&be_len, buf.data() + offset, sizeof(be_len));
    uint32_t len = ntohl(be_len);
    if (offset + sizeof(uint32_t) + len > buf.size()) return false;
    payload.assign(buf.begin() + offset + sizeof(uint32_t), buf.begin() + offset + sizeof(uint32_t) + len);
    offset += sizeof(uint32_t) + len;
    return true;
}

}  // namespace hft
