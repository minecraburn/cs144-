#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if(seg.header().syn) {_syn_re=true;_isn=seg.header().seqno;}
    int tem=1;
    if(seg.header().seqno==_isn&&seg.header().syn) tem=0;
    int32_t tem32=seg.header().seqno-wrap(_reassembler.fir_ua(),_isn)-tem;
    uint64_t rseqno=unwrap(seg.header().seqno-tem,_isn,_rec_seqno);
    if(tem32>=0&&tem32<static_cast<int32_t>(_reassembler.cap()) )_rec_seqno=rseqno;
    _reassembler.push_substring(seg.payload().copy(),rseqno,seg.header().fin);

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(!_syn_re)return {}; 
    if(_reassembler.otpend())
        return wrap(_reassembler.fir_ua(),_isn)+2;
    return wrap(_reassembler.fir_ua(),_isn)+1;
}

size_t TCPReceiver::window_size() const { return _reassembler.rca(); }
