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
    , l(0)
    , win_size(1)
    , crm(0)
    , timer(0)
    , rto(0)
    , timer_start(false)
    , end(false)
    , _stream(capacity)
    , pre_ack(0)
    , byte_fight(0) {}

uint64_t TCPSender::bytes_in_flight() const { return byte_fight; }
void TCPSender::send_segment(TCPSegment ans) {
    _segments_out.push(ans);
    outstanding.push(ans);
    size_t len = ans.length_in_sequence_space();
    _next_seqno += len;
    byte_fight += len;
    if (len != 0 && timer_start == false) {
        timer_start = true;
        rto = _initial_retransmission_timeout;
        timer = 0;
    }
}
void TCPSender::fill_window() {
    if (end)
        return;
    if (next_seqno_absolute() == 0) {
        TCPSegment ans;
        ans.header().syn = true;
        ans.header().seqno = _isn;
        send_segment(ans);
    } else {
        uint64_t r = 1;
        if (r < win_size)
            r = win_size;
        r += pre_ack - 1;
        while (end == false && next_seqno_absolute() <= r) {
            TCPSegment ans;
            if (_stream.eof()) {
                ans.header().fin = true;
                ans.header().seqno = next_seqno();
                send_segment(ans);
                end = true;
            } else {
                size_t len = r - next_seqno_absolute() + 1;
                if (len > TCPConfig::MAX_PAYLOAD_SIZE)
                    len = TCPConfig::MAX_PAYLOAD_SIZE;
                string tmp = _stream.read(len);
                ans.header().seqno = next_seqno();
                if (tmp.size() < r - next_seqno_absolute() + 1 && _stream.eof()) {
                    ans.header().fin = true;
                    end = true;
                }
                Buffer send_buf(move(tmp));
                ans.payload() = send_buf;
                if (ans.length_in_sequence_space() == 0)
                    break;
                send_segment(ans);
            }
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t ack = unwrap(ackno, _isn, _next_seqno);
    if (ack > next_seqno_absolute())
        return;
    if (ack > pre_ack) {
        byte_fight -= ack - pre_ack;
        pre_ack = ack;
        rto = _initial_retransmission_timeout;
        if (!outstanding.empty()) {
            timer_start = true;
            timer = 0;
        }
        crm = 0;
    }
    win_size = window_size;
    while (!outstanding.empty()) {
        TCPSegment tmp = outstanding.front();
        size_t len = tmp.length_in_sequence_space();
        uint64_t tmp_ack = unwrap(tmp.header().seqno + len, _isn, _next_seqno);
        if (tmp_ack <= ack) {
            outstanding.pop();
        } else
            break;
    }
    if (outstanding.empty())
        timer_start = false;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!timer_start)
        return;
    timer += ms_since_last_tick;
    if (timer >= rto) {
        TCPSegment tmp = outstanding.front();
        _segments_out.push(tmp);
        if (win_size != 0) {
            rto *= 2;
            crm++;
        }
        timer = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return crm; }

void TCPSender::send_empty_segment() {
    TCPSegment ans;
    ans.header().seqno = next_seqno();
    _segments_out.push(ans);
}

bool TCPSender::isend() { return end; }
