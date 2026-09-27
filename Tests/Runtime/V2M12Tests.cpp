#include "Nexora/Network/EntityMapping.h"
#include "Nexora/Runtime/HardeningV2.h"
#include "Nexora/Runtime/Shipping.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int RunTests() {
  using namespace nexora::runtime::hardening_v2;
  using namespace nexora::runtime::shipping;

  const auto projects = ReferenceProjects();
  Require(projects.size() == 5 &&
              projects[0].kind == ReferenceProjectKind::MassiveOutdoor &&
              projects[1].kind == ReferenceProjectKind::IndoorPortalDungeon &&
              projects[2].kind == ReferenceProjectKind::NetworkArena &&
              projects[3].kind == ReferenceProjectKind::CrowdCity &&
              projects[4].kind == ReferenceProjectKind::MobileStress,
          "V2 reference project catalog is incomplete or reordered");
  Require(HasCapability(projects[0].capabilities, ReferenceCapability::Streaming) &&
              HasCapability(projects[1].capabilities, ReferenceCapability::PortalAudio) &&
              HasCapability(projects[2].capabilities, ReferenceCapability::Network) &&
              HasCapability(projects[3].capabilities, ReferenceCapability::Crowd) &&
              HasCapability(projects[4].capabilities, ReferenceCapability::MobileThermal),
          "reference projects lost their hardening capability coverage");

  SoakMonitor streaming{32, 4};
  for (std::uint64_t frame = 1; frame <= 100000; ++frame)
    Require(streaming.Sample({frame, 4096 + frame % 16, 500 + frame % 3}),
            "portable streaming soak probe rejected ordered evidence");
  Require(!streaming.Result().suspected_leak && streaming.Result().samples == 100000,
          "bounded portable streaming soak probe reported unbounded growth");

  nexora::network::ServerEntityMap server_map;
  nexora::network::ClientEntityMap client_map;
  for (std::uint64_t reconnect = 1; reconnect <= 2000; ++reconnect) {
    std::array<nexora::network::NetworkEntityID, 16> identities{};
    for (std::size_t index = 0; index < identities.size(); ++index) {
      const auto network = server_map.Spawn(reconnect * 1000 + index);
      Require(network && client_map.Spawn(*network, reconnect * 1000 + index),
              "network mapping could not be established");
      identities[index] = *network;
    }
    Require(server_map.Size() == identities.size() && client_map.Size() == identities.size(),
            "network mapping count changed before disconnect");
    server_map.ResetSession();
    client_map.ResetSession();
    Require(server_map.Size() == 0 && client_map.Size() == 0,
            "network mapping leaked after disconnect/reconnect reset");
    for (const auto identity : identities)
      Require(!server_map.Contains(identity), "stale network identity survived session reset");
  }

  BundleUpdater updater({7, "known-good"});
  Require(updater.Stage({8, "candidate"}) && updater.Activate() &&
              updater.Current().generation == 8 && updater.Rollback() &&
              updater.Current().generation == 7 && updater.Current().digest == "known-good",
          "patch rollback did not restore the known-good generation");

  const std::array payload{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40}};
  auto save = MakeSaveImage(4, payload);
  Require(VerifySaveImage(save), "valid save image failed integrity validation");
  save.payload[2] ^= std::byte{0x01};
  Require(!VerifySaveImage(save), "corrupted save image was accepted");

  const ThermalPolicy thermal{70000};
  const std::array thermal_evidence{
      ThermalEvidence{1, 55000, false},
      ThermalEvidence{2, 69999, false},
      ThermalEvidence{3, 70000, true},
      ThermalEvidence{4, 76000, true},
  };
  Require(ValidateThermalEvidence(thermal, thermal_evidence),
          "valid thermal throttle evidence was rejected");
  auto invalid_thermal = thermal_evidence;
  invalid_thermal.back().throttled = false;
  Require(!ValidateThermalEvidence(thermal, invalid_thermal),
          "over-threshold unthrottled thermal evidence was accepted");

  Require(FootprintWithinBudget(10 * 1024 * 1024, 10 * 1024 * 1024 + 64 * 1024, 64 * 1024) &&
              !FootprintWithinBudget(10 * 1024 * 1024, 11 * 1024 * 1024, 64 * 1024),
          "V1-like footprint growth budget did not enforce the configured tolerance");

  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &) {
    return 1;
  }
}
