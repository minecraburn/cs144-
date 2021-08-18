#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    , _rto{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno-_ack_seqno; }

void TCPSender::fill_window() {
    if(!_syn_flag){
        TCPSegment sentone;
        _syn_flag=true;
        sentone.header().syn=true;
        sentone.header().seqno=wrap(_next_seqno,_isn);
        _next_seqno+=sentone.length_in_sequence_space();
        if(!_tmlf)_tmlf=_rto;
        _segque.push(sentone);
        _segments_out.push(sentone);
        return;
    }
    uint16_t win=_winsiz>0?_winsiz:1;
    uint16_t maxsegs=TCPConfig::MAX_PAYLOAD_SIZE;
    uint64_t biflight=_next_seqno-_ack_seqno;
    bool whended=false;
    while(win>biflight&&!whended){
        if(_stream.buffer_empty()){
            if(_stream.eof()&&!_fin_sended){
                _fin_sended=true;
                _fin_flag=true;
                TCPSegment sentones;
                sentones.header().fin=true;
                sentones.header().seqno=wrap(_next_seqno,_isn);
                _next_seqno+=sentones.length_in_sequence_space();
                if(!_tmlf)_tmlf=_rto;
                _segque.push(sentones);
                _segments_out.push(sentones);
                _fin_sended=true;
            }
            return;
        }
        uint16_t readsi=win-biflight>maxsegs?maxsegs:win-biflight;
        std::string ss=_stream.read(readsi);
        TCPSegment sentone;
        sentone.payload()=Buffer(std::move(ss));
        sentone.header().seqno=wrap(_next_seqno,_isn);

        if(_stream.eof()&&_stream.buffer_empty()){_fin_flag=true;whended=true;}
        if(_fin_flag&&sentone.length_in_sequence_space()<win-biflight)
            {sentone.header().fin=true;_fin_sended=true;}
        _next_seqno+=sentone.length_in_sequence_space();
        win-=sentone.length_in_sequence_space();
        if(!_tmlf)_tmlf=_rto;
        _segque.push(sentone);
        _segments_out.push(sentone);

    }

}


void TCPSender::fill_window(bool nosyn) {
    if(!_syn_flag&&!nosyn){
        TCPSegment sentone;
        _syn_flag=true;
        sentone.header().syn=true;
        sentone.header().seqno=wrap(_next_seqno,_isn);
        _next_seqno+=sentone.length_in_sequence_space();
        if(!_tmlf)_tmlf=_rto;
        _segque.push(sentone);
        _segments_out.push(sentone);
        return;
    }
    uint16_t win=_winsiz>0?_winsiz:1;
    uint16_t maxsegs=TCPConfig::MAX_PAYLOAD_SIZE;
    uint64_t biflight=_next_seqno-_ack_seqno;
    bool whended=false;
    while(win>biflight&&!whended){
        if(_stream.buffer_empty()){
            if(_stream.eof()&&!_fin_sended){
                _fin_sended=true;
                _fin_flag=true;
                TCPSegment sentones;
                sentones.header().fin=true;
                sentones.header().seqno=wrap(_next_seqno,_isn);
                _next_seqno+=sentones.length_in_sequence_space();
                if(!_tmlf)_tmlf=_rto;
                _segque.push(sentones);
                _segments_out.push(sentones);
                _fin_sended=true;
            }
            return;
        }
        uint16_t readsi=win-biflight>maxsegs?maxsegs:win-biflight;
        std::string ss=_stream.read(readsi);
        TCPSegment sentone;
        sentone.payload()=Buffer(std::move(ss));
        sentone.header().seqno=wrap(_next_seqno,_isn);

        if(_stream.eof()&&_stream.buffer_empty()){_fin_flag=true;whended=true;}
        if(_fin_flag&&sentone.length_in_sequence_space()<win-biflight)
            {sentone.header().fin=true;_fin_sended=true;}
        _next_seqno+=sentone.length_in_sequence_space();
        win-=sentone.length_in_sequence_space();
        if(!_tmlf)_tmlf=_rto;
        _segque.push(sentone);
        _segments_out.push(sentone);

    }

}


//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t aacno=unwrap(ackno,_isn,_next_seqno);
    if(aacno>_next_seqno)return;
    if(aacno>_ack_seqno)_ack_seqno=aacno;
    _winsiz=window_size;
    bool newdare=false;
    while(!_segque.empty()&&unwrap(_segque.front().header().seqno,_isn,_next_seqno)+_segque.front().length_in_sequence_space()<=aacno){
        newdare=true;
        _segque.pop();
    }
    fill_window();
    if(newdare){
        _rto=_initial_retransmission_timeout;
        _num_ret=0;
        _tmlf=_rto;
    }
    if(_segque.empty())_tmlf=0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if(!_tmlf)return;
    if(_segque.empty()){_tmlf=0;return;}
    if(_tmlf>ms_since_last_tick){_tmlf-=ms_since_last_tick;return;}
    _segments_out.push(_segque.front());
    _num_ret+=1;
    if(_winsiz||!_ack_seqno)_rto*=2;
    _tmlf=_rto;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _num_ret; }

void TCPSender::send_empty_segment() {
        TCPSegment sentone;
        sentone.header().seqno=wrap(_next_seqno,_isn);
        _segments_out.push(sentone);
}
void TCPSender::send_empty_segment(bool rst) {
        TCPSegment sentone;
        sentone.header().seqno=wrap(_next_seqno,_isn);
        if(rst){
            sentone.header().rst=true;   
            _next_seqno+=sentone.length_in_sequence_space();
            if(!_tmlf)_tmlf=_rto;
            _segque.push(sentone);
        }
        _segments_out.push(sentone);
}

void TCPSender::send_fin_segment(bool wait) {
        TCPSegment sentone;
        sentone.header().seqno=wrap(_next_seqno,_isn);
        sentone.header().fin=true;   
        if(wait){
            _next_seqno+=sentone.length_in_sequence_space();
            if(!_tmlf)_tmlf=_rto;
            _segque.push(sentone);
        }
        _segments_out.push(sentone);
}
