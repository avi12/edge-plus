// Unit tests for the pak reader (pak.h). No Edge/Win32 deps. Run via test\run.cmd.
#include <cstdio>
#include <string>
#include <vector>
#include "../src/pak.h"

using namespace fixedge::pak;

namespace {

// A failed check ends the run: the checks after it read results it vouched for.
struct CheckFailed {};

int g_checks = 0;
void Check(bool ok, const char* what) {
  g_checks++;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    throw CheckFailed{};
  }
}

void PutLittleEndian(std::vector<uint8_t>* bytes, uint64_t value,
                     size_t width) {
  for (size_t i = 0; i < width; i++) {
    bytes->push_back(static_cast<uint8_t>(value >> (8 * i)));
  }
}

// A version 5 pak holding `resources` (id, bytes), in order.
std::vector<uint8_t> MakePak(
    const std::vector<std::pair<uint16_t, std::string>>& resources) {
  std::vector<uint8_t> file;
  PutLittleEndian(&file, 5, 4);
  file.push_back(1);
  file.insert(file.end(), 3, 0);
  PutLittleEndian(&file, resources.size(), 2);
  PutLittleEndian(&file, 0, 2);
  size_t offset = kHeaderSize + (resources.size() + 1) * kEntrySize;
  for (const auto& [resourceId, bytes] : resources) {
    PutLittleEndian(&file, resourceId, 2);
    PutLittleEndian(&file, offset, 4);
    offset += bytes.size();
  }
  PutLittleEndian(&file, 0, 2);
  PutLittleEndian(&file, offset, 4);
  for (const auto& [resourceId, bytes] : resources) {
    file.insert(file.end(), bytes.begin(), bytes.end());
  }
  return file;
}

// A "brotli" resource whose stream is the text reversed, to be undone by the
// fake decompressor below.
std::string FakeBrotli(const std::string& text) {
  std::vector<uint8_t> header = {kBrotliMagic0, kBrotliMagic1};
  PutLittleEndian(&header, text.size(), kBrotliSizeBytes);
  std::string resource(header.begin(), header.end());
  resource.append(text.rbegin(), text.rend());
  return resource;
}

// A "gzip" resource: the magic, the text reversed, then its length.
std::string FakeGzip(const std::string& text) {
  std::string resource = {static_cast<char>(kGzipMagic0),
                          static_cast<char>(kGzipMagic1)};
  resource.append(text.rbegin(), text.rend());
  std::vector<uint8_t> footer;
  PutLittleEndian(&footer, text.size(), kGzipSizeBytes);
  resource.append(footer.begin(), footer.end());
  return resource;
}

// Undoes both fakes: the stream is the text reversed (a gzip one also carries
// its two magic bytes and four footer bytes, which Edge's decoder reads itself).
bool FakeDecompress(Encoding encoding, const uint8_t* data, size_t size,
                    size_t decodedSize, std::string* out) {
  const bool isGzip = encoding == Encoding::kGzip;
  const size_t skipped = isGzip ? 2 : 0;
  const size_t length = isGzip ? size - 2 - kGzipSizeBytes : size;
  out->assign(reinterpret_cast<const char*>(data) + skipped, length);
  out->assign(out->rbegin(), out->rend());
  return length == decodedSize;
}

void RunChecks() {
  const std::string plain = "hello needle world";
  const std::string packed = "packed text with a needle in it";
  const std::string zipped = "zipped text with a gzip needle";
  const std::vector<uint8_t> file = MakePak({{7, "no match here"},
                                             {9, plain},
                                             {12, FakeBrotli(packed)},
                                             {15, FakeGzip(zipped)}});

  std::vector<Resource> resources;
  Check(ReadResources(file, &resources) && resources.size() == 4,
        "four resources are read");
  Check(resources[1].id == 9 && resources[1].size == plain.size(),
        "ids and sizes are read");
  Check(Contents(resources[1], FakeDecompress) == plain,
        "a plain resource is its bytes");
  Check(EncodingOf(resources[2]) == Encoding::kBrotli &&
            Contents(resources[2], FakeDecompress) == packed,
        "a brotli resource is decompressed");
  Check(EncodingOf(resources[3]) == Encoding::kGzip &&
            DecodedSize(resources[3]) == zipped.size() &&
            Contents(resources[3], FakeDecompress) == zipped,
        "a gzip resource is decompressed");
  Check(Contents(resources[2], nullptr).empty(),
        "a brotli resource with no decompressor is nothing");

  Check(FindContaining(file, "needle", 0, 1000, FakeDecompress) == plain,
        "the first resource holding the needle wins");
  Check(FindContaining(file, "needle in it", 0, 1000, FakeDecompress) == packed,
        "a compressed one is found too");
  Check(FindContaining(file, "gzip needle", 0, 1000, FakeDecompress) == zipped,
        "a gzip one is found too");
  Check(FindContaining(file, "needle", packed.size(), 1000, FakeDecompress) ==
            packed,
        "the size window skips a resource that is too small");
  Check(FindContaining(file, "needle", 0, 5, FakeDecompress).empty(),
        "the size window skips every resource that is too big");
  Check(FindContaining(file, "absent", 0, 1000, FakeDecompress).empty(),
        "no needle, no text");

  std::vector<uint8_t> wrongVersion = file;
  wrongVersion[0] = 4;
  Check(!ReadResources(wrongVersion, &resources), "another version is refused");
  const std::vector<uint8_t> truncated(file.begin(), file.begin() + 20);
  resources.clear();
  Check(!ReadResources(truncated, &resources), "a cut-short file is refused");
}

}  // namespace

int main() {
  try {
    RunChecks();
  } catch (...) {
    return 1;
  }
  std::printf("OK: %d checks passed\n", g_checks);
  return 0;
}
