#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : s(0), e(0), writeLen(0), cp(capacity + 1), endinput(false), buf(capacity + 1) {}

size_t ByteStream::write(const string &data) {
    int cnt = 0;
    for (char c : data) {
        int tmp = e + 1;
        if (tmp >= cp)
            tmp -= cp;
        if (tmp != s) {
            buf[e] = c;
            e = tmp;
            cnt++;
            writeLen++;
        } else
            break;
    }
    return cnt;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ret = "";
    int now = s;
    for (size_t i = 0; i < len; i++) {
        if (now == e)
            break;
        ret += buf[now];
        now++;
        if (now >= cp)
            now -= cp;
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    s += len;
    if (s >= cp)
        s -= cp;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ret = peek_output(len);
    pop_output(ret.size());
    return ret;
}

void ByteStream::end_input() { endinput = true; }

bool ByteStream::input_ended() const { return endinput; }

size_t ByteStream::buffer_size() const { return e >= s ? e - s : e + cp - s; }

bool ByteStream::buffer_empty() const { return s == e; }

bool ByteStream::eof() const { return input_ended() && (s == e); }

size_t ByteStream::bytes_written() const { return writeLen; }

size_t ByteStream::bytes_read() const { return writeLen - buffer_size(); }

size_t ByteStream::remaining_capacity() const { return cp - 1 - buffer_size(); }
