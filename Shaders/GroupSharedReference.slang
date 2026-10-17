// Equivalent integer result without the SM6.10 64KiB shared allocation.
StructuredBuffer<uint> input : register(t0);
RWStructuredBuffer<uint> output : register(u1);
[numthreads(1,1,1)]
void computeMain() {
 uint sum=0;
 for(uint i=0;i<16384;++i) sum+=input[i%8]+i;
 output[0]=sum;
}
