#include <zlib.h>

#include <cstdio>
#include <cstring>

#include "ritm/io.hpp"

namespace ritm {

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return true;
}

bool gunzip(const std::vector<uint8_t>& in, std::string& out, std::string& err) {
    z_stream s;
    std::memset(&s, 0, sizeof s);
    if (inflateInit2(&s, 16 + MAX_WBITS) != Z_OK) {
        err = "inflateInit failed";
        return false;
    }
    s.next_in = const_cast<Bytef*>(in.data());
    s.avail_in = uInt(in.size());
    out.clear();
    char buf[65536];
    int rc;
    do {
        s.next_out = reinterpret_cast<Bytef*>(buf);
        s.avail_out = sizeof buf;
        rc = inflate(&s, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            inflateEnd(&s);
            err = "gzip data corrupt";
            return false;
        }
        out.append(buf, sizeof buf - s.avail_out);
        if (out.size() > (size_t(1) << 30)) {
            inflateEnd(&s);
            err = "gzip payload too large";
            return false;
        }
    } while (rc != Z_STREAM_END);
    inflateEnd(&s);
    return true;
}

bool gzip(const std::string& in, std::vector<uint8_t>& out) {
    z_stream s;
    std::memset(&s, 0, sizeof s);
    if (deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;
    s.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    s.avail_in = uInt(in.size());
    out.clear();
    uint8_t buf[65536];
    int rc;
    do {
        s.next_out = buf;
        s.avail_out = sizeof buf;
        rc = deflate(&s, Z_FINISH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            deflateEnd(&s);
            return false;
        }
        out.insert(out.end(), buf, buf + (sizeof buf - s.avail_out));
    } while (rc != Z_STREAM_END);
    deflateEnd(&s);
    return true;
}

bool loadProject(const std::string& path, Project& p, std::string& err) {
    std::vector<uint8_t> data;
    if (!readFile(path, data)) {
        err = "cannot read " + path;
        return false;
    }
    if (data.size() >= 4 && std::memcmp(data.data(), "FLhd", 4) == 0) return loadFlp(data, p, err);
    if (data.size() >= 2 && data[0] == 0x1F && data[1] == 0x8B) return loadAls(data, p, err);
    if (data.size() >= 2 && data[0] == 'P' && data[1] == 'K') {
        err = "zip container (.flm?) is not supported yet";
        return false;
    }
    err = "unknown project format";
    return false;
}

}  // namespace ritm
