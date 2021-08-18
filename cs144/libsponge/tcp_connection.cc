#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {;}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time-_tlaser; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    if(!_active)_active=true;
    _tlaser=_time;
    if(seg.header().rst){Abort();}
    _receiver.segment_received(seg);
    if(seg.header().ack)_sender.ack_received(seg.header().ackno,seg.header().win);
    if(seg.length_in_sequence_space()){
        _sender.fill_window();   
        if(!Sendseg()){
            _sender.send_empty_segment();
            Sendseg();
        }
    }
    Checkmode();
}

bool TCPConnection::active() const {return _active; }

size_t TCPConnection::write(const string &data) {
    size_t ret=_sender.write(data);
    Checkmode();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time+=ms_since_last_tick;
    if(_sender.consecutive_retransmissions()>=TCPConfig::MAX_RETX_ATTEMPTS){
        _sender.send_empty_segment(true);Sendseg();
        Abort();
    }
    else{
        _sender.tick(ms_since_last_tick);
        _sender.fill_window(true);
        Sendseg();

    }
    Endifnes();
}

void TCPConnection::end_input_stream() {_sender.end_input_stream();_sender.fill_window();Sendseg();Checkmode();}

void TCPConnection::connect() {_active=true;_sender.fill_window();Sendseg();Checkmode();}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

bool TCPConnection::Sendseg(){
    if(_sender.segments_out().empty())return false;
    while(!_sender.segments_out().empty()){
        TCPSegment sendone=_sender.segments_out().front();
        _sender.segments_out().pop();
        Segimp(sendone);
        _segments_out.push(sendone);
    }
    
    return true;

}

void TCPConnection::Segimp(TCPSegment &seg){
    if(_receiver.ackno()){
        seg.header().ack=true;
        seg.header().ackno=*_receiver.ackno();
    }
    seg.header().win=_receiver.window_size();
}

void TCPConnection::Abort(){
    _sender.set_error();_receiver.set_error();
    _active=false;
    Checkmode();
    return;
}


void TCPConnection::Endifnes(){
    Checkmode();
    if(_sender.steof()&&_receiver.steof()&&_sender.serec())
        if(_active)
            if(!_linger_after_streams_finish||time_since_last_segment_received()>=10*_cfg.rt_timeout){
                _active=false;
            }
}

void TCPConnection::Checkmode(){
    if(_receiver.steof()&&!_sender.steof())_linger_after_streams_finish=false;
}
