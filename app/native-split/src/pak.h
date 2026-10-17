// pak.h — reading a Chromium .pak resource file. One concern, pure (no Edge, no
// Win32; unit-tested in test/pak_test.cpp): split a version 5 pak into its
// resources and find the one holding a given text, decompressing brotli and
// gzip resources through a function the caller supplies (Edge's own decoders
// in the DLL, a fake in the test).
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace fixedge::pak {

// The version 5 layout: a header {version u32, encoding u8, 3 bytes padding,
// resource count u16, alias count u16}, then count+1 entries {id u16, offset
// u32} (the last only closes the final resource), then the data.
inline constexpr uint32_t kSupportedVersion = 5;
inline constexpr size_t kHeaderSize = 12;
inline constexpr size_t kEntrySize = 6;
inline constexpr size_t kCountOffset = 8;

// A brotli-compressed resource starts with these two bytes and the size it
// decompresses to (six bytes, little endian), then the brotli stream. A gzip
// one starts with the gzip magic and ends with the size it decompresses to (four
// bytes, little endian). Anything else is stored as it is.
inline constexpr uint8_t kBrotliMagic0 = 0x1e;
inline constexpr uint8_t kBrotliMagic1 = 0x9b;
inline constexpr size_t kBrotliSizeBytes = 6;
inline constexpr size_t kBrotliHeaderSize = 2 + kBrotliSizeBytes;
inline constexpr uint8_t kGzipMagic0 = 0x1f;
inline constexpr uint8_t kGzipMagic1 = 0x8b;
inline constexpr size_t kGzipSizeBytes = 4;
inline constexpr size_t kGzipMinimumSize = 2 + kGzipSizeBytes;

enum class Encoding {
  kPlain,
  kBrotli,
  kGzip
};

struct Resource {
  uint16_t id = 0;
  const uint8_t* data = nullptr;
  size_t size = 0;
};

// Decompresses `size` bytes at `data` (a stream in `encoding`) into
// `decodedSize` bytes. False when it cannot.
using Decompress =
    std::function<bool(Encoding encoding, const uint8_t* data, size_t size,
                       size_t decodedSize, std::string* out)>;

template <typename T>
inline T ReadLittleEndian(const uint8_t* bytes) {
  T value = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    value |= static_cast<T>(static_cast<T>(bytes[i]) << (8 * i));
  }
  return value;
}

inline bool ReadResources(const std::vector<uint8_t>& file,
                          std::vector<Resource>* resources) {
  const bool hasHeader = file.size() >= kHeaderSize;
  if (!hasHeader ||
      ReadLittleEndian<uint32_t>(file.data()) != kSupportedVersion) {
    return false;
  }
  const size_t count = ReadLittleEndian<uint16_t>(file.data() + kCountOffset);
  const size_t entriesEnd = kHeaderSize + (count + 1) * kEntrySize;
  if (file.size() < entriesEnd) {
    return false;
  }
  for (size_t i = 0; i < count; i++) {
    const uint8_t* entry = file.data() + kHeaderSize + i * kEntrySize;
    const uint8_t* next = entry + kEntrySize;
    const size_t begin = ReadLittleEndian<uint32_t>(entry + 2);
    const size_t end = ReadLittleEndian<uint32_t>(next + 2);
    const bool isInFile = begin <= end && end <= file.size();
    if (!isInFile) {
      return false;
    }
    resources->push_back(Resource{ReadLittleEndian<uint16_t>(entry),
                                  file.data() + begin, end - begin});
  }
  return true;
}

inline Encoding EncodingOf(const Resource& resource) {
  const bool isBrotli = resource.size > kBrotliHeaderSize &&
                        resource.data[0] == kBrotliMagic0 &&
                        resource.data[1] == kBrotliMagic1;
  if (isBrotli) {
    return Encoding::kBrotli;
  }
  const bool isGzip = resource.size > kGzipMinimumSize &&
                      resource.data[0] == kGzipMagic0 &&
                      resource.data[1] == kGzipMagic1;
  return isGzip ? Encoding::kGzip : Encoding::kPlain;
}

// How many bytes `resource` has once decompressed (what it says up front).
inline size_t DecodedSize(const Resource& resource) {
  size_t size = resource.size;
  switch (EncodingOf(resource)) {
    case Encoding::kBrotli: {
      size = 0;
      for (size_t i = 0; i < kBrotliSizeBytes; i++) {
        size |= static_cast<size_t>(resource.data[2 + i]) << (8 * i);
      }
      break;
    }
    case Encoding::kGzip:
      size = ReadLittleEndian<uint32_t>(resource.data + resource.size -
                                        kGzipSizeBytes);
      break;
    case Encoding::kPlain:
      break;
  }
  return size;
}

// What `resource` holds, decompressed; empty when it cannot be.
inline std::string Contents(const Resource& resource,
                            const Decompress& decompress) {
  const Encoding encoding = EncodingOf(resource);
  if (encoding == Encoding::kPlain) {
    return std::string(reinterpret_cast<const char*>(resource.data),
                       resource.size);
  }
  const size_t skipped = encoding == Encoding::kBrotli ? kBrotliHeaderSize : 0;
  std::string text;
  const bool isDecoded =
      decompress &&
      decompress(encoding, resource.data + skipped, resource.size - skipped,
                 DecodedSize(resource), &text);
  return isDecoded ? text : std::string();
}

// The text of the first resource that holds `needle` and is between `minSize`
// and `maxSize` bytes once decompressed (a compressed resource says its size up
// front, so the rest are never decompressed); empty if none does.
inline std::string FindContaining(const std::vector<uint8_t>& file,
                                  const std::string& needle, size_t minSize,
                                  size_t maxSize,
                                  const Decompress& decompress) {
  std::vector<Resource> resources;
  if (!ReadResources(file, &resources)) {
    return std::string();
  }
  for (const Resource& resource : resources) {
    const size_t size = DecodedSize(resource);
    if (size < minSize || size > maxSize) {
      continue;
    }
    std::string text = Contents(resource, decompress);
    if (text.find(needle) != std::string::npos) {
      return text;
    }
  }
  return std::string();
}

}  // namespace fixedge::pak
