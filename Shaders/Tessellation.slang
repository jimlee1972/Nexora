// Bounded bootstrap patch and baked triangle-list fallback. Both evaluate the same curved
// boundary; the fallback contains sixteen pre-tessellated triangles and needs no hull/domain stage.
#include "Triangle.slang"
struct PatchFactors { float edges[3] : SV_TessFactor; float inside : SV_InsideTessFactor; };
PatchFactors patchFactors(InputPatch<VertexOutput, 3> patch) {
 PatchFactors f; f.edges[0]=4; f.edges[1]=4; f.edges[2]=4; f.inside=4; return f;
}
[shader("hull")]
[domain("tri")][partitioning("integer")][outputtopology("triangle_cw")]
[outputcontrolpoints(3)][patchconstantfunc("patchFactors")][maxtessfactor(4.0)]
VertexOutput tessControlMain(InputPatch<VertexOutput, 3> patch, uint id:SV_OutputControlPointID) {
 return patch[id];
}
VertexOutput evaluatePatch(VertexOutput a, VertexOutput b, VertexOutput c, float3 uv) {
 VertexOutput o; o.position=a.position*uv.x+b.position*uv.y+c.position*uv.z;
 o.position.y += 0.6*uv.x*uv.y; o.color=float3(0.2,0.7,1.0); return o;
}
[shader("domain")][domain("tri")]
VertexOutput tessEvaluationMain(PatchFactors factors, float3 uv:SV_DomainLocation,
 OutputPatch<VertexOutput,3> patch) { return evaluatePatch(patch[0],patch[1],patch[2],uv); }
[shader("vertex")]
VertexOutput preTessellatedVertexMain(uint id:SV_VertexID) {
 const float3 barycentric[48] = {
float3(1, 0, 0),
float3(0.75, 0.25, 0),
float3(0.75, 0, 0.25),
float3(0.75, 0.25, 0),
float3(0.5, 0.25, 0.25),
float3(0.75, 0, 0.25),
float3(0.75, 0, 0.25),
float3(0.5, 0.25, 0.25),
float3(0.5, 0, 0.5),
float3(0.5, 0.25, 0.25),
float3(0.25, 0.25, 0.5),
float3(0.5, 0, 0.5),
float3(0.5, 0, 0.5),
float3(0.25, 0.25, 0.5),
float3(0.25, 0, 0.75),
float3(0.25, 0.25, 0.5),
float3(0, 0.25, 0.75),
float3(0.25, 0, 0.75),
float3(0.25, 0, 0.75),
float3(0, 0.25, 0.75),
float3(0, 0, 1),
float3(0.75, 0.25, 0),
float3(0.5, 0.5, 0),
float3(0.5, 0.25, 0.25),
float3(0.5, 0.5, 0),
float3(0.25, 0.5, 0.25),
float3(0.5, 0.25, 0.25),
float3(0.5, 0.25, 0.25),
float3(0.25, 0.5, 0.25),
float3(0.25, 0.25, 0.5),
float3(0.25, 0.5, 0.25),
float3(0, 0.5, 0.5),
float3(0.25, 0.25, 0.5),
float3(0.25, 0.25, 0.5),
float3(0, 0.5, 0.5),
float3(0, 0.25, 0.75),
float3(0.5, 0.5, 0),
float3(0.25, 0.75, 0),
float3(0.25, 0.5, 0.25),
float3(0.25, 0.75, 0),
float3(0, 0.75, 0.25),
float3(0.25, 0.5, 0.25),
float3(0.25, 0.5, 0.25),
float3(0, 0.75, 0.25),
float3(0, 0.5, 0.5),
float3(0.25, 0.75, 0),
float3(0, 1, 0),
float3(0, 0.75, 0.25)
};
 VertexOutput a,b,c;
 a.position=mul(frame.transform,float4(0,-0.5,0,1));
 b.position=mul(frame.transform,float4(0.5,0.5,0,1));
 c.position=mul(frame.transform,float4(-0.5,0.5,0,1));
 a.color=b.color=c.color=float3(0,0,0);
 return evaluatePatch(a,b,c,barycentric[id]);
}
