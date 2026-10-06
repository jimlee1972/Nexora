#include "Nexora/Math/Math.h"
#include "ShowcaseRooms.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using nexora::showcase::RoomSession;
using Nexora::Window::Key;
using Nexora::Window::WindowEvent;
using Nexora::Window::WindowEventType;
static void Press(RoomSession &session, Key key) {
  WindowEvent event{};
  event.type = WindowEventType::Key;
  event.value0 = static_cast<int>(key);
  event.value1 = 1;
  session.Event(event, 1280, 720);
  event.value1 = 0;
  session.Event(event, 1280, 720);
}
int main() {
#ifdef NEXORA_SHOWCASE_TEST_PLUGIN
  RoomSession session("hub", false, false, NEXORA_SHOWCASE_TEST_PLUGIN);
#else
  RoomSession session("hub");
#endif
  session.Tick(1.0 / 60);
  assert(session.Healthy());
  assert(session.Probes().size() == 13);
#if NEXORA_ASSET_PIPELINE_ENABLED
  for (const auto error : {nexora::showcase::ErrorInjection::InvalidAsset,
                           nexora::showcase::ErrorInjection::DependencyCycle,
                           nexora::showcase::ErrorInjection::Rollback}) {
    session.RerunProbe(5, error);
    assert(session.Probes()[5].status == nexora::showcase::ProbeStatus::Pass);
    assert(session.Probes()[5].issues.empty());
  }
#endif
#if NEXORA_EDITOR_SDK_ENABLED && defined(NEXORA_SHOWCASE_TEST_PLUGIN)
  session.RerunProbe(6, nexora::showcase::ErrorInjection::PluginAbiMismatch);
  assert(session.Probes()[6].status == nexora::showcase::ProbeStatus::Pass);
  assert(session.Report().find("Plugin ABI mismatch rejected before registration") !=
         std::string::npos);
  RoomSession missingPlugin("scene", false, false, "missing-lab-plugin.so");
  missingPlugin.Tick(0.01);
  missingPlugin.RerunProbe(6, nexora::showcase::ErrorInjection::PluginAbiMismatch);
  assert(!missingPlugin.Healthy());
  assert(missingPlugin.Probes()[6].status == nexora::showcase::ProbeStatus::Fail);
#endif
#if NEXORA_SHIPPING_ENABLED
  session.RerunProbe(12, nexora::showcase::ErrorInjection::Rollback);
  assert(session.Probes()[12].status == nexora::showcase::ProbeStatus::Pass);
#endif
  // Courtyard cameras replay exactly after orbit input; diagnostic-free output owns no UI.
  RoomSession courtyard("courtyard");
  courtyard.Tick(0.01);
  const auto wide = courtyard.Scene(1280, 720);
  assert(wide.vertices.size() > 2000 && wide.indices.size() > 3000);
  for (const auto index : wide.indices)
    assert(index < wide.vertices.size());
  assert(wide.pbr);
  assert(wide.postProcessAntiAliasing);
  Press(courtyard, Key::F10);
  assert(!courtyard.Scene(1280, 720).postProcessAntiAliasing);
  Press(courtyard, Key::F10);
  assert(courtyard.Scene(1280, 720).postProcessAntiAliasing);
  assert(wide.materials.size() == 44 && !wide.batches.empty());
  assert(Nexora::Presentation::ValidateSceneMaterials(wide.materials, wide.batches));
  const auto sourceMaterialCount = wide.materials.size() / 2;
  std::size_t covered = 0;
  std::vector<bool> selectedMaterials(wide.materials.size());
  for (const auto &batch : wide.batches) {
    assert(batch.firstInstance + batch.instanceCount <= wide.instances.size());
    if (batch.firstInstance == 1)
      assert(batch.instanceCount == 4);
    else if (batch.firstInstance >= 5 && batch.firstInstance < 437 && batch.materialIndex == 0)
      assert(batch.instanceCount > 1);
    else if (batch.firstInstance >= 5 && batch.firstInstance < 437 && batch.materialIndex == 21)
      assert(batch.instanceCount >= 1); // A wet subset may contain one original tile.
    else if (batch.firstInstance >= 437 && batch.materialIndex < sourceMaterialCount)
      assert(batch.instanceCount >= 1 && batch.indexCount == 132);
    else
      assert(batch.instanceCount == 1);
    if (batch.materialIndex < sourceMaterialCount) {
      assert(batch.firstIndex == covered && batch.materialIndex < sourceMaterialCount);
      covered += batch.indexCount;
    } else {
      assert(batch.firstInstance == wide.instances.size() - 1 &&
             batch.materialIndex >= sourceMaterialCount);
      assert(wide.materials[batch.materialIndex].reflectionRole ==
             Nexora::Presentation::SceneReflectionRole::ReflectedGeometry);
      assert(!wide.materials[batch.materialIndex].castsShadow);
      const auto source =
          std::find_if(wide.batches.begin(), wide.batches.end(), [&](const auto &candidate) {
            return candidate.firstInstance == 0 && candidate.firstIndex == batch.firstIndex &&
                   candidate.indexCount == batch.indexCount &&
                   candidate.materialIndex + sourceMaterialCount == batch.materialIndex;
          });
      assert(source != wide.batches.end());
    }
    assert(batch.materialIndex < selectedMaterials.size());
    selectedMaterials[batch.materialIndex] = true;
  }
  // The annular device bend reverses orientation. Its actual triangles must
  // still wind outward, agreeing with the transformed shading normals.
  unsigned annularWedges = 0;
  for (const auto &batch : wide.batches)
    if (batch.firstInstance == 0 && batch.materialIndex == 0 && batch.indexCount == 132) {
      ++annularWedges;
      for (std::size_t triangle = batch.firstIndex; triangle < batch.firstIndex + batch.indexCount;
           triangle += 3) {
        const auto &a = wide.vertices[wide.indices[triangle]];
        const auto &b = wide.vertices[wide.indices[triangle + 1]];
        const auto &c = wide.vertices[wide.indices[triangle + 2]];
        const nexora::math::Vector3 ab{b.position[0] - a.position[0], b.position[1] - a.position[1],
                                       b.position[2] - a.position[2]};
        const nexora::math::Vector3 ac{c.position[0] - a.position[0], c.position[1] - a.position[1],
                                       c.position[2] - a.position[2]};
        const nexora::math::Vector3 normal{a.normal[0], a.normal[1], a.normal[2]};
        assert(nexora::math::Dot(nexora::math::Cross(ab, ac), normal) > 1e-7F);
      }
    }
  assert(annularWedges > 0);
  // All rigid authored stone/ceramic/paint faces agree with their outward
  // normals, including shared lathe rows, sharp vessel profiles and flutes.
  std::size_t rigidTriangles = 0;
  for (const auto &batch : wide.batches) {
    if (batch.materialIndex != 0 && batch.materialIndex != 3 && batch.materialIndex != 17)
      continue;
    for (std::size_t triangle = batch.firstIndex; triangle < batch.firstIndex + batch.indexCount;
         triangle += 3) {
      const auto &a = wide.vertices[wide.indices[triangle]];
      const auto &b = wide.vertices[wide.indices[triangle + 1]];
      const auto &c = wide.vertices[wide.indices[triangle + 2]];
      const nexora::math::Vector3 ab{b.position[0] - a.position[0], b.position[1] - a.position[1],
                                     b.position[2] - a.position[2]};
      const nexora::math::Vector3 ac{c.position[0] - a.position[0], c.position[1] - a.position[1],
                                     c.position[2] - a.position[2]};
      const auto face = nexora::math::Cross(ab, ac);
      for (const auto *vertex : {&a, &b, &c})
        assert(nexora::math::Dot(face, nexora::math::Vector3{vertex->normal[0], vertex->normal[1],
                                                             vertex->normal[2]}) > 1e-7F);
      ++rigidTriangles;
    }
  }
  assert(rigidTriangles > 0);

  std::size_t sourceLeaves = 0;
  for (const auto &batch : wide.batches)
    if (batch.materialIndex == 5)
      sourceLeaves += batch.indexCount / 6;
  assert(sourceLeaves > 700);
  std::size_t foldedLeaves = 0;
  for (const auto &batch : wide.batches) {
    if (batch.materialIndex != 5 || batch.firstInstance != 0)
      continue;
    for (std::size_t i = batch.firstIndex; i < batch.firstIndex + batch.indexCount; i += 6) {
      const auto &a = wide.vertices[wide.indices[i]];
      const auto &b = wide.vertices[wide.indices[i + 1]];
      const auto &c = wide.vertices[wide.indices[i + 2]];
      const auto &d = wide.vertices[wide.indices[i + 5]];
      const auto point = [](const auto &v) {
        return nexora::math::Vector3{v.position[0], v.position[1], v.position[2]};
      };
      const auto face = nexora::math::Cross(point(b) - point(a), point(c) - point(a));
      if (std::abs(nexora::math::Dot(face, point(d) - point(a))) > 1e-6F)
        ++foldedLeaves;
      for (const auto *v : {&a, &b, &c, &d}) {
        const nexora::math::Vector3 normal{v->normal[0], v->normal[1], v->normal[2]};
        const nexora::math::Vector3 tangent{v->tangent[0], v->tangent[1], v->tangent[2]};
        assert(std::abs(nexora::math::Dot(normal, tangent)) < 1e-5F);
        assert(std::abs(nexora::math::Dot(normal, normal) - 1) < 1e-5F);
        assert(std::abs(nexora::math::Dot(tangent, tangent) - 1) < 1e-5F);
        assert(nexora::math::Dot(face, normal) > 0);
      }
    }
  }
  assert(foldedLeaves > 700);

  assert(wide.materials[5].twoSidedLighting && wide.materials[15].twoSidedLighting &&
         wide.materials[16].twoSidedLighting);
  assert(courtyard.Report().find("\"foliage_quad_count\":" + std::to_string(sourceLeaves)) !=
         std::string::npos);
  // Flowing strip faces must agree with their analytic surface normals. This
  // catches an orientation reversal when indexing the shared descending rows.
  std::size_t waterfallTriangles = 0;
  for (const auto &batch : wide.batches) {
    if (batch.materialIndex != 14 || batch.firstInstance != 0)
      continue;
    for (std::size_t triangle = batch.firstIndex; triangle < batch.firstIndex + batch.indexCount;
         triangle += 3) {
      const auto &a = wide.vertices[wide.indices[triangle]];
      const auto &b = wide.vertices[wide.indices[triangle + 1]];
      const auto &c = wide.vertices[wide.indices[triangle + 2]];
      const nexora::math::Vector3 ab{b.position[0] - a.position[0], b.position[1] - a.position[1],
                                     b.position[2] - a.position[2]};
      const nexora::math::Vector3 ac{c.position[0] - a.position[0], c.position[1] - a.position[1],
                                     c.position[2] - a.position[2]};
      const auto normal = nexora::math::Cross(ab, ac);
      assert(nexora::math::Dot(
                 normal, nexora::math::Vector3{a.normal[0], a.normal[1], a.normal[2]}) > 1e-7F);
      ++waterfallTriangles;
    }
  }
  assert(waterfallTriangles > 0);
  assert(wide.materials[12].opacity == 0.95F && !wide.materials[12].castsShadow &&
         wide.materials[12].dielectricRefraction);
#if NEXORA_ASSET_PIPELINE_ENABLED
  // The mineral core stays contained by the closed shell and shares its animated range.
  const auto shell = std::find_if(wide.batches.begin(), wide.batches.end(), [](const auto &batch) {
    return batch.materialIndex == 12 && batch.firstInstance == 0;
  });
  assert(shell != wide.batches.end());
  std::size_t coreCorners = 0;
  for (const auto &batch : wide.batches) {
    if (batch.materialIndex < 18 || batch.materialIndex > 20)
      continue;
    assert(batch.firstInstance == 0 && batch.instanceCount == 1);
    const auto &material = wide.materials[batch.materialIndex];
    assert(material.opacity == 1 && material.refractionIndex == 1 &&
           material.refractionThickness == 0 && !material.refractionFrontSurfaceOnly &&
           !material.castsShadow);
    coreCorners += batch.indexCount;
    for (std::size_t i = batch.firstIndex; i < batch.firstIndex + batch.indexCount; ++i) {
      const auto &vertex = wide.vertices[wide.indices[i]];
      assert(std::hypot(vertex.position[0], vertex.position[2]) < 0.45F);
      // Use the actual convex shell planes, including the frame's shared rotation
      // and hover, rather than a fixed world-space envelope.
      for (std::size_t triangle = shell->firstIndex;
           triangle < shell->firstIndex + shell->indexCount; triangle += 3) {
        const auto &surface = wide.vertices[wide.indices[triangle]];
        float distance = 0;
        for (unsigned axis = 0; axis < 3; ++axis)
          distance += (vertex.position[axis] - surface.position[axis]) * surface.normal[axis];
        assert(distance < -1e-4F);
      }
    }
  }
  assert(coreCorners == 144);
#endif

  assert(covered == wide.indices.size() && wide.instances.size() > 438 && wide.planarReflection);
  const auto columnBatch =
      std::find_if(wide.batches.begin(), wide.batches.end(), [](const auto &batch) {
        return batch.firstInstance == 1 && batch.instanceCount == 4;
      });
  assert(columnBatch != wide.batches.end() && columnBatch->materialIndex == 0);
  for (std::size_t i = 1; i <= 4; ++i)
    assert(Nexora::Presentation::ValidateSceneInstance(wide.instances[i]));
  assert(wide.instances[1].translation[0] == -4.5F && wide.instances[2].translation[0] == -5.5F);
  assert(wide.instances[2].scale[1] == 0.7F && wide.instances[4].scale[1] == 0.7F);
  std::size_t pavingCount = 0, wetPavingCount = 0;
  std::set<std::pair<float, float>> pavingLocations;
  for (const auto &batch : wide.batches)
    if ((batch.materialIndex == 0 || batch.materialIndex == 21) && batch.firstInstance >= 5 &&
        batch.firstInstance < 437) {
      assert(batch.indexCount == 54); // Nine faces per original bevelled tile.
      pavingCount += batch.instanceCount;
      if (batch.materialIndex == 21)
        wetPavingCount += batch.instanceCount;
      for (std::size_t i = batch.firstInstance; i < batch.firstInstance + batch.instanceCount;
           ++i) {
        const auto &tile = wide.instances[i];
        assert(Nexora::Presentation::ValidateSceneInstance(tile));
        assert(pavingLocations.emplace(tile.translation[0], tile.translation[2]).second);
        if (batch.materialIndex == 21) {
          const float d0x = tile.translation[0] - 0.2F, d0z = tile.translation[2] - 3.8F;
          const float d1x = tile.translation[0] - 2.9F, d1z = tile.translation[2] - 2.8F;
          assert(std::min(d0x * d0x + d0z * d0z, d1x * d1x + d1z * d1z) < 2.5F * 2.5F);
        }
        assert(std::abs(tile.translation[0]) > 1.2F || std::abs(tile.translation[2]) > 1.2F);
      }
    }
  assert(pavingCount == 432 && pavingLocations.size() == 432 && wide.vertices.size() < 65536);
  assert(wetPavingCount > 0 && wetPavingCount < pavingCount && selectedMaterials[21]);
  assert(wide.materials[21].roughness < wide.materials[0].roughness);
  assert(wide.materials[21].metallic == 0);
  assert(wide.materials[21].reflectionRole == Nexora::Presentation::SceneReflectionRole::Receiver);
  assert(std::none_of(wide.batches.begin(), wide.batches.end(), [&](const auto &batch) {
    return batch.materialIndex == 21 + sourceMaterialCount;
  }));
  std::size_t masonryCount = 0;
  for (const auto &batch : wide.batches)
    if (batch.materialIndex < sourceMaterialCount && batch.firstInstance >= 437) {
      assert(batch.materialIndex == 0 || batch.materialIndex == 8);
      assert(batch.indexCount == 132); // Exact 26-face authored bevel profile.
      masonryCount += batch.instanceCount;
      // Duplicated UV/normal corners still form one closed physical stone surface.
      using BoundaryPoint = std::array<long long, 3>;
      std::map<std::pair<BoundaryPoint, BoundaryPoint>, unsigned> stoneEdges;
      std::set<BoundaryPoint> stonePoints;
      const auto pointKey = [](const auto &vertex) {
        return BoundaryPoint{std::llround(vertex.position[0] * 1000000.0),
                             std::llround(vertex.position[1] * 1000000.0),
                             std::llround(vertex.position[2] * 1000000.0)};
      };
      // Curved wedges must retain outward triangle winding and finite normals.
      for (std::size_t triangle = batch.firstIndex; triangle < batch.firstIndex + batch.indexCount;
           triangle += 3) {
        const auto &a = wide.vertices[wide.indices[triangle]];
        const auto &b = wide.vertices[wide.indices[triangle + 1]];
        const auto &c = wide.vertices[wide.indices[triangle + 2]];
        const nexora::math::Vector3 ab{b.position[0] - a.position[0], b.position[1] - a.position[1],
                                       b.position[2] - a.position[2]};
        const nexora::math::Vector3 ac{c.position[0] - a.position[0], c.position[1] - a.position[1],
                                       c.position[2] - a.position[2]};
        const nexora::math::Vector3 normal{a.normal[0], a.normal[1], a.normal[2]};
        assert(nexora::math::Dot(nexora::math::Cross(ab, ac), normal) > 1e-7F);
        const std::array points{pointKey(a), pointKey(b), pointKey(c)};
        for (unsigned edge = 0; edge < 3; ++edge) {
          auto from = points[edge], to = points[(edge + 1) % 3];
          assert(from != to);
          stonePoints.insert(from);
          if (to < from)
            std::swap(from, to);
          ++stoneEdges[{from, to}];
        }
      }
      assert(stonePoints.size() == 24 && stoneEdges.size() == 66);
      for (const auto &[edge, count] : stoneEdges)
        assert(count == 2);
      for (std::size_t i = batch.firstInstance; i < batch.firstInstance + batch.instanceCount; ++i)
        assert(Nexora::Presentation::ValidateSceneInstance(wide.instances[i]));
    }
  assert(masonryCount > 100 && wide.instances.size() == 438 + masonryCount);

  // The continuous ridge must have one shared outer boundary, not disconnected
  // per-quad islands. Interior edges belong to exactly two triangles.
  const auto ridge = std::find_if(wide.batches.begin(), wide.batches.end(), [](const auto &batch) {
    return batch.materialIndex == 9 && batch.firstInstance == 0;
  });
  assert(ridge != wide.batches.end());
  std::map<std::pair<std::uint16_t, std::uint16_t>, unsigned> ridgeEdges;
  for (std::size_t triangle = ridge->firstIndex; triangle < ridge->firstIndex + ridge->indexCount;
       triangle += 3)
    for (unsigned edge = 0; edge < 3; ++edge) {
      auto a = wide.indices[triangle + edge];
      auto b = wide.indices[triangle + (edge + 1) % 3];
      assert(a != b);
      if (a > b)
        std::swap(a, b);
      ++ridgeEdges[{a, b}];
    }
  std::map<std::uint16_t, std::vector<std::uint16_t>> boundary;
  for (const auto &[edge, count] : ridgeEdges) {
    assert(count == 1 || count == 2);
    if (count == 1) {
      boundary[edge.first].push_back(edge.second);
      boundary[edge.second].push_back(edge.first);
    }
  }
  assert(!boundary.empty());
  for (const auto &[vertex, neighbors] : boundary) {
    static_cast<void>(vertex);
    assert(neighbors.size() == 2);
  }
  std::set<std::uint16_t> visitedBoundary;
  const auto start = boundary.begin()->first;
  auto current = start, previous = start;
  do {
    assert(visitedBoundary.insert(current).second);
    const auto &neighbors = boundary.at(current);
    const auto next = neighbors[0] == previous ? neighbors[1] : neighbors[0];
    previous = current;
    current = next;
  } while (current != start);
  assert(visitedBoundary.size() == boundary.size());

  for (std::size_t i = 0; i < 7; ++i)
    assert(selectedMaterials[i]);
  for (std::size_t i = 8; i < 11; ++i) {
    assert(selectedMaterials[i] && wide.materials[i].castsShadow == (i != 9));
  }
  const auto skyboxIterator =
      std::find_if(wide.batches.begin(), wide.batches.end(),
                   [](const auto &batch) { return batch.materialIndex == 6; });
  assert(skyboxIterator != wide.batches.end() && skyboxIterator->indexCount == 36);
  const auto &skybox = *skyboxIterator;
  std::array<float, 3> skyCenter{};
  for (std::size_t i = 0; i < 24; ++i) {
    const auto &vertex = wide.vertices[wide.indices[skybox.firstIndex] + i];
    for (std::size_t axis = 0; axis < 3; ++axis) {
      assert(std::abs(std::abs(vertex.position[axis] - wide.cameraPosition[axis]) - 120) < 1e-4F);
      skyCenter[axis] += vertex.position[axis] / 24;
    }
  }
  for (std::size_t axis = 0; axis < 3; ++axis)
    assert(std::abs(skyCenter[axis] - wide.cameraPosition[axis]) < 1e-4F);
  assert(wide.materials[13].metallic == 1 && !wide.materials[13].castsShadow);
  assert(!wide.materials[14].castsShadow);
  std::array<float, 3> solarOffset{};
  const auto solarIterator =
      std::find_if(wide.batches.begin(), wide.batches.end(),
                   [](const auto &batch) { return batch.materialIndex == 11; });
  assert(solarIterator != wide.batches.end() && wide.materials[11].emission[0] > 1);
  const auto &solarBatch = *solarIterator;
  for (std::size_t i = 0; i < solarBatch.indexCount; ++i) {
    const auto &v = wide.vertices[wide.indices[solarBatch.firstIndex + i]];
    for (std::size_t axis = 0; axis < 3; ++axis)
      solarOffset[axis] += (v.position[axis] - wide.cameraPosition[axis]) / solarBatch.indexCount;
  }
  float lightLength = 0;
  for (const auto d : wide.light_direction)
    lightLength += d * d;
  lightLength = std::sqrt(lightLength);
  for (std::size_t axis = 0; axis < 3; ++axis)
    assert(std::abs(solarOffset[axis] / 104 + wide.light_direction[axis] / lightLength) < 1e-4F);
  assert(wide.materials[6].unlit && !wide.materials[6].castsShadow);
  for (const auto &vertex : wide.vertices) {
    float orthogonal = 0, length = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
      orthogonal += vertex.normal[axis] * vertex.tangent[axis];
      length += vertex.tangent[axis] * vertex.tangent[axis];
    }
    assert(std::abs(orthogonal) < 1e-4F && std::abs(length - 1) < 1e-4F);
    assert(std::abs(vertex.tangent[3]) == 1);
  }
#if NEXORA_ASSET_PIPELINE_ENABLED
  assert(wide.environment && wide.linearTextureUploads.size() == 3);
  assert(wide.environment->specularMipLevels == 7);
  for (const auto &upload : wide.linearTextureUploads)
    assert(Nexora::Presentation::ValidateSceneLinearTexture(upload));
  Press(courtyard, Key::O);
  assert(!courtyard.Scene(1280, 720).environment);
  Press(courtyard, Key::O);
  assert(courtyard.Scene(1280, 720).environment);
  assert(courtyard.Report().find("\"environment_loaded\":true") != std::string::npos);
#endif
  assert(wide.hdr && wide.offscreen && wide.exposure == 1 && wide.bloom && wide.depthOfField);
  Press(courtyard, Key::J);
  assert(!courtyard.Scene(1280, 720).depthOfField);
  Press(courtyard, Key::J);
  assert(courtyard.Scene(1280, 720).depthOfField);
  Press(courtyard, Key::K);
  assert(!courtyard.Scene(1280, 720).bloom);
  Press(courtyard, Key::K);
  assert(courtyard.Scene(1280, 720).bloom);
  assert(wide.shadow && wide.lightingStyle && wide.shadow->resolution == 2048);
  // Side-arcade crowns must remain inside the shadow camera. Their shadows use
  // the same light-space XY, even when projected beyond the central pedestal.
  for (const float x : {-7.5F, 7.5F})
    for (const float z : {-4.0F, 2.0F}) {
      const std::array<float, 4> crown{x, 7.45F, z, 1.0F};
      const auto &matrix = wide.shadow->lightViewProjection;
      std::array<float, 4> clip{};
      for (unsigned row = 0; row < 4; ++row)
        for (unsigned column = 0; column < 4; ++column)
          clip[row] += matrix[row * 4 + column] * crown[column];
      assert(std::abs(clip[0]) < clip[3] && std::abs(clip[1]) < clip[3]);
      assert(clip[2] > 0 && clip[2] < clip[3]);
    }
  for (const float x : {-24.0F, -10.0F, 10.0F, 18.0F}) {
    const std::array<float, 4> crown{x, 8.1F, x < -20 ? -26.0F : -16.0F, 1};
    const auto &matrix = wide.shadow->lightViewProjection;
    std::array<float, 4> clip{};
    for (unsigned row = 0; row < 4; ++row)
      for (unsigned column = 0; column < 4; ++column)
        clip[row] += matrix[row * 4 + column] * crown[column];
    assert(std::abs(clip[0]) < clip[3] && std::abs(clip[1]) < clip[3]);
    assert(clip[2] > 0 && clip[2] < clip[3]);
  }
  for (unsigned tower = 0; tower < 5; ++tower) {
    const std::array<float, 4> crown{-4 + tower * 5.0F, 10.6F + static_cast<float>(tower * 7 % 6),
                                     -23, 1};
    const auto &matrix = wide.shadow->lightViewProjection;
    std::array<float, 4> clip{};
    for (unsigned row = 0; row < 4; ++row)
      for (unsigned column = 0; column < 4; ++column)
        clip[row] += matrix[row * 4 + column] * crown[column];
    assert(std::abs(clip[0]) < clip[3] && std::abs(clip[1]) < clip[3]);
    assert(clip[2] > 0 && clip[2] < clip[3]);
  }
  Press(courtyard, Key::F6);
  assert(!courtyard.Scene(1280, 720).shadow);
  Press(courtyard, Key::F6);
  assert(courtyard.Scene(1280, 720).shadow);
  Press(courtyard, Key::G);
  assert(!courtyard.Scene(1280, 720).lightingStyle);
  Press(courtyard, Key::G);
  assert(courtyard.Scene(1280, 720).lightingStyle);
  Press(courtyard, Key::RightBracket);
  assert(courtyard.Scene(1280, 720).shadow->normalBias == wide.shadow->normalBias * 2);
  Press(courtyard, Key::LeftBracket);
  assert(courtyard.Scene(1280, 720).shadow->normalBias == wide.shadow->normalBias);
  Press(courtyard, Key::E);
  assert(courtyard.Scene(1280, 720).exposure == 0.25F);
  Press(courtyard, Key::E);
  assert(courtyard.Scene(1280, 720).exposure == 1);
  Press(courtyard, Key::P);
  assert(!courtyard.Scene(1280, 720).pbr);
  assert(!courtyard.Scene(1280, 720).hdr);
  assert(!courtyard.Scene(1280, 720).environment &&
         courtyard.Scene(1280, 720).linearTextureUploads.empty());
  Press(courtyard, Key::P);
  assert(courtyard.Scene(1280, 720).pbr);
  const auto wideMatrix = std::to_array(wide.model_view_projection);
  Press(courtyard, Key::B);
  const auto closeMatrix = std::to_array(courtyard.Scene(1280, 720).model_view_projection);
  assert(closeMatrix != wideMatrix);
  Press(courtyard, Key::B);
  const auto motionMatrix = std::to_array(courtyard.Scene(1280, 720).model_view_projection);
  assert(motionMatrix != closeMatrix && motionMatrix != wideMatrix);
  Press(courtyard, Key::B);
  assert(std::to_array(courtyard.Scene(1280, 720).model_view_projection) == wideMatrix);
  Nexora::Presentation::SurfaceDiagnostics diagnostics{};
  assert(!courtyard.Overlay(1280, 720, "validation", diagnostics, 0).vertices.empty());
  Press(courtyard, Key::F4);
  const auto hidden = courtyard.Overlay(1280, 720, "validation", diagnostics, 0);
  assert(hidden.vertices.empty() && hidden.commands.empty() && hidden.textureUploads.empty());
  Press(courtyard, Key::F4);
  assert(!courtyard.Overlay(1280, 720, "validation", diagnostics, 0).vertices.empty());
  Press(courtyard, Key::Digit1);
  assert(courtyard.Selected() == "hub");
  Press(courtyard, Key::Digit9);
  assert(courtyard.Selected() == "courtyard");
  assert(std::to_array(courtyard.Scene(1280, 720).model_view_projection) == wideMatrix);
#if NEXORA_ASSET_PIPELINE_ENABLED
  assert(courtyard.Report().find("\"representative_asset_loaded\":true") != std::string::npos);
  assert(courtyard.Report().find("\"adopted_mesh_count\":3") != std::string::npos);
  const auto adopted = courtyard.Scene(1280, 720);
  assert(adopted.textureId == 2 && adopted.textureUploads.size() == 10);
  assert(adopted.materials[0].normalTextureId == 11 && adopted.materials[1].ormTextureId == 15);
  assert(adopted.textureUploads[0].pixels.size() == 64 * 64 * 4);
  assert(adopted.textureUploads[1].width == 256 && adopted.textureUploads[1].height == 256);
  assert(adopted.textureUploads[1].pixels.size() == 256 * 256 * 4);
  assert(adopted.textureUploads.back().width == 768 && adopted.textureUploads.back().height == 512);
  assert(adopted.textureUploads.back().pixels.size() == 768 * 512 * 4);
#endif
  assert(courtyard.Scene(1280, 720).materials[5].alphaCutoff == 0.5F);
  courtyard.Tick(0.5);
  const float animatedTime = courtyard.Scene(1280, 720).vegetationTime;
  Press(courtyard, Key::Space);
  courtyard.Tick(0.5);
  assert(courtyard.Scene(1280, 720).vegetationTime == animatedTime);
  Press(courtyard, Key::Enter);
  const auto active = courtyard.Scene(1280, 720);
  assert(active.pointLight && active.pointLight->radius == 4.5F);
  Press(courtyard, Key::F9);
  assert(!courtyard.Scene(1280, 720).pointLight);
  Press(courtyard, Key::F9);
  assert(courtyard.Scene(1280, 720).pointLight);
  const auto activeParticles =
      std::find_if(active.batches.begin(), active.batches.end(),
                   [](const auto &batch) { return batch.materialIndex == 7; });
  assert(activeParticles != active.batches.end() && activeParticles->indexCount == 48 * 6);
  const auto mirroredParticles =
      std::find_if(active.batches.begin(), active.batches.end(), [&](const auto &batch) {
        return batch.materialIndex == 7 + sourceMaterialCount;
      });
  assert(mirroredParticles != active.batches.end() && mirroredParticles->indexCount == 48 * 6);
  const std::vector<Nexora::Presentation::SceneVertex> frozen(active.vertices.begin(),
                                                              active.vertices.end());
  courtyard.Tick(0.5);
  const auto pausedScene = courtyard.Scene(1280, 720);
  assert(std::memcmp(frozen.data(), pausedScene.vertices.data(),
                     pausedScene.vertices.size_bytes()) == 0);
  Press(courtyard, Key::R);
  assert(courtyard.Scene(1280, 720).vegetationTime == 0);
  Press(courtyard, Key::Enter);
  const auto inactive = courtyard.Scene(1280, 720);
  assert(std::none_of(inactive.batches.begin(), inactive.batches.end(), [&](const auto &batch) {
    return batch.materialIndex == 7 || batch.materialIndex == 7 + sourceMaterialCount;
  }));
  Press(courtyard, Key::U);
  assert(courtyard.Scene(1280, 720).materials[12].opacity == 1);
  Press(courtyard, Key::U);
  assert(courtyard.Scene(1280, 720).materials[12].opacity == 0.95F);
  session.RerunProbe(0, nexora::showcase::ErrorInjection::DependencyCycle);
  assert(session.Probes()[0].status == nexora::showcase::ProbeStatus::Unsupported);
  assert(session.Healthy());
  session.RerunProbe(0);
  assert(session.Markdown().find("sample_tick") != std::string::npos);
  const auto scene = session.Scene(1280, 720);
  assert(!scene.vertices.empty() && !scene.indices.empty());
  for (const auto index : scene.indices)
    assert(index < scene.vertices.size());
  const auto originalMatrix =
      std::array{scene.model_view_projection[0], scene.model_view_projection[2]};
  WindowEvent move{};
  move.type = WindowEventType::Key;
  move.value0 = static_cast<int>(Key::D);
  move.value1 = 1;
  session.Event(move, 1280, 720);
  session.Tick(0.5);
  const auto movedScene = session.Scene(1280, 720);
  assert(originalMatrix[0] != movedScene.model_view_projection[0] ||
         originalMatrix[1] != movedScene.model_view_projection[2]);
  move.value1 = 0;
  session.Event(move, 1280, 720);
  Press(session, Key::Digit2);
  const auto instanced = session.Scene(1280, 720);
  assert(instanced.vertices.size() == 24 && instanced.indices.size() == 36);
  assert(instanced.instances.size() == 4);
  assert(instanced.instances[0].scale[0] == 6 && instanced.instances[1].translation[1] == 1.5F);
  Press(session, Key::P);
  const auto quad = session.Scene(1280, 720);
  assert(quad.vertices.size() == 28 && quad.indices.size() == 42 && quad.instances.empty());
  Press(session, Key::P);
  const auto triangle = session.Scene(1280, 720);
  assert(triangle.vertices.size() == 27 && triangle.indices.size() == 39 &&
         triangle.instances.empty());
  Press(session, Key::P);
  assert(session.Scene(1280, 720).instances.size() == 4);
  Press(session, Key::Digit3);
  assert(session.Selected() == "scene");
#if NEXORA_EDITOR_SDK_ENABLED
  Press(session, Key::E);
  assert(session.Report().find("Transform.x = 0.50") != std::string::npos);
  Press(session, Key::U);
  assert(session.Report().find("Transform.x = 0.00") != std::string::npos);
  Press(session, Key::P);
  session.Tick(0.1);
  assert(session.Report().find("Transform.x = 0.00") != std::string::npos); // Play is isolated.
  Press(session, Key::F5);
  Press(session, Key::E);
  Press(session, Key::U);
  assert(session.Report().find("Transform.x = 0.00") != std::string::npos);
#endif
  for (const auto room : {"input", "gameplay", "presentation", "streaming", "shipping"}) {
    session.Select(room);
    session.Tick(1.0 / 60);
    assert(session.Healthy());
    assert(!session.Scene(960, 540).vertices.empty());
  }
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  session.Select("gameplay");
  const auto capsule = session.Scene(1280, 720);
  assert(capsule.vertices.size() > 250);
  assert(capsule.vertices[24].position[1] >= -0.0001F); // Feet-origin capsule stays above ground.
  for (const auto &vertex : capsule.vertices) {
    const float length =
        std::sqrt(vertex.normal[0] * vertex.normal[0] + vertex.normal[1] * vertex.normal[1] +
                  vertex.normal[2] * vertex.normal[2]);
    assert(std::abs(length - 1) < 0.001F);
  }
#endif
#if NEXORA_PRESENTATION_ENABLED
  session.Select("presentation");
  const auto before = session.Scene(1280, 720).vertices[96]; // After floor and 3 joint markers.
  Press(session, Key::J);
  session.Tick(0.3);
  const auto after = session.Scene(1280, 720).vertices[96];
  assert(before.position[0] != after.position[0] || before.position[1] != after.position[1]);
#endif
  auto overlay = session.Overlay(1280, 720, "validation", {}, 16.67);
  assert(!overlay.vertices.empty() && overlay.textureUploads.size() == 1);
  overlay = session.Overlay(960, 540, "validation", {}, 16.67);
  assert(overlay.textureUploads.empty());
  Nexora::Presentation::SurfaceDiagnostics resized{};
  resized.resizeGenerations = 1;
  assert(session.Overlay(960, 540, "validation", resized, 16.67).textureUploads.size() == 1);
  Press(session, Key::L);
  assert(session.Report().find("Missing translation uses English fallback") != std::string::npos ||
         session.Selected() != "input");
  session.Select("input");
  assert(session.Report().find("Missing translation uses English fallback") != std::string::npos);
  RoomSession visualTour("courtyard", true);
  assert(visualTour.Selected() == "courtyard" && !visualTour.TourComplete());
  const auto initialTourMatrix = std::to_array(visualTour.Scene(1280, 720).model_view_projection);
  for (int i = 0; i < 25; ++i)
    visualTour.Tick(1);
  assert(std::to_array(visualTour.Scene(1280, 720).model_view_projection) != initialTourMatrix);
  Press(visualTour, Key::Space);
  const auto pausedTourMatrix = std::to_array(visualTour.Scene(1280, 720).model_view_projection);
  visualTour.Tick(1);
  assert(std::to_array(visualTour.Scene(1280, 720).model_view_projection) == pausedTourMatrix);
  Press(visualTour, Key::Space);
  for (int i = 25; i < 100; ++i) {
    visualTour.Tick(1);
    static_cast<void>(visualTour.Scene(1280, 720));
  }
  assert(visualTour.TourComplete());
  Press(visualTour, Key::J);
  assert(!visualTour.Scene(1280, 720).depthOfField);
  assert(visualTour.Report().find("\"duration_seconds\":100") != std::string::npos);
  visualTour.ReplayTour();
  assert(!visualTour.TourComplete() && visualTour.Scene(1280, 720).vegetationTime == 0);
  assert(visualTour.Scene(1280, 720).depthOfField);
  assert(std::to_array(visualTour.Scene(1280, 720).model_view_projection) == initialTourMatrix);
  RoomSession quality("courtyard");
  quality.SetAnimationPaused(true);
  quality.SetDeviceActive(true);
  std::array<std::size_t, 3> qualityVertices{};
  for (unsigned tier = 0; tier < 3; ++tier) {
    const auto name = std::array<std::string_view, 3>{"basic", "standard", "high"}[tier];
    quality.SetQuality(name);
    quality.Tick(0.1);
    const auto draw = quality.Scene(1280, 720);
    assert(draw.vertices.size() < 65536);
    assert(draw.hdr && draw.pbr && draw.shadow && draw.shadow->resolution == (tier ? 2048U : 512U));
    assert(draw.vegetationTime == 0 && quality.QualityName() == name);
    assert(draw.bloom.has_value() == (tier != 0));
    assert(draw.atmosphere.has_value() == (tier != 0));
#if NEXORA_ASSET_PIPELINE_ENABLED
    assert(draw.environment.has_value() == (tier != 0));
    assert(draw.postProcessAntiAliasing == (tier != 0));
#endif
    assert(draw.materials[5].twoSidedLighting && draw.materials[15].twoSidedLighting);
    assert(draw.materials[6].unlit && !draw.materials[6].castsShadow);
    assert(draw.materials[8].castsShadow == (tier != 0) &&
           draw.materials[10].castsShadow == (tier != 0) && !draw.materials[9].castsShadow);
    assert(draw.materials[7].unlit && !draw.materials[7].castsShadow);
    const auto particleBatch =
        std::find_if(draw.batches.begin(), draw.batches.end(),
                     [](const auto &batch) { return batch.materialIndex == 7; });
    assert(particleBatch != draw.batches.end() && particleBatch->indexCount == (24U << tier) * 6);
    assert(draw.planarReflection.has_value() == (tier != 0));
    assert(draw.materials[12].dielectricRefraction == (tier != 0));
    assert(draw.instances.size() == (tier != 0 ? 438 : 437) + masonryCount);
    qualityVertices[tier] = draw.vertices.size();
  }
  assert(quality.Scene(1280, 720).materials[12].refractionIndex == 1.46F);
  Press(quality, Key::F8);
  assert(quality.Scene(1280, 720).materials[12].refractionIndex == 1 &&
         quality.Scene(1280, 720).materials[12].refractionThickness == 0 &&
         !quality.Scene(1280, 720).materials[12].dielectricRefraction);
  Press(quality, Key::F8);
  assert(quality.Scene(1280, 720).materials[12].refractionIndex == 1.46F);
  Press(quality, Key::F7);
  assert(!quality.Scene(1280, 720).atmosphere);
  Press(quality, Key::F7);
  assert(quality.Scene(1280, 720).atmosphere);
  // Basic retains the standalone ripple mesh; Standard/High use shared mirror instances.
  // Compare quality geometry under the same reflection setting to retain the strict budget check.
  Press(quality, Key::V);
  const auto highWithoutReflection = quality.Scene(1280, 720).vertices.size();
  quality.SetQuality("standard");
  const auto standardWithoutReflection = quality.Scene(1280, 720).vertices.size();
  assert(qualityVertices[0] < standardWithoutReflection &&
         standardWithoutReflection < highWithoutReflection);
  Press(quality, Key::V);
  quality.SetQuality("high");
  Press(quality, Key::Q);
  assert(quality.QualityName() == "basic");
  assert(quality.Scene(1280, 720).vertices.size() == qualityVertices[0]);
  bool qualityRejected = false;
  try {
    quality.SetQuality("invalid");
  } catch (const std::invalid_argument &) {
    qualityRejected = true;
  }
  assert(qualityRejected && quality.QualityName() == "basic");

  RoomSession explore("courtyard");
  Press(explore, Key::C);
  const auto freeStart = std::to_array(explore.Scene(1280, 720).model_view_projection);
  WindowEvent freeMove{};
  freeMove.type = WindowEventType::Key;
  freeMove.value0 = static_cast<int>(Key::W);
  freeMove.value1 = 1;
  explore.Event(freeMove, 1280, 720);
  explore.Tick(0.5);
  freeMove.value1 = 0;
  explore.Event(freeMove, 1280, 720);
  assert(std::to_array(explore.Scene(1280, 720).model_view_projection) != freeStart);
  assert(explore.Report().find("\"camera_mode\":\"free\"") != std::string::npos);
  Press(explore, Key::B);
  assert(explore.Report().find("\"camera_mode\":\"orbit\"") != std::string::npos);
  session.ReplayTour();
  session.Tick(1);
  Press(session, Key::Space);
  for (int i = 0; i < 40; ++i)
    session.Tick(1);
  assert(session.Selected() == "hub");
  Press(session, Key::Space);
  for (int i = 0; i < 210; ++i)
    session.Tick(1);
  assert(session.Selected() == "shipping");
  assert(session.Report().find("\"paused\":true") != std::string::npos);
  assert(session.Report().find("\"step\":6") != std::string::npos);
  Press(session, Key::R);
  assert(session.Selected() == "hub");
  bool invalid = false;
  try {
    session.Tick(-1);
  } catch (const std::invalid_argument &) {
    invalid = true;
  }
  assert(invalid);
  invalid = false;
  try {
    session.Select("missing");
  } catch (const std::invalid_argument &) {
    invalid = true;
  }
  assert(invalid);
  RoomSession minimal("gameplay", false, true);
  minimal.Tick(0.1);
  assert(minimal.Healthy());
  assert(minimal.Probes()[8].status == nexora::showcase::ProbeStatus::Unsupported);
}
