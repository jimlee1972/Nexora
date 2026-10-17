RaytracingAccelerationStructure scene : register(t0);
struct RayInput {
  float3 origin;
  float minimum;
  float3 direction;
  float maximum;
};
struct [raypayload] Hit {
  uint primitive : read(caller, closesthit, miss) : write(caller, closesthit, miss);
  float distance : read(caller, closesthit, miss) : write(caller, closesthit, miss);
  float2 barycentrics : read(caller, closesthit, miss) : write(caller, closesthit, miss);
};
StructuredBuffer<RayInput> rays : register(t1);
RWStructuredBuffer<Hit> hits : register(u1);

bool validRay(RayInput input) {
  float lengthSquared = dot(input.direction, input.direction);
  return all(isfinite(input.origin)) && all(isfinite(input.direction)) && isfinite(input.minimum) &&
      isfinite(input.maximum) && isfinite(lengthSquared) && lengthSquared > 0 &&
      input.minimum >= 0 && input.maximum >= input.minimum;
}

[shader("raygeneration")]
void rayGeneration() {
  uint index = DispatchRaysIndex().x;
  RayInput input = rays[index];
  RayDesc ray = {input.origin, input.minimum, input.direction, input.maximum};
  Hit result = {0xffffffff, input.maximum, float2(0, 0)};
  if (validRay(input)) {
    dx::HitObject hit = dx::HitObject::TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 1, 0, ray, result);
    dx::MaybeReorderThread(hit);
    dx::HitObject::Invoke(hit, result);
  } else {
    result.primitive = 0xfffffffeu;
    result.distance = 0;
  }
  // Explicit fields preserve payload-access qualifiers through the UAV store.
  hits[index].primitive = result.primitive;
  hits[index].distance = result.distance;
  hits[index].barycentrics = result.barycentrics;
}

[shader("miss")]
void rayMiss(inout Hit result) {
  result.primitive = 0xffffffff;
}

[shader("closesthit")]
void rayClosest(inout Hit result, BuiltInTriangleIntersectionAttributes attributes) {
  result.primitive = PrimitiveIndex();
  result.distance = RayTCurrent();
#if defined(NEXORA_RAY_IDENTITY_NEGATIVE)
  result.distance += 1;
#endif
  result.barycentrics = attributes.barycentrics;
}
