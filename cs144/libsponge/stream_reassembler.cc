#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    _buffer.reserve(capacity);_used.assign(capacity,false);
}


//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    bool read=false,outer=false;string dataw;
    if(index<=_fir_uas) read=true;
    if(index+data.length()>_fir_uas+_capacity)outer=true;
    size_t lastb=(index+data.length())>(_fir_uas+_capacity)?(_fir_uas+_capacity):(index+data.length());
    size_t i;size_t canus=_output.remaining_capacity();
    for(i=index>_fir_uas?index:_fir_uas;i<lastb;i++)
    {
        if(!_used[i%_capacity]){
            _buffer[i%_capacity]=data[i-index];
            _used[i%_capacity]=true;
            _uasb++;
        } 
        if(read&&canus){
            dataw.push_back(_buffer[i%_capacity]);
            _used[i%_capacity]=false;
            _uasb--;
            _fir_uas++;
            canus--;
        }
    }
    if(!read&&_used[_fir_uas%_capacity]){i=_fir_uas;read=true;}
    if(read){
        while(_used[i%_capacity]&&canus){
            dataw.push_back(_buffer[i%_capacity]);
            _used[i%_capacity]=false;
            _uasb--;
            _fir_uas++;
            i++;
            canus--;
        }
        _output.write(dataw);
    }
    if(eof&&!outer)_eof_flag=true;
    if(_eof_flag&&empty())_output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _uasb; }

bool StreamReassembler::empty() const { return _uasb==0; }
