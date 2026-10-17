#include <dx/linalg.h>

ByteAddressBuffer input : register(t0);
RWByteAddressBuffer output : register(u1);

[numthreads(1, 1, 1)]
void computeMain() {
  using namespace dx::linalg;
  // Row-major 4x4 F32 matrix at0, row stride16, 128-byte aligned input base.
  Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Thread> matrix =
      Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Thread>::Load<MatrixLayout::RowMajor>(
          input, 0, 16, 128);
  vector<float, 4> value = asfloat(input.Load4(64));
  vector<float, 4> result = Multiply<float>(matrix, value);
  output.Store4(0, asuint(result));
}
