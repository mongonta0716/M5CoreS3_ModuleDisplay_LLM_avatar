#include "VowelWire.h"
#include "ModelStore.h"
#include "VowelEngine.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <unistd.h>

int main() {
  using namespace vowelWire;
  Frame original;
  original.server = 123; original.client = 456; original.epoch = 2; original.sequence = 100;
  original.text = "あ"; original.open = .55f; original.confidence = .9f;
  const auto encoded = encode(original);
  Lines lines;
  std::string line; unsigned completed = 0;
  for (const auto c : encoded + encoded) {
    if (lines.push(c, line)) {
      Frame f; assert(decode(line, f)); ++completed;
      assert(f.text == "あ" && f.open == .55f && f.confidence == .9f && f.sequence == 100);
    }
  }
  assert(completed == 2);  // Every byte, including each UTF-8 byte, split apart.
  Frame f;
  auto broken = encoded; broken[10] ^= 1;
  assert(!decode(broken.substr(0, broken.size() - 1), f));
  for (const auto& body : {
       "V2\t1\t2\t3\t4\tあ\t0.5\t0.9\t0\t0\t1\t0\t0",
       "V1\t1\t2\t3\t4\tあ\tnan\t0.9\t0\t0\t1\t0\t0",
       "V1\t1\t2\t3\t4\tあ\t1.1\t0.9\t0\t0\t1\t0\t0",
       "V1\t1\t2\t3\t4\tか\t0.5\t0.9\t0\t0\t1\t0\t0",
       "V1\t1\t2\t3\t4\t無音\t0.5\t0\t0\t0\t1\t0\t0",
       "V1\t1\t2\t3\t4\tあ\t0.5\t0.9\t1\t6\t1\t0\t0",
       "V1\t4294967296\t2\t3\t4\tあ\t0.5\t0.9\t0\t0\t1\t0\t0"}) {
    auto message = finish(body); message.pop_back(); assert(!decode(message, f));
  }
  for (unsigned i = 0; i < 1000; ++i) assert(!lines.push('x', line));
  assert(!lines.push('\n', line));
  for (const auto c : encoded) if (lines.push(c, line)) assert(decode(line, f));
  Control command; command.client = 17; command.epoch = 9; command.command = Command::Calibrate;
  auto controlLine = encode(command); controlLine.pop_back(); Control parsed;
  assert(decode(controlLine, parsed) && parsed.command == Command::Calibrate);
  Receiver receiver;
  receiver.expect(456, 2);
  const uint32_t t = UINT32_MAX - 200;
  assert(receiver.accept(original, t));
  assert(!receiver.accept(original, t + 1)); // replay/duplicate.
  original.sequence++; original.client++;
  assert(!receiver.accept(original, t + 1)); // other CoreS3 boot.
  original.client--; original.epoch++;
  assert(!receiver.accept(original, t + 1)); // wrong control epoch.
  assert(receiver.fresh(t + 499) && !receiver.fresh(t + 500)); // including rollover.
  receiver.expect(456, 3); // Pause/resume invalidates old queued results immediately.
  assert(!receiver.fresh(t + 5));
  original.epoch = 2; assert(!receiver.accept(original, t + 6));
  original.epoch = 3; assert(receiver.accept(original, t + 7));
  original.server++; original.sequence++;
  assert(!receiver.accept(original, t + 8)); // server reboot needs a fresh handshake.

  char folder[] = "/tmp/vowel-store-XXXXXX"; assert(mkdtemp(folder));
  const std::string path = std::string(folder) + "/model.txt";
  ModelStore store(path, "mic-right-gain-profile-A");
  VowelModel model, loaded;
  assert(!store.load(loaded)); assert(store.save(model)); assert(store.load(loaded));
  assert(loaded.f1[0] == model.f1[0] && loaded.noiseRms == model.noiseRms);
  assert(!ModelStore(path, "mic-right-gain-profile-B").load(loaded));
  auto invalid = model; invalid.f1[0] = NAN;
  assert(!store.save(invalid) && store.load(loaded)); // failed replacement preserves old model.
  { std::ofstream file(path); file << "VOWELROOTS 1 mic-right-gain-profile-A\n35\n800"; }
  assert(!store.load(loaded));
  assert(store.reset()); assert(store.reset()); rmdir(folder);

  VowelEngine engine;
  std::array<int16_t, 512> quiet{};
  engine.startCalibration(0);
  for (uint32_t now = 0; now <= 2200; now += 16) engine.process(quiet.data(), now);
  auto state = engine.frame(2200, false);
  assert(state.text == "無音" && state.open == 0);
  assert(state.status == unsigned(VowelCalibration::Status::Running) && state.step == 1);
  engine.cancel(2210);
  assert(engine.frame(2210, false).status == unsigned(VowelCalibration::Status::Cancelled));
  engine.resetModel(false, 2300);
  assert(engine.frame(2300, false).status == unsigned(VowelCalibration::Status::SaveFailed));
  std::cout << "Wire framing, CRC, sessions, stale results, model persistence and remote engine tests passed\n";
}
