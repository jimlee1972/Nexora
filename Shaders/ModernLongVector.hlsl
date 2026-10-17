// SM6.9 eight-component vector operations, using the existing t0/u1 compute roots.
StructuredBuffer<vector<uint, 8> > input : register(t0);
RWStructuredBuffer<uint> output : register(u1);

[numthreads(1, 1, 1)]
void computeMain() {
  vector<uint, 8> value = input[0];
  uint sum = 0;
  for (uint i = 0; i < 8; ++i)
    sum += value[i] * (i + 1);
  output[0] = sum;
}
