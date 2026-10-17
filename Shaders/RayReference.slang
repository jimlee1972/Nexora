// Bounded portable ray/triangle oracle. Input: counts, nine floats/triangle, eight floats/ray.
[[vk::binding(0, 0)]] StructuredBuffer<uint> referenceInput : register(t0);
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> referenceHits : register(u1);
float3 load3(uint offset) {
  return float3(asfloat(referenceInput[offset]), asfloat(referenceInput[offset+1]), asfloat(referenceInput[offset+2]));
}
[numthreads(1, 1, 1)]
void computeMain(uint3 id : SV_DispatchThreadID) {
  uint count=referenceInput[0], triangles=referenceInput[1];
  if(count>64 || triangles>8 || id.x>=count) return;
  uint base=2+triangles*9+id.x*8;
  float3 origin=load3(base), direction=load3(base+4);
  float minimum=asfloat(referenceInput[base+3]), maximum=asfloat(referenceInput[base+7]);
  float lengthSquared=dot(direction,direction);
  bool valid=all(isfinite(origin)) && all(isfinite(direction)) && isfinite(minimum) && isfinite(maximum) &&
             isfinite(lengthSquared) && lengthSquared>0 && minimum>=0 && maximum>=minimum;
  uint primitive=valid ? 0xffffffffu : 0xfffffffeu;
  float distance=valid ? maximum : 0;
  float2 barycentrics=float2(0,0);
  if(valid) for(uint i=0;i<triangles;++i) {
    uint vertex=2+i*9;
    float3 a=load3(vertex), e1=load3(vertex+3)-a, e2=load3(vertex+6)-a;
    float3 p=cross(direction,e2);
    float determinant=dot(e1,p);
    if(determinant==0 || !isfinite(determinant)) continue;
    float inverse=1/determinant;
    float3 t=origin-a;
    float u=dot(t,p)*inverse;
    float3 q=cross(t,e1);
    float v=dot(direction,q)*inverse;
    float hit=dot(e2,q)*inverse;
    if(u>=0 && v>=0 && u+v<=1 && hit>=minimum && hit<=distance) {
      primitive=i; distance=hit; barycentrics=float2(u,v);
    }
  }
  referenceHits[id.x*4]=primitive;
  referenceHits[id.x*4+1]=asuint(distance);
  referenceHits[id.x*4+2]=asuint(barycentrics.x);
  referenceHits[id.x*4+3]=asuint(barycentrics.y);
}
