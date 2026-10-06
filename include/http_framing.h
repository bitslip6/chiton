#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <functional>
#include <map>
#include <poll.h>
#include <string>
#include <sys/socket.h>

namespace web {
namespace framing {

// This is a framing gate for the legacy, connection-close API, not a general
// HTTP/body parser. Only a fully validated zero-body request may reach routing.
// Header bytes include the final CRLFCRLF; line lengths exclude their CRLF.
constexpr size_t max_headers = 16384;
constexpr size_t max_line = 4096;
constexpr size_t max_fields = 100;
constexpr int receive_ms = 1000;

struct Result {
    int status = 0; // zero: disconnected or cancelled; 200: safe to dispatch

    // Validated headers are retained only for the dedicated upgrade gate;
    // ordinary route handlers still receive no raw HTTP headers.
    std::string line;
    std::string method;
    std::map<std::string, std::string> headers;
};

// HTTP token characters are shared by method names and header field names.
inline bool token(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos;
}

// Called only after receive() has checked raw bytes and found exactly one
// complete CRLF-terminated header block. Default to rejection on every error.
inline Result validate(const std::string &headers) {
    Result result{400, {}, {}};

    const size_t end = headers.find("\r\n");
    if (end == std::string::npos || end > max_line) return result;
    const std::string line = headers.substr(0, end);

    // Require METHOD SP origin-form-target SP HTTP-version. Do not normalize
    // tabs or repeated spaces: different interpretations can change routing.
    const size_t a = line.find(' '), b = line.find(' ', a == std::string::npos ? 0 : a + 1);
    if (a == std::string::npos || b == std::string::npos || !a || b <= a + 1 ||
        line.find(' ', b + 1) != std::string::npos) return result;

    const auto method = line.substr(0, a), target = line.substr(a + 1, b - a - 1);
    const auto version = line.substr(b + 1);
    if (!std::all_of(method.begin(), method.end(), token) || target[0] != '/' ||
        target.find('#') != std::string::npos ||
        (version != "HTTP/1.1" && version != "HTTP/1.0")) return result;
    for (unsigned char c : target) if (c <= 32 || c >= 127) return result;

    bool length_seen = false, host_seen = false, body = false;
    size_t fields = 0;

    for (size_t pos = end + 2; pos < headers.size() - 2;) {
        const size_t next = headers.find("\r\n", pos);
        if (next == std::string::npos || next - pos > max_line || ++fields > max_fields) return result;
        const auto field = headers.substr(pos, next - pos);
        const size_t colon = field.find(':');
        if (colon == std::string::npos || !colon) return result;

        // Checking the untrimmed name rejects folding and whitespace before
        // the colon. Only value-edge spaces/tabs are optional whitespace.
        auto name = field.substr(0, colon);
        if (!std::all_of(name.begin(), name.end(), token)) return result;
        for (auto &c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';

        auto value = field.substr(colon + 1);
        for (unsigned char c : value) if ((c < 32 && c != '\t') || c == 127) return result;
        const auto first = value.find_first_not_of(" \t");
        value = first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \t") - first + 1);

        if (name == "transfer-encoding") return result; // no chunked/identity framing
        if (name == "content-length") {
            // Reject even identical duplicate lengths rather than reconcile
            // potentially ambiguous framing. We only need zero/nonzero, so
            // inspecting decimal digits avoids conversion overflow entirely.
            if (length_seen || value.empty()) return result;
            length_seen = true;

            for (unsigned char c : value) {
                if (c < '0' || c > '9') return result;
                body = body || c != '0'; // no integer overflow, even for huge lengths
            }
        }

        if (name == "host") {
            if (host_seen || value.empty() || value.find_first_of(" \t,/") != std::string::npos) return result;
            host_seen = true;
        }

        auto inserted = result.headers.emplace(name, value);
        if (!inserted.second) inserted.first->second += "," + value;

        // Never wait for a body while a client waits for 100 Continue.
        if (name == "expect") return result;

        pos = next + 2;
    }

    if (version == "HTTP/1.1" && !host_seen) return result;

    // Validate all fields before applying the body policy: malformed framing
    // remains a 400 even when another field declares an unsupported body.
    if (body) return {413, {}, {}}; // legacy routes have a deliberate zero-byte body limit

    result.status = 200; result.line = line + "\r\n"; result.method = method;
    return result;
}

inline Result receive(int socket, const std::function<bool()> &running) {
    // One monotonic deadline covers the entire receive, not each fragment.
    // A client sending just often enough to avoid idle timeouts still expires.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(receive_ms);
    std::string bytes;

    while (running()) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) return {408, {}, {}};

        pollfd pending{socket, POLLIN, 0};
        // Short poll slices let shutdown cancel an otherwise idle connection.
        const int ready = poll(&pending, 1, static_cast<int>(std::min<long long>(left, 50)));
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0 || (pending.revents & POLLNVAL)) return {};
        if (!ready) continue;

        char chunk[1024];
        // Never read beyond the header budget, even if the peer floods data.
        const ssize_t n = recv(socket, chunk, std::min(sizeof(chunk), max_headers - bytes.size()), MSG_DONTWAIT);
        if (n == 0) return {}; // incomplete EOF never dispatches
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return {};
        }
        bytes.append(chunk, static_cast<size_t>(n));

        // Check raw bytes, including CR/LF pairing, before interpreting strings.
        // A trailing CR may belong to a split CRLF; defer its check until the
        // next fragment. Rescanning is bounded by the fixed header-size limit.
        for (size_t i = 0; i < bytes.size(); ++i) {
            const unsigned char c = bytes[i];
            if ((c < 32 && c != '\r' && c != '\n' && c != '\t') || c == 127 ||
                (c == '\n' && (i == 0 || bytes[i-1] != '\r')) ||
                (c == '\r' && i + 1 < bytes.size() && bytes[i+1] != '\n')) return {400, {}, {}};
        }

        const auto end = bytes.find("\r\n\r\n");
        if (end != std::string::npos) {
            // No body or second request may be hidden in the bytes already
            // read. Later-arriving bytes are discarded when the server closes
            // this connection; we do not wait for them before dispatching.
            if (end + 4 != bytes.size()) return {400, {}, {}}; // coalesced body/pipeline
            if (!running()) return {};
            return validate(bytes);
        }

        if (bytes.size() == max_headers) return {431, {}, {}};
    }

    return {};
}

} // namespace framing
} // namespace web
