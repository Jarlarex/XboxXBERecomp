#include "core/texture_pack.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <system_error>

namespace texpack {

namespace {

// DDS_HEADER and DDS_PIXELFORMAT fields and flags, as Microsoft documents the format
constexpr uint32_t kMagic = 0x20534444;  // "DDS "
constexpr uint32_t kHeaderBytes = 124, kPixelFormatBytes = 32;
constexpr uint32_t kDdsdCaps = 0x1, kDdsdHeight = 0x2, kDdsdWidth = 0x4, kDdsdPitch = 0x8, kDdsdPixelFormat = 0x1000,
                   kDdsdMipCount = 0x20000, kDdsdLinearSize = 0x80000, kDdsdDepth = 0x800000;
constexpr uint32_t kDdpfAlpha = 0x1, kDdpfFourCc = 0x4, kDdpfRgb = 0x40;
constexpr uint32_t kCapsComplex = 0x8, kCapsTexture = 0x1000, kCapsMipmap = 0x400000;
constexpr uint32_t kCaps2Cube = 0x200, kCaps2AllFaces = 0xFC00, kCaps2Volume = 0x200000;
constexpr uint32_t FourCc(char a, char b, char c, char d) {
    return uint32_t(uint8_t(a)) | uint32_t(uint8_t(b)) << 8 | uint32_t(uint8_t(c)) << 16 | uint32_t(uint8_t(d)) << 24;
}
// DXGI formats a DX10 header names
constexpr uint32_t kDxgiRgba8 = 28, kDxgiRgba8Srgb = 29, kDxgiBc1 = 71, kDxgiBc1Srgb = 72, kDxgiBc2 = 74,
                   kDxgiBc2Srgb = 75, kDxgiBc3 = 77, kDxgiBc3Srgb = 78, kDxgiBgra8 = 87, kDxgiBgra8Srgb = 91,
                   kDxgiBc7 = 98, kDxgiBc7Srgb = 99;
constexpr uint32_t kDx10Texture2D = 3, kDx10Texture3D = 4, kDx10Cube = 0x4;

void Put(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; i++) out.push_back(uint8_t(v >> (8 * i)));
}
uint32_t Get(const std::vector<uint8_t>& in, size_t at) {
    uint32_t v = 0;
    if (at + 4 <= in.size()) std::memcpy(&v, &in[at], 4);
    return v;
}

uint32_t Faces(const Image& img) { return img.cube ? 6 : 1; }

}  // namespace

uint64_t HashBytes(const uint8_t* data, size_t size, uint64_t seed) {
    constexpr uint64_t kMul = 0x9E3779B97F4A7C15ull;
    uint64_t h = seed ^ (uint64_t(size) * kMul);
    size_t i = 0;
    for (; i + 8 <= size; i += 8) {
        uint64_t w;
        std::memcpy(&w, data + i, 8);
        h = (h ^ w) * kMul;
        h ^= h >> 29;
    }
    uint64_t tail = 0;
    if (i < size) std::memcpy(&tail, data + i, size - i);
    h = (h ^ tail) * kMul;
    h ^= h >> 32;
    return h;
}

uint64_t Hash(const uint8_t* data, size_t size, const Key& key) {
    const uint64_t shape[] = {key.format, key.width, key.height, key.depth, key.levels, key.cube ? 1u : 0u, key.palette};
    return HashBytes(data, size, HashBytes(reinterpret_cast<const uint8_t*>(shape), sizeof(shape), 0x63616D626572ull));
}

std::string Name(uint64_t hash) {
    char text[17];
    std::snprintf(text, sizeof(text), "%016llX", static_cast<unsigned long long>(hash));
    return text;
}

bool ParseName(std::string_view file_name, uint64_t* hash) {
    if (file_name.size() < 17 || (file_name[16] != '.' && file_name[16] != '_')) return false;
    uint64_t v = 0;
    for (size_t i = 0; i < 16; i++) {
        char c = char(std::toupper(static_cast<unsigned char>(file_name[i])));
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return false;
        v = v << 4 | uint64_t(d);
    }
    *hash = v;
    return true;
}

uint32_t BlockBytes(Pixels p) {
    switch (p) {
        case Pixels::Bc1: return 8;
        case Pixels::Bc2: case Pixels::Bc3: case Pixels::Bc7: return 16;
        case Pixels::Bgra8: break;
    }
    return 0;
}

uint32_t LevelBytes(Pixels p, uint32_t w, uint32_t h) {
    if (uint32_t block = BlockBytes(p)) return std::max(1u, (w + 3) / 4) * std::max(1u, (h + 3) / 4) * block;
    return w * h * 4;
}

size_t ImageBytes(const Image& img) {
    size_t total = 0;
    for (uint32_t l = 0; l < img.levels; l++) {
        uint32_t w = std::max(1u, img.width >> l), h = std::max(1u, img.height >> l), d = std::max(1u, img.depth >> l);
        total += size_t(LevelBytes(img.pixels, w, h)) * d;
    }
    return total * Faces(img);
}

std::vector<uint8_t> WriteDds(const Image& img) {
    std::vector<uint8_t> out;
    bool bc7 = img.pixels == Pixels::Bc7;
    Put(out, kMagic);
    Put(out, kHeaderBytes);
    uint32_t flags = kDdsdCaps | kDdsdHeight | kDdsdWidth | kDdsdPixelFormat;
    flags |= BlockBytes(img.pixels) ? kDdsdLinearSize : kDdsdPitch;
    if (img.levels > 1) flags |= kDdsdMipCount;
    if (img.depth > 1) flags |= kDdsdDepth;
    Put(out, flags);
    Put(out, img.height);
    Put(out, img.width);
    Put(out, BlockBytes(img.pixels) ? LevelBytes(img.pixels, img.width, img.height) : img.width * 4);
    Put(out, img.depth > 1 ? img.depth : 0);
    Put(out, img.levels);
    for (int i = 0; i < 11; i++) Put(out, 0);
    // pixel format
    Put(out, kPixelFormatBytes);
    switch (img.pixels) {
        case Pixels::Bgra8:
            Put(out, kDdpfRgb | kDdpfAlpha);
            Put(out, 0);
            Put(out, 32);
            Put(out, 0x00FF0000);
            Put(out, 0x0000FF00);
            Put(out, 0x000000FF);
            Put(out, 0xFF000000);
            break;
        default:
            Put(out, kDdpfFourCc);
            Put(out, img.pixels == Pixels::Bc1   ? FourCc('D', 'X', 'T', '1')
                     : img.pixels == Pixels::Bc2 ? FourCc('D', 'X', 'T', '3')
                     : img.pixels == Pixels::Bc3 ? FourCc('D', 'X', 'T', '5')
                                                 : FourCc('D', 'X', '1', '0'));
            for (int i = 0; i < 5; i++) Put(out, 0);
            break;
    }
    uint32_t caps = kCapsTexture;
    if (img.levels > 1) caps |= kCapsMipmap | kCapsComplex;
    if (img.cube || img.depth > 1) caps |= kCapsComplex;
    Put(out, caps);
    Put(out, img.cube ? kCaps2Cube | kCaps2AllFaces : img.depth > 1 ? kCaps2Volume : 0);
    for (int i = 0; i < 3; i++) Put(out, 0);
    if (bc7) {
        Put(out, kDxgiBc7);
        Put(out, img.depth > 1 ? kDx10Texture3D : kDx10Texture2D);
        Put(out, img.cube ? kDx10Cube : 0);
        Put(out, 1);
        Put(out, 0);
    }
    out.insert(out.end(), img.data.begin(), img.data.end());
    return out;
}

bool ReadDds(const std::vector<uint8_t>& file, Image& out, std::string& why) {
    if (file.size() < 4 + kHeaderBytes || Get(file, 0) != kMagic || Get(file, 4) != kHeaderBytes) {
        why = "not a DDS file";
        return false;
    }
    Image img;
    img.height = Get(file, 12);
    img.width = Get(file, 16);
    img.depth = std::max(1u, Get(file, 24));
    img.levels = std::max(1u, Get(file, 28));
    size_t pf = 76, data = 4 + kHeaderBytes;
    uint32_t pf_flags = Get(file, pf + 4), four = Get(file, pf + 8), bits = Get(file, pf + 12);
    uint32_t caps2 = Get(file, 112);
    img.cube = (caps2 & kCaps2Cube) != 0;
    bool rgba = false;  // red in the low byte: swapped to BGRA
    if (pf_flags & kDdpfFourCc) {
        if (four == FourCc('D', 'X', 'T', '1')) img.pixels = Pixels::Bc1;
        else if (four == FourCc('D', 'X', 'T', '2') || four == FourCc('D', 'X', 'T', '3')) img.pixels = Pixels::Bc2;
        else if (four == FourCc('D', 'X', 'T', '4') || four == FourCc('D', 'X', 'T', '5')) img.pixels = Pixels::Bc3;
        else if (four == FourCc('D', 'X', '1', '0')) {
            uint32_t fmt = Get(file, data), dim = Get(file, data + 4), misc = Get(file, data + 8),
                     array = Get(file, data + 12);
            data += 20;
            if (fmt == kDxgiBgra8 || fmt == kDxgiBgra8Srgb) img.pixels = Pixels::Bgra8;
            else if (fmt == kDxgiRgba8 || fmt == kDxgiRgba8Srgb) img.pixels = Pixels::Bgra8, rgba = true;
            else if (fmt == kDxgiBc1 || fmt == kDxgiBc1Srgb) img.pixels = Pixels::Bc1;
            else if (fmt == kDxgiBc2 || fmt == kDxgiBc2Srgb) img.pixels = Pixels::Bc2;
            else if (fmt == kDxgiBc3 || fmt == kDxgiBc3Srgb) img.pixels = Pixels::Bc3;
            else if (fmt == kDxgiBc7 || fmt == kDxgiBc7Srgb) img.pixels = Pixels::Bc7;
            else {
                why = "DXGI format " + std::to_string(fmt) + " is not BGRA, RGBA or BC1, BC2, BC3, BC7";
                return false;
            }
            if (array > 1) {
                why = "texture arrays are not read";
                return false;
            }
            img.cube = (misc & kDx10Cube) != 0;
            if (dim != kDx10Texture3D) img.depth = 1;
        } else {
            why = "its compression is not DXT1, DXT3, DXT5 or a DX10 header's";
            return false;
        }
    } else if ((pf_flags & kDdpfRgb) && bits == 32) {
        uint32_t r = Get(file, pf + 16), b = Get(file, pf + 24);
        if (r == 0x00FF0000 && b == 0x000000FF) img.pixels = Pixels::Bgra8;
        else if (r == 0x000000FF && b == 0x00FF0000) img.pixels = Pixels::Bgra8, rgba = true;
        else {
            why = "32-bit pixels in an order other than BGRA or RGBA";
            return false;
        }
    } else {
        why = "a pixel format other than 32-bit colour or block compression";
        return false;
    }
    if (!img.width || !img.height || img.width > 16384 || img.height > 16384 || img.levels > 15) {
        why = "a size of " + std::to_string(img.width) + " x " + std::to_string(img.height);
        return false;
    }
    if (!(Get(file, 8) & kDdsdMipCount)) img.levels = 1;
    size_t bytes = ImageBytes(img);
    if (file.size() < data + bytes) {
        why = "shorter than its levels";
        return false;
    }
    img.data.assign(file.begin() + std::ptrdiff_t(data), file.begin() + std::ptrdiff_t(data + bytes));
    if (rgba)
        for (size_t i = 0; i + 3 < img.data.size(); i += 4) std::swap(img.data[i], img.data[i + 2]);
    out = std::move(img);
    return true;
}

void MakeMips(Image& img) {
    if (img.pixels != Pixels::Bgra8 || img.cube || img.depth > 1 || img.levels != 1) return;
    uint32_t w = img.width, h = img.height;
    std::vector<uint8_t> level(img.data);
    while (w > 1 || h > 1) {
        uint32_t nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
        std::vector<uint8_t> next(size_t(nw) * nh * 4);
        for (uint32_t y = 0; y < nh; y++) {
            for (uint32_t x = 0; x < nw; x++) {
                for (int c = 0; c < 4; c++) {
                    uint32_t sum = 0;
                    for (uint32_t dy = 0; dy < 2; dy++)
                        for (uint32_t dx = 0; dx < 2; dx++) {
                            uint32_t sx = std::min(w - 1, x * 2 + dx), sy = std::min(h - 1, y * 2 + dy);
                            sum += level[(size_t(sy) * w + sx) * 4 + c];
                        }
                    next[(size_t(y) * nw + x) * 4 + c] = uint8_t((sum + 2) / 4);
                }
            }
        }
        img.data.insert(img.data.end(), next.begin(), next.end());
        level = std::move(next);
        w = nw;
        h = nh;
        img.levels++;
    }
}

std::unordered_map<uint64_t, std::filesystem::path> IndexPack(const std::filesystem::path& folder, size_t* skipped) {
    std::unordered_map<uint64_t, std::filesystem::path> index;
    size_t odd = 0;
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(folder, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (ext != ".dds" && ext != ".png") continue;
        uint64_t hash;
        if (!ParseName(it->path().filename().string(), &hash)) {
            odd++;
            continue;
        }
        index.emplace(hash, it->path());
    }
    if (skipped) *skipped = odd;
    return index;
}

}  // namespace texpack
