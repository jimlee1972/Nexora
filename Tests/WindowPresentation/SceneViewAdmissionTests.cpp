#include "SceneViewAdmission.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  try {
    using namespace Nexora::Presentation;
    detail::SceneViewAdmission admission;
    SceneDrawData left, right;
    left.viewport = {0, 0, 320, 480};
    right.viewport = {320, 0, 320, 480};
    Require(admission.CanSubmit(left, 640, 480, true) && admission.Count() == 0,
            "query consumed admission");
    Require(admission.Commit(left, 640, 480, true), "first bounded view rejected");
    for (const SceneViewport bounds : {SceneViewport{319, 0, 320, 480},
                                       {0, 0, 320, 480},
                                       {320, 479, 320, 2},
                                       {320, 0, 0, 480},
                                       {UINT32_MAX, 0, 2, 480}}) {
      auto invalid = right;
      invalid.viewport = bounds;
      Require(!admission.Commit(invalid, 640, 480, true) && admission.Count() == 1,
              "invalid/overlapping view consumed capacity");
    }
    Require(admission.Commit(right, 640, 480, true) && admission.Count() == 2 &&
                !admission.Commit(right, 640, 480, true),
            "touching view boundary or maximum admission is wrong");
    for (int exclusive = 0; exclusive < 5; ++exclusive) {
      admission.Reset();
      auto first = left;
      if (exclusive == 0)
        first.viewport = {};
      if (exclusive == 1)
        first.offscreen = true;
      if (exclusive == 2)
        first.hdr = true;
      if (exclusive == 3)
        first.shadow = SceneDirectionalShadow{};
      Require(admission.Commit(first, 640, 480, exclusive != 4) &&
                  !admission.Commit(right, 640, 480, true),
              "exclusive scene workflow admitted a second view");
    }
    admission.Reset();
    Require(admission.Commit(left, 640, 480, true), "reset did not clear previous view");
    for (int exclusive = 0; exclusive < 5; ++exclusive) {
      auto second = right;
      if (exclusive == 0)
        second.viewport = {};
      if (exclusive == 1)
        second.offscreen = true;
      if (exclusive == 2)
        second.hdr = true;
      if (exclusive == 3)
        second.shadow = SceneDirectionalShadow{};
      Require(!admission.Commit(second, 640, 480, exclusive != 4) && admission.Count() == 1,
              "second view changed mode or consumed rejected admission");
    }
    Require(admission.Commit(right, 640, 480, true), "rejections prevented valid second view");
    admission.Reset();
    left.viewport = {0, 0, UINT32_MAX - 1, 1};
    right.viewport = {UINT32_MAX - 1, 0, 1, 1};
    Require(admission.Commit(left, UINT32_MAX, 1, true) &&
                admission.Commit(right, UINT32_MAX, 1, true),
            "uint32 boundary overflowed touching view rectangles");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
