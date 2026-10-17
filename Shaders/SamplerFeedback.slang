// Bounded metadata-only footprint recipe; feedback does not read paired texture color data.
Texture2D<float4> sampled : register(t0);
SamplerState pointClamp : register(s0);
FeedbackTexture2D<SAMPLER_FEEDBACK_MIN_MIP> feedback : register(u0);
struct Request {float2 uv;uint mip;uint enabled;};
StructuredBuffer<Request> requests : register(t1);
cbuffer RequestCount : register(b0) {uint count;uint3 reserved;};
[numthreads(1,1,1)]
void writeFeedback(uint3 id:SV_DispatchThreadID) {
 if(id.x>=count || count>64) return;
 Request input=requests[id.x];
 if(input.enabled!=1 || input.mip>2 || !all(isfinite(input.uv)) || any(input.uv<0) || any(input.uv>=1))return;
 feedback.WriteSamplerFeedbackLevel(sampled,pointClamp,input.uv,float(input.mip));
}
