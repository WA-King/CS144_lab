#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address), time_now(0) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame ans;
    ans.header().src = this->_ethernet_address;
    ans.header().type = ans.header().TYPE_IPv4;
    ans.payload() = BufferList(dgram.serialize());
    if (mapping.find(next_hop_ip) == mapping.end() || time_now - mapping[next_hop_ip].first > 30000) {
        if (last_arp.find(next_hop_ip) == last_arp.end() || time_now - last_arp[next_hop_ip] > 5000) {
            last_arp[next_hop_ip] = time_now;
            wait_queue[next_hop_ip].push_back(ans);
            ARPMessage tmp;
            tmp.opcode = tmp.OPCODE_REQUEST;
            tmp.sender_ip_address = this->_ip_address.ipv4_numeric();
            tmp.sender_ethernet_address = this->_ethernet_address;
            tmp.target_ip_address = next_hop_ip;
            EthernetFrame arp;
            arp.header().type = arp.header().TYPE_ARP;
            arp.header().src = this->_ethernet_address;
            arp.header().dst = std::array<uint8_t, 6>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            arp.payload() = BufferList(tmp.serialize());
            _frames_out.push(arp);
        }
    } else {
        ans.header().dst = mapping[next_hop_ip].second;
        _frames_out.push(ans);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    EthernetHeader header = frame.header();
    if (header.type == header.TYPE_IPv4 && header.dst == this->_ethernet_address) {
        InternetDatagram ans;
        if (ans.parse(frame.payload()) == ParseResult::NoError)
            return ans;
    } else if (header.type == header.TYPE_ARP) {
        ARPMessage tmp;
        if (tmp.parse(frame.payload()) == ParseResult::NoError) {
            mapping[tmp.sender_ip_address] = {time_now, tmp.sender_ethernet_address};
            if (wait_queue.find(tmp.sender_ip_address) != wait_queue.end()) {
                for (EthernetFrame x : wait_queue[tmp.sender_ip_address]) {
                    x.header().dst = tmp.sender_ethernet_address;
                    _frames_out.push(x);
                }
                wait_queue.erase(tmp.sender_ip_address);
            }
            if (tmp.opcode == tmp.OPCODE_REQUEST && tmp.target_ip_address == this->_ip_address.ipv4_numeric()) {
                tmp.opcode = tmp.OPCODE_REPLY;
                tmp.target_ip_address = tmp.sender_ip_address;
                tmp.target_ethernet_address = tmp.sender_ethernet_address;
                tmp.sender_ip_address = this->_ip_address.ipv4_numeric();
                tmp.sender_ethernet_address = this->_ethernet_address;
                EthernetFrame F = frame;
                F.header().src = this->_ethernet_address;
                F.header().dst = frame.header().src;
                F.payload() = BufferList(tmp.serialize());
                _frames_out.push(F);
            }
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { time_now += ms_since_last_tick; }
