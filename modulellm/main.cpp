#include "ModelStore.h"
#include "VowelEngine.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <dirent.h>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <termios.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

namespace {
volatile sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
uint64_t micros() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
uint32_t millis() { return uint32_t(micros() / 1000); }
struct Options {
  std::string uart = "/dev/ttyS1", capture = "hw:0,0", input, channel = "right", profile;
  std::string model = "/var/lib/modulellm-vowel/model.txt";
  unsigned seconds = 0;
  bool pattern = false;
};
Options options(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    if (key == "--test-pattern") { o.pattern = true; continue; }
    if (i + 1 == argc) throw std::runtime_error("missing option value");
    const std::string value = argv[++i];
    if (key == "--uart") o.uart = value;
    else if (key == "--capture") o.capture = value;
    else if (key == "--input") o.input = value;
    else if (key == "--channel") o.channel = value;
    else if (key == "--profile") o.profile = value;
    else if (key == "--model") o.model = value;
    else if (key == "--seconds") {
      uint32_t n; if (!vowelWire::integer(value, n)) throw std::runtime_error("bad duration"); o.seconds = n;
    } else throw std::runtime_error("unknown option: " + key);
  }
  if (o.channel != "left" && o.channel != "right" && o.channel != "mix") throw std::runtime_error("invalid channel");
  if (o.profile.empty() || o.profile.find_first_of(" \t\r\n") != std::string::npos)
    throw std::runtime_error("--profile is required; include device, PCM format, channel and mixer/gain fingerprint");
  return o;
}
void nonblock(int fd) {
  const int flags = fcntl(fd, F_GETFL);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) throw std::runtime_error("nonblock failed");
}
// Refuse a UART already open by another application (TIOCEXCL alone does not
// detect owners that opened it before us). No services are stopped implicitly.
void checkUartOwner(const std::string& device) {
#ifdef __linux__
  DIR* proc = opendir("/proc");
  if (!proc) throw std::runtime_error("cannot inspect UART owners");
  bool busy = false;
  while (auto* entry = readdir(proc)) {
    uint32_t pid;
    if (!vowelWire::integer(entry->d_name, pid) || pid == unsigned(getpid())) continue;
    const std::string folder = std::string("/proc/") + entry->d_name + "/fd";
    DIR* fds = opendir(folder.c_str()); if (!fds) continue;
    while (auto* file = readdir(fds)) {
      char path[1024]; const auto n = readlink((folder + "/" + file->d_name).c_str(), path, sizeof(path) - 1);
      if (n < 0) continue;
      path[n] = 0; if (device == path) busy = true;
    }
    closedir(fds);
  }
  closedir(proc);
  if (busy) throw std::runtime_error("UART already owned: " + device);
#else
  (void)device;
#endif
}
class Uart {
 public:
  explicit Uart(const std::string& path) {
    if (path == "-") { fd = STDOUT_FILENO; nonblock(fd); return; }
    checkUartOwner(path);
    fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) throw std::runtime_error("cannot open UART: " + path);
    if (flock(fd, LOCK_EX | LOCK_NB) != 0 || ioctl(fd, TIOCEXCL) != 0)
      throw std::runtime_error("cannot exclusively own UART");
    termios t{};
    if (tcgetattr(fd, &t) != 0) throw std::runtime_error("tcgetattr failed");
    cfmakeraw(&t); cfsetispeed(&t, B115200); cfsetospeed(&t, B115200);
    t.c_cflag |= CLOCAL | CREAD; t.c_cflag &= ~(CSTOPB | PARENB | CRTSCTS);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &t) != 0) throw std::runtime_error("tcsetattr failed");
    tcflush(fd, TCIOFLUSH);
  }
  ~Uart() { if (fd != STDOUT_FILENO && fd >= 0) { ioctl(fd, TIOCNXCL); close(fd); } }
  int fd = -1;
};
class Audio {
 public:
  explicit Audio(const Options& o) {
    if (o.pattern) return;
    if (!o.input.empty()) {
      fd = open(o.input.c_str(), O_RDONLY); if (fd < 0) throw std::runtime_error("PCM file open failed");
      return;
    }
    int pipefd[2]; if (pipe(pipefd) != 0) throw std::runtime_error("audio pipe failed");
    child = fork();
    if (child == 0) {
#ifdef __linux__
      prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
      dup2(pipefd[1], STDOUT_FILENO); close(pipefd[0]); close(pipefd[1]);
      execlp("arecord", "arecord", "-q", "-D", o.capture.c_str(), "-t", "raw", "-f", "S16_LE",
             "-r", "16000", "-c", "2", "--period-size=256", "--buffer-size=1024", static_cast<char*>(nullptr));
      _exit(127);
    }
    close(pipefd[1]);
    if (child < 0) { close(pipefd[0]); throw std::runtime_error("fork failed"); }
    fd = pipefd[0]; nonblock(fd);
  }
  ~Audio() {
    if (fd >= 0) close(fd);
    if (child > 0) { kill(child, SIGTERM); while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {} }
  }
  int fd = -1;
 private:
  pid_t child = -1;
};
// At most one partial line and one replaceable newest result; no result FIFO.
class Outbox {
 public:
  void queue(std::string line) {
    if (current_.empty()) { current_ = std::move(line); started_ = millis(); }
    else { if (!next_.empty()) ++dropped; next_ = std::move(line); }
  }
  void flush(int fd) {
    if (current_.empty()) return;
    if (millis() - started_ > 250) {
      // Resynchronize the newline parser after an abandoned partial line.
      current_ = "\n"; next_.clear(); offset_ = 0; started_ = millis(); ++dropped;
      if (isatty(fd)) tcflush(fd, TCOFLUSH);
    }
    const auto n = write(fd, current_.data() + offset_, current_.size() - offset_);
    if (n > 0) { offset_ += size_t(n); bytes += size_t(n); }
    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
      throw std::runtime_error("UART write failed");
    if (offset_ == current_.size()) {
      current_ = std::move(next_); next_.clear(); offset_ = 0; started_ = millis();
    }
  }
  bool pending() const { return !current_.empty(); }
  uint64_t dropped = 0, bytes = 0;
 private:
  std::string current_, next_;
  size_t offset_ = 0;
  uint32_t started_ = 0;
};
}  // namespace

int main(int argc, char** argv) try {
  const auto o = options(argc, argv);
  std::signal(SIGINT, stop); std::signal(SIGTERM, stop); std::signal(SIGPIPE, SIG_IGN);
  Uart uart(o.uart);
  // Created after the UART: forked arecord must not retain the UART descriptor.
  if (uart.fd != STDOUT_FILENO) fcntl(uart.fd, F_SETFD, FD_CLOEXEC);
  Audio audio(o);
  ModelStore store(o.model, o.profile);
  VowelEngine engine; VowelModel model;
  const bool loaded = store.load(model); engine.setModel(loaded ? model : VowelModel{});
  std::cerr << "model=" << (loaded ? "loaded" : "initial; B calibration required")
            << " profile=" << o.profile << " channel=" << o.channel << '\n';
  const uint32_t boot = uint32_t(micros()) ^ unsigned(getpid());
  uint32_t client = o.uart == "-" ? 1 : 0, epoch = o.uart == "-" ? 1 : 0, sequence = 0;
  bool muted = o.uart != "-";
  vowelWire::Command lastCommand = vowelWire::Command::Pause;
  vowelWire::Lines lines; Outbox out;
  const auto started = millis(); auto lastStats = started, lastPattern = started;
  std::array<uint8_t, 1024> bytes{}; std::array<int16_t, 512> stereo{};
  size_t filled = 0; uint64_t frames = 0, analysisUs = 0, maxUs = 0, lostAudio = 0;
  bool eof = false;
  while (!stopping && !eof && (!o.seconds || millis() - started < uint64_t(o.seconds) * 1000)) {
    pollfd fds[] = {{audio.fd, POLLIN, 0}, {o.uart == "-" ? -1 : uart.fd, POLLIN, 0}};
    if (poll(fds, 2, o.pattern ? 8 : 20) < 0 && errno != EINTR) throw std::runtime_error("poll failed");
    if (fds[1].revents & POLLIN) {
      char buffer[512]; const auto n = read(uart.fd, buffer, sizeof(buffer));
      for (ssize_t i = 0; i < n; ++i) {
        std::string line; vowelWire::Control c;
        if (!lines.push(buffer[i], line) || !vowelWire::decode(line, c)) continue;
        if (c.client == client && int32_t(c.epoch - epoch) <= 0) continue; // duplicates are idempotent.
        if (c.client != client) { engine.cancel(millis()); engine.resetAnalysis(millis()); }
        client = c.client; epoch = c.epoch; lastCommand = c.command;
        switch (c.command) {
          case vowelWire::Command::Resume: muted = false; engine.resetAnalysis(millis()); break;
          case vowelWire::Command::Pause: muted = true; engine.cancel(millis()); engine.resetAnalysis(millis()); break;
          case vowelWire::Command::Calibrate: muted = false; engine.startCalibration(millis()); break;
          case vowelWire::Command::Cancel: engine.cancel(millis()); break;
          case vowelWire::Command::Reset: engine.resetModel(store.reset(), millis()); break;
          case vowelWire::Command::Status: break;
        }
        std::cerr << "control=" << vowelWire::name(lastCommand) << " client=" << client << " epoch=" << epoch << '\n';
      }
    }
    bool ready = false;
    uint32_t now = millis();
    if (o.pattern) {
      if (now - lastPattern >= 32) { ready = true; lastPattern = now; ++frames; }
    } else if (fds[0].revents & (POLLIN | POLLHUP)) {
      int available = 0;
      if (o.input.empty() && ioctl(audio.fd, FIONREAD, &available) == 0 && available > 4096) {
        uint8_t discard[4096]; ssize_t n;
        while ((n = read(audio.fd, discard, sizeof(discard))) > 0) lostAudio += size_t(n);
        filled = 0; engine.cancel(now); engine.resetAnalysis(now);
      }
      const auto n = read(audio.fd, bytes.data() + filled, bytes.size() - filled);
      if (n > 0) filled += size_t(n);
      else if (n == 0) {
        if (o.input.empty()) throw std::runtime_error("microphone capture exited");
        if (filled % 4) throw std::runtime_error("incomplete stereo PCM sample");
        eof = true; continue;
      }
      else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) throw std::runtime_error("audio read failed");
      if (filled == bytes.size()) {
        for (size_t i = 0; i < 256; ++i) {
          const auto left = int16_t(uint16_t(bytes[i * 4]) | (uint16_t(bytes[i * 4 + 1]) << 8));
          const auto right = int16_t(uint16_t(bytes[i * 4 + 2]) | (uint16_t(bytes[i * 4 + 3]) << 8));
          const auto value = o.channel == "left" ? left : o.channel == "right" ? right : int16_t((int(left) + right) / 2);
          stereo[i * 2] = stereo[i * 2 + 1] = value;
        }
        filled = 0;
        if (!o.input.empty()) now = uint32_t(frames * 16);
        const auto began = micros(); ready = engine.process(stereo.data(), now);
        const auto took = micros() - began; analysisUs += took; maxUs = std::max(maxUs, took); ++frames;
        if (engine.needsSave()) {
          const bool ok = store.save(engine.pendingModel()); engine.saved(ok, now);
          std::cerr << "calibration_save=" << (ok ? "OK" : "FAIL") << '\n';
        }
      }
    }
    if (ready && client) {
      auto f = engine.frame(now, muted);
      if (o.pattern && !muted) {
        const unsigned v = ((millis() - started) / 700) % 7;
        f.text = v < 5 ? vowelKana(Vowel(v + 1)) : v == 5 ? "無音" : "不明";
        f.open = v == 5 ? 0 : 0.6f; f.confidence = v < 5 ? 0.9f : 0;
      }
      f.server = boot ? boot : 1; f.client = client; f.epoch = epoch; f.sequence = ++sequence;
      out.queue(vowelWire::encode(f));
    }
    out.flush(uart.fd);
    if (millis() - lastStats >= 5000) {
      rusage usage{}; getrusage(RUSAGE_SELF, &usage);
      const auto& f = engine.features();
      std::cerr << "frames=" << frames << " mean_us=" << (frames ? analysisUs / frames : 0)
                << " max_us=" << maxUs << " tx_bytes=" << out.bytes << " tx_replaced=" << out.dropped
                << " audio_drop_bytes=" << lostAudio << " rss=" << usage.ru_maxrss
                << " cpu_s=" << (double(usage.ru_utime.tv_sec) + usage.ru_utime.tv_usec / 1000000.0)
                << " rms=" << f.rms << " F1=" << f.f1 << " F2=" << f.f2 << " confidence=" << f.confidence << '\n';
      lastStats = millis();
    }
  }
  const auto deadline = millis() + 250;
  while (out.pending() && int32_t(deadline - millis()) > 0) { out.flush(uart.fd); usleep(1000); }
  std::cerr << "stopped frames=" << frames << " tx_bytes=" << out.bytes << " audio_drop_bytes=" << lostAudio << '\n';
  return 0;
} catch (const std::exception& error) {
  std::cerr << "modulellm-vowel: " << error.what() << '\n'; return 1;
}
