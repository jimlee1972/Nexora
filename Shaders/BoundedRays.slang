RaytracingAccelerationStructure scene : register(t0);
struct RayInput { float3 origin; float minimum; float3 direction; float maximum; };
struct Hit { uint primitive; float distance; float2 barycentrics; };
StructuredBuffer<RayInput> rays : register(t1);
RWStructuredBuffer<Hit> hits : register(u1);
bool validRay(RayInput input) {
 float lengthSquared=dot(input.direction,input.direction);
 return all(isfinite(input.origin)) && all(isfinite(input.direction)) && isfinite(input.minimum) &&
 isfinite(input.maximum) && isfinite(lengthSquared) && lengthSquared>0 && input.minimum>=0 && input.maximum>=input.minimum;
}
[shader("raygeneration")]
void rayGeneration() {
 uint index=DispatchRaysIndex().x;
 RayInput input=rays[index];
 RayDesc ray={input.origin,input.minimum,input.direction,input.maximum};
 Hit result={0xffffffff,input.maximum,float2(0,0)};
 if(validRay(input)) TraceRay(scene,RAY_FLAG_NONE,0xff,0,1,0,ray,result);
 else { result.primitive=0xfffffffeu; result.distance=0; }
 hits[index]=result;
}
[shader("miss")]
void rayMiss(inout Hit result) { result.primitive=0xffffffff; }
[shader("closesthit")]
void rayClosest(inout Hit result, BuiltInTriangleIntersectionAttributes attributes) {
 result.primitive=InstanceID()+PrimitiveIndex();
 result.distance=RayTCurrent();
#if defined(NEXORA_RAY_IDENTITY_NEGATIVE)
 result.distance+=1;
#endif
 result.barycentrics=attributes.barycentrics;
}
[numthreads(1,1,1)]
void inlineQuery(uint3 id:SV_DispatchThreadID) {
 RayInput input=rays[id.x];
 RayDesc ray={input.origin,input.minimum,input.direction,input.maximum};
 if(!validRay(input)) { Hit invalid={0xfffffffeu,0,float2(0,0)}; hits[id.x]=invalid; return; }
 RayQuery<RAY_FLAG_NONE> query;
 query.TraceRayInline(scene,RAY_FLAG_NONE,0xff,ray);
 while(query.Proceed()) {}
 Hit result={0xffffffff,input.maximum,float2(0,0)};
 if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT) {
 result.primitive=query.CommittedInstanceID()+query.CommittedPrimitiveIndex();
 result.distance=query.CommittedRayT();
 result.barycentrics=query.CommittedTriangleBarycentrics();
 }
 hits[id.x]=result;
}
