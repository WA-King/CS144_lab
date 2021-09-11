#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , input(capacity)
    , vis(capacity)
    , s(0)
    , id(0)
    , inputLen(0)
    , finishInput(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (data.size() == 0) {
        if (eof)
            finishInput = true;
    } else {
        size_t l = max(id, index), r = min(id + _capacity - 1, index + data.size() - 1);
        if (l > r)
            return;
        size_t now = s + (l - id);
        if (now >= _capacity)
            now -= _capacity;
        for (size_t i = l; i <= r; i++) {
            if (!vis[now]) {
                vis[now] = true;
                input[now] = data[i - index];
                inputLen++;
            }
            now++;
            if (now >= _capacity)
                now -= _capacity;
        }
        if (r - index + 1 == data.size() && eof)
            finishInput = true;
    }
    std::string tmp = "";
    size_t tmp_id = s;
    while (tmp.size() < _capacity && vis[tmp_id]) {
        tmp += input[tmp_id];
        tmp_id++;
        if (tmp_id >= _capacity)
            tmp_id -= _capacity;
    }
    int cnt = _output.write(tmp);
    while (cnt--) {
        vis[s] = false;
        s++;
        if (s >= _capacity)
            s -= _capacity;
        id++;
        inputLen--;
    }
    if (empty() && finishInput) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return inputLen; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
