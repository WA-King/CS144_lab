#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader Header = seg.header();
    bool eof = false;
    if (Header.syn) {
        ISN = Header.seqno;
        receive_SYN = true;
        Header.seqno = Header.seqno + 1;
    }
    if (!receive_SYN)
        return;
    if (receive_SYN && Header.fin) {
        eof = true;
    }
    uint64_t checkpoint = stream_out().bytes_written() + 1;
    uint64_t Index = unwrap(Header.seqno, ISN, checkpoint);
    if (Index == 0)
        return;
    string data = seg.payload().copy();
    _reassembler.push_substring(data, Index - 1, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    optional<WrappingInt32> ret;
    if (receive_SYN) {
        uint64_t tmp = stream_out().bytes_written();
        tmp++;
        if (stream_out().input_ended())
            tmp++;
        ret = wrap(tmp, ISN);
    }
    return ret;
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
