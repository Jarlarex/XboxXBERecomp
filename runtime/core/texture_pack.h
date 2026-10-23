#pragma once
// Texture dumps and packs: a texture the title uploads is named by a hash of what it is in guest memory, dumped as a DDS
// of what the renderer uploads, and replaced by a file of that name from the player's pack. Portable: the renderer hashes,
// dumps and loads in gpu/d3d12.cpp, PNG files are read there through Windows' imaging component.
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace texpack {

// what a guest texture is: its bytes in guest memory, the console format and shape, and for a P8 texture its palette's
// hash (0 without one). The same texture hashes the same on every run and PC.
struct Key {
    uint32_t format = 0;
    uint32_t width = 0, height = 0, depth = 1, levels = 1;
    bool cube = false;
    uint64_t palette = 0;
};
uint64_t Hash(const uint8_t* data, size_t size, const Key& key);
uint64_t HashBytes(const uint8_t* data, size_t size, uint64_t seed);

// file names: the hash as 16 upper case hex digits, then anything after an underscore, then .dds or .png
std::string Name(uint64_t hash);
bool ParseName(std::string_view file_name, uint64_t* hash);

// the pixel formats a DDS here holds: what the renderer uploads (BGRA, BC1 to BC3) and, in packs, BC7
enum class Pixels : uint8_t { Bgra8, Bc1, Bc2, Bc3, Bc7 };
uint32_t BlockBytes(Pixels p);   // 0 for an uncompressed format
uint32_t LevelBytes(Pixels p, uint32_t w, uint32_t h);  // one slice of a level, 4 x 4 blocks for BC

// a texture as a DDS holds it: faces (6 for a cube) or slices of every level, level after level within a face
struct Image {
    Pixels pixels = Pixels::Bgra8;
    uint32_t width = 0, height = 0, depth = 1, levels = 1;
    bool cube = false;
    std::vector<uint8_t> data;  // face 0 levels 0..n-1, face 1 ..., each level depth slices, rows tightly packed
};
size_t ImageBytes(const Image& img);
std::vector<uint8_t> WriteDds(const Image& img);
// false with why on a file this does not read (a format other than the five, a cube or volume packed oddly)
bool ReadDds(const std::vector<uint8_t>& file, Image& out, std::string& why);

// mip levels of a BGRA image made by averaging each 2 x 2 block (an odd edge repeats its last texel)
void MakeMips(Image& img);

// a pack: every .dds and .png named by a hash under a folder, first one found for a hash wins
std::unordered_map<uint64_t, std::filesystem::path> IndexPack(const std::filesystem::path& folder, size_t* skipped);

}  // namespace texpack
