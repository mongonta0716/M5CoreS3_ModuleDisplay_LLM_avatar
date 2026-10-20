#pragma once

#include "VowelModel.h"
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

class ModelStore {
 public:
  ModelStore(std::string path, std::string profile) : path_(path), profile_(profile) {}
  bool load(VowelModel& model) const {
    std::ifstream in(path_); in.imbue(std::locale::classic());
    std::string magic, profile, extra; unsigned version = 0;
    VowelModel m;
    if (!(in >> magic >> version >> profile >> m.noiseRms) || magic != "VOWELROOTS" ||
        version != 1 || profile != profile_) return false;
    for (unsigned i = 0; i < 5; ++i) if (!(in >> m.f1[i] >> m.f2[i])) return false;
    if (in >> extra || !m.valid()) return false;
    model = m; return true;
  }
  bool save(const VowelModel& m) const {
    if (!m.valid() || profile_.empty() || profile_.find_first_of(" \t\r\n") != std::string::npos) return false;
    std::ostringstream out; out.imbue(std::locale::classic());
    out << "VOWELROOTS 1 " << profile_ << '\n' << std::setprecision(9) << m.noiseRms << '\n';
    for (unsigned i = 0; i < 5; ++i) out << m.f1[i] << ' ' << m.f2[i] << '\n';
    const auto body = out.str(), temp = path_ + ".tmp." + std::to_string(getpid());
    int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return false;
    size_t used = 0;
    while (used < body.size()) {
      const auto n = ::write(fd, body.data() + used, body.size() - used);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0) break;
      used += size_t(n);
    }
    bool ok = used == body.size() && fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(temp.c_str(), path_.c_str()) == 0;
    if (!ok) unlink(temp.c_str());
    if (ok) ok = syncDirectory();
    return ok;
  }
  bool reset() const {
    if (unlink(path_.c_str()) != 0 && errno != ENOENT) return false;
    return syncDirectory();
  }
 private:
  std::string path_, profile_;
  bool syncDirectory() const {
    const auto slash = path_.find_last_of('/');
    const auto dir = slash == std::string::npos ? "." : slash == 0 ? "/" : path_.substr(0, slash);
    const int fd = ::open(dir.c_str(), O_RDONLY);
    if (fd < 0) return false;
    const bool ok = fsync(fd) == 0; close(fd); return ok;
  }
};
