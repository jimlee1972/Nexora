StructuredBuffer<uint> input : register(t0);
RWStructuredBuffer<uint> output : register(u1);
[numthreads(1,1,1)]
void computeMain() {
 uint sum=0;
 for(uint i=0;i<8;++i) sum+=input[i]*(i+1);
 output[0]=sum;
}
