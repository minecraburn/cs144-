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
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
    _mapae[_ip_address.ipv4_numeric()]=_ethernet_address;
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    if(_mapae.count(next_hop_ip)){
        BufferList ss=dgram.serialize();
        EthernetFrame sende;
        sende.payload()=ss;
        EthernetHeader eh;
        eh.dst=_mapae[next_hop_ip];
        eh.src=_ethernet_address;
        eh.type=EthernetHeader::TYPE_IPv4;
        sende.header()=eh;
        _frames_out.push(sende);return;
    }
    _queind.push(dgram);
    _queip.push(next_hop_ip);
    if(_reqat.count(next_hop_ip))return;
    ARPMessage arpme;
    arpme.opcode=ARPMessage::OPCODE_REQUEST;
    arpme.sender_ip_address=_ip_address.ipv4_numeric();
    arpme.target_ip_address=next_hop_ip;
    arpme.sender_ethernet_address=_ethernet_address;
    string ss=arpme.serialize();
    EthernetFrame sende;
    sende.payload()=Buffer(std::move(ss));
    EthernetHeader eh;
    eh.dst=ETHERNET_BROADCAST;
    eh.src=_ethernet_address;
    eh.type=EthernetHeader::TYPE_ARP;
    sende.header()=eh;
    _frames_out.push(sende);
    _reqat[next_hop_ip]=_time;
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst!=_ethernet_address&&frame.header().dst!=ETHERNET_BROADCAST) return {};
    if(frame.header().type==EthernetHeader::TYPE_IPv4){
        InternetDatagram rec;
        if(rec.parse(frame.payload())==ParseResult::NoError){
            uint32_t myaddr;
            EthernetAddress myead;
            myaddr=rec.header().src;
            myead=frame.header().src;
            _mapae[myaddr]=myead;
            if(myaddr!=_ip_address.ipv4_numeric()) _mapat[myaddr]=_time;
            return rec;
        }
    }
    if(frame.header().type==EthernetHeader::TYPE_ARP){
        ARPMessage recarp;
        if(recarp.parse(frame.payload())==ParseResult::NoError){
            uint32_t myaddr;
            EthernetAddress myead;
            myaddr=recarp.sender_ip_address;
            myead=recarp.sender_ethernet_address;
            _mapae[myaddr]=myead;
            if(myaddr!=_ip_address.ipv4_numeric()) _mapat[myaddr]=_time;
            if(recarp.opcode==ARPMessage::OPCODE_REQUEST&&_mapae.count(recarp.target_ip_address)){
                ARPMessage arpme;
                arpme.opcode=ARPMessage::OPCODE_REPLY;
                arpme.sender_ip_address=recarp.target_ip_address;
                arpme.sender_ethernet_address=_mapae[recarp.target_ip_address];
                arpme.target_ethernet_address=myead;
                arpme.target_ip_address=myaddr;
                string ss=arpme.serialize();
                EthernetFrame sende;
                sende.payload()=Buffer(std::move(ss));
                EthernetHeader eh;
                eh.dst=myead;
                eh.src=_ethernet_address;
                eh.type=EthernetHeader::TYPE_ARP;
                sende.header()=eh;
                _frames_out.push(sende);
            }else{
                myaddr=recarp.target_ip_address;
                myead=recarp.target_ethernet_address;
                _mapae[myaddr]=myead;
                if(myaddr!=_ip_address.ipv4_numeric()) _mapat[myaddr]=_time;
            }
            std::queue<InternetDatagram> qid;
            std::queue<uint32_t> qip;
            while(!_queip.empty()){
                InternetDatagram dgra;
                uint32_t dip;
                dip=_queip.front();
                _queip.pop();
                dgra=_queind.front();
                _queind.pop();
                if(_mapae.count(dip)){
                    BufferList ss=dgra.serialize();
                    EthernetFrame sende;
                    sende.payload()=ss;
                    EthernetHeader eh;
                    eh.dst=_mapae[dip];
                    eh.src=_ethernet_address;
                    eh.type=EthernetHeader::TYPE_IPv4;
                    sende.header()=eh;
                    _frames_out.push(sende); 
                }else{
                    qid.push(dgra);
                    qip.push(dip);
                }
                
            }
            _queind=qid;
            _queip=qip;
        }
    }
    
    return {};

}


void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _time+=ms_since_last_tick;
    for(auto it=_mapat.begin();it!=_mapat.end();){
        if(it->second+30000<=_time){
            _mapae.erase(it->first);
            _mapat.erase(it++);
        }else
            it++;
    }
    for(auto it=_reqat.begin();it!=_reqat.end();){
        if(it->second+5000<=_time){
            _reqat.erase(it++);
        }else
            it++;
    }

}
