[[vk::binding(0,0)]] StructuredBuffer<uint> input : register(t0);
[[vk::binding(1,0)]] RWStructuredBuffer<uint> output : register(u1);
[numthreads(1,1,1)]
void computeMain(uint3 id:SV_DispatchThreadID) {
 if(id.x!=0 || input[0]>64)return;
 for(uint cell=0;cell<16;++cell)output[cell]=255;
 for(uint record=0;record<input[0];++record) {
  uint base=1+record*4;
  float2 uv=float2(asfloat(input[base]),asfloat(input[base+1]));
  uint mip=input[base+2], enabled=input[base+3];
  if(enabled!=1 || mip>2 || !all(isfinite(uv)) || any(uv<0) || any(uv>=1))continue;
  uint2 cell=uint2(floor(uv*4));uint index=cell.y*4+cell.x;
  output[index]=min(output[index],mip);
 }
}
