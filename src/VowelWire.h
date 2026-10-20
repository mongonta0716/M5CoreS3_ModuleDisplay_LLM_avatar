#pragma once

#include "VowelModel.h"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Bounded, newline-framed UTF-8 text with tab-separated fields and CRC32.
// Shared verbatim by Linux and ESP32; no native struct layout on the wire.
namespace vowelWire {
constexpr size_t kMaxLine = 384;
constexpr uint32_t kTimeoutMs = 500;
enum class Command { Resume, Pause, Calibrate, Cancel, Reset, Status };
inline const char* name(Command c) {
  static const char* names[] = {"resume", "pause", "calibrate", "cancel", "reset", "status"};
  return names[unsigned(c)];
}
struct Control { uint32_t client = 0, epoch = 0; Command command = Command::Status; };
struct Frame {
  uint32_t server = 0, client = 0, epoch = 0, sequence = 0;
  std::string text = "無音";
  float open = 0, confidence = 0;
  unsigned status = 0, step = 0, round = 1, rest = 0, seconds = 0;
};
inline uint32_t crc32(const char* data, size_t size) {
  uint32_t crc = ~uint32_t(0);
  for (size_t i = 0; i < size; ++i) {
    crc ^= uint8_t(data[i]);
    for (unsigned b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
inline std::string finish(const std::string& body) {
  char checksum[16];
  std::snprintf(checksum, sizeof(checksum), "\t%08x\n", unsigned(crc32(body.data(), body.size())));
  return body + checksum;
}
inline bool fields(const std::string& line, std::array<std::string, 15>& out, size_t wanted) {
  if (line.empty() || line.size() >= kMaxLine) return false;
  const auto end = line.rfind('\t');
  if (end == std::string::npos || line.size() - end != 9) return false;
  uint32_t checksum = 0;
  for (size_t i = end + 1; i < line.size(); ++i) {
    const char c = line[i];
    const unsigned v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 16;
    if (v > 15) return false;
    checksum = (checksum << 4) | v;
  }
  if (checksum != crc32(line.data(), end)) return false;
  size_t start = 0, count = 0;
  for (size_t i = 0; i <= end; ++i) {
    if (i == end || line[i] == '\t') {
      if (count >= wanted || i == start) return false;
      out[count++] = line.substr(start, i - start); start = i + 1;
    } else if (uint8_t(line[i]) < 0x20) return false;
  }
  return count == wanted;
}
inline bool integer(const std::string& s, uint32_t& value) {
  if (s.empty() || s.size() > 10) return false;
  uint64_t n = 0;
  for (const auto c : s) {
    if (c < '0' || c > '9') return false;
    n = n * 10 + unsigned(c - '0');
    if (n > UINT32_MAX) return false;
  }
  value = uint32_t(n); return true;
}
inline bool unit(const std::string& s, float& value) {
  if (s.empty() || s.size() > 16) return false;
  for (const auto c : s) if (!((c >= '0' && c <= '9') || c == '.')) return false;
  char* end = nullptr; errno = 0;
  value = std::strtof(s.c_str(), &end);
  return !errno && end == s.c_str() + s.size() && std::isfinite(value) && value >= 0 && value <= 1;
}
inline Vowel vowel(const std::string& s) {
  for (unsigned v = 1; v <= 5; ++v) if (s == vowelKana(Vowel(v))) return Vowel(v);
  return Vowel::Unknown;
}
inline std::string encode(const Control& c) {
  return finish("C1\t" + std::to_string(c.client) + "\t" + std::to_string(c.epoch) + "\t" + name(c.command));
}
inline bool decode(const std::string& line, Control& c) {
  std::array<std::string, 15> f;
  if (!fields(line, f, 4) || f[0] != "C1" || !integer(f[1], c.client) ||
      !c.client || !integer(f[2], c.epoch) || !c.epoch) return false;
  for (unsigned i = 0; i <= unsigned(Command::Status); ++i)
    if (f[3] == name(Command(i))) { c.command = Command(i); return true; }
  return false;
}
inline std::string encode(const Frame& f) {
  char buffer[320];
  std::snprintf(buffer, sizeof(buffer), "V1\t%u\t%u\t%u\t%u\t%s\t%.4f\t%.4f\t%u\t%u\t%u\t%u\t%u",
                unsigned(f.server), unsigned(f.client), unsigned(f.epoch), unsigned(f.sequence), f.text.c_str(),
                double(f.open), double(f.confidence), f.status, f.step, f.round, f.rest, f.seconds);
  return finish(buffer);
}
inline bool decode(const std::string& line, Frame& result) {
  std::array<std::string, 15> f;
  Frame r;
  uint32_t status, step, round, rest, seconds;
  if (!fields(line, f, 13) || f[0] != "V1" || !integer(f[1], r.server) || !r.server ||
      !integer(f[2], r.client) || !r.client || !integer(f[3], r.epoch) || !r.epoch ||
      !integer(f[4], r.sequence) || !unit(f[6], r.open) || !unit(f[7], r.confidence) ||
      !integer(f[8], status) || status > 7 || !integer(f[9], step) || step > 6 ||
      !integer(f[10], round) || round < 1 || round > 2 || !integer(f[11], rest) || rest > 1 ||
      !integer(f[12], seconds) || seconds > 6) return false;
  if (vowel(f[5]) == Vowel::Unknown && f[5] != "無音" && f[5] != "不明") return false;
  if (f[5] == "無音" && (r.open != 0 || r.confidence != 0)) return false;
  if (status == 1 && step > 5) return false;
  r.text = f[5]; r.status = status; r.step = step; r.round = round; r.rest = rest; r.seconds = seconds;
  result = r; return true;
}
class Lines {
 public:
  bool push(char c, std::string& completed) {
    if (c == '\n') {
      const bool valid = !dropping_ && !line_.empty();
      if (valid) completed = line_;
      line_.clear(); dropping_ = false; return valid;
    }
    if (dropping_) return false;
    if (line_.size() >= kMaxLine - 1 || c == '\0') { line_.clear(); dropping_ = true; }
    else line_ += c;
    return false;
  }
  void reset() { line_.clear(); dropping_ = false; }
 private:
  std::string line_;
  bool dropping_ = false;
};
// State acceptance is independent of Arduino, so stale/mute handling is tested
// with the exact code used by the CoreS3 receiver.
class Receiver {
 public:
  void expect(uint32_t client, uint32_t epoch) {
    client_ = client; epoch_ = epoch; valid_ = false;
  }
  bool accept(const Frame& f, uint32_t now) {
    if (f.client != client_ || f.epoch != epoch_) return false;
    if (valid_ && (f.server != frame_.server || int32_t(f.sequence - frame_.sequence) <= 0)) return false;
    frame_ = f; arrived_ = now; valid_ = true; return true;
  }
  bool fresh(uint32_t now) const { return valid_ && now - arrived_ < kTimeoutMs; }
  const Frame& frame() const { return frame_; }
 private:
  Frame frame_;
  uint32_t client_ = 0, epoch_ = 0, arrived_ = 0;
  bool valid_ = false;
};
}  // namespace vowelWire
