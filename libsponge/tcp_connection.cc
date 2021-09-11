#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return since_last_segment; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    since_last_segment = 0;
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        live = false;
        return;
    }
    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.length_in_sequence_space() != 0)
        send_segment();
    else if (_receiver.has_SYN()) {
        _sender.fill_window();
        if (!_sender.segments_out().empty())
            send_segment();
    }

    if (_receiver.stream_out().input_ended() && _sender.stream_in().eof() == false) {
        _linger_after_streams_finish = false;
    }
    check_done();
}

bool TCPConnection::active() const { return live; }

size_t TCPConnection::write(const string &data) {
    size_t ans = _sender.stream_in().write(data);
    _sender.fill_window();
    if (!_sender.segments_out().empty())
        send_segment();
    return ans;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst();
        return;
    }
    since_last_segment += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    while (!_sender.segments_out().empty()) {
        TCPSegment tmp = _sender.segments_out().front();
        _sender.segments_out().pop();
        optional<WrappingInt32> ack = _receiver.ackno();
        if (ack.has_value()) {
            tmp.header().ack = true;
            tmp.header().ackno = ack.value();
        }
        tmp.header().win = _receiver.window_size();
        _segments_out.push(tmp);
    }
    check_done();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    send_segment();
}

void TCPConnection::connect() {
    send_segment();
    live = true;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::send_rst() {
    _sender.send_empty_segment();
    TCPSegment tmp = _sender.segments_out().front();
    _sender.segments_out().pop();
    tmp.header().rst = true;
    _segments_out.push(tmp);
    live = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

void TCPConnection::send_segment() {
    _sender.fill_window();
    if (_sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    while (!_sender.segments_out().empty()) {
        TCPSegment tmp = _sender.segments_out().front();
        _sender.segments_out().pop();
        optional<WrappingInt32> ack = _receiver.ackno();
        if (ack.has_value()) {
            tmp.header().ack = true;
            tmp.header().ackno = ack.value();
        }
        tmp.header().win = _receiver.window_size();
        _segments_out.push(tmp);
    }
    check_done();
}

void TCPConnection::check_done() {
    if (_receiver.stream_out().input_ended() && _sender.isend() && _sender.bytes_in_flight() == 0) {
        if (_linger_after_streams_finish) {
            if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
                live = false;
            }
        } else {
            live = false;
        }
    }
}