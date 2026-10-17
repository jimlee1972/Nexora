StructuredBuffer<uint> input : register(t0);
RWStructuredBuffer<uint> output : register(u1);
[numthreads(1,1,1)]
void computeMain() {
 for(uint row=0;row<4;++row) {
  float sum=0;
  for(uint col=0;col<4;++col) sum+=asfloat(input[row*4+col])*asfloat(input[16+col]);
  output[row]=asuint(sum);
 }
}
