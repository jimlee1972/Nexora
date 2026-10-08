import Nexora.Common;
struct ToneParameters { float4 settings; float4 bloom; float4 focus; float4 antiAlias; float4 occlusion; }; // x exposure; y manual output transfer
#ifdef NEXORA_VULKAN
[[vk::push_constant]] ConstantBuffer<ToneParameters> tone;
#else
[[vk::binding(0, 5)]] ConstantBuffer<ToneParameters> tone : register(b0);
#endif
[[vk::binding(0, 0)]] Sampler2D<float4> hdrScene : register(t0);
struct ToneVertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
[shader("vertex")]
ToneVertex toneVertexMain(
  [[vk::location(0)]] float2 position : POSITION,
  [[vk::location(1)]] float2 uv : TEXCOORD0) {
  ToneVertex output;
  output.position = float4(position, 0, 1);
#ifdef NEXORA_VULKAN
  output.uv = float2(uv.x, 1 - uv.y);
#else
  output.uv = uv;
#endif
  return output;
}
float displayLuma(float3 hdr) {
  return dot(NexoraAcesApproximate(hdr * tone.settings.x), float3(0.299, 0.587, 0.114));
}
float3 filterEdges(float2 uv, float3 center) {
  float2 pixel = tone.antiAlias.xy;
  float3 nw = hdrScene.SampleLevel(uv + float2(-1,-1) * pixel, 0).rgb;
  float3 ne = hdrScene.SampleLevel(uv + float2(1,-1) * pixel, 0).rgb;
  float3 sw = hdrScene.SampleLevel(uv + float2(-1,1) * pixel, 0).rgb;
  float3 se = hdrScene.SampleLevel(uv + float2(1,1) * pixel, 0).rgb;
  float lnw=displayLuma(nw), lne=displayLuma(ne), lsw=displayLuma(sw), lse=displayLuma(se);
  float lm=displayLuma(center);
  float low=min(lm,min(min(lnw,lne),min(lsw,lse)));
  float high=max(lm,max(max(lnw,lne),max(lsw,lse)));
  if (high-low <= max(0.0312, high*0.125)) return center;
  float2 direction=float2(-((lnw+lne)-(lsw+lse)),(lnw+lsw)-(lne+lse));
  float reduction=max((lnw+lne+lsw+lse)*0.03125,0.0078125);
  direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduction),float2(-8),float2(8))*pixel;
  float3 a=0.5*(hdrScene.SampleLevel(uv+direction*(-1.0/6.0),0).rgb+
                hdrScene.SampleLevel(uv+direction*(1.0/6.0),0).rgb);
  float3 b=0.5*a+0.25*(hdrScene.SampleLevel(uv+direction*(-0.5),0).rgb+
                       hdrScene.SampleLevel(uv+direction*0.5,0).rgb);
  float lb=displayLuma(b);
  return lb<low || lb>high ? a : b;
}
// Reconstruct perspective positions from radial camera distance; the descriptor owns the FOV.
float3 contactPosition(float2 uv, float distance) {
  float aspect = tone.antiAlias.y / tone.antiAlias.x;
  float3 ray = float3((uv.x * 2 - 1) * aspect * tone.occlusion.w, (1 - uv.y * 2) * tone.occlusion.w, -1);
  return normalize(ray) * distance;
}
float contactVisibility(float2 uv, float distance) {
  // RGBA16F radial distance loses contact precision at large distances.
  if (distance <= 0 || distance >= 64) return 1;
  float3 center = contactPosition(uv, distance);
  float2 pixel = tone.antiAlias.xy;
  float3 left = contactPosition(uv - float2(pixel.x,0), hdrScene.SampleLevel(uv - float2(pixel.x,0),0).a);
  float3 right = contactPosition(uv + float2(pixel.x,0), hdrScene.SampleLevel(uv + float2(pixel.x,0),0).a);
  float3 up = contactPosition(uv - float2(0,pixel.y), hdrScene.SampleLevel(uv - float2(0,pixel.y),0).a);
  float3 down = contactPosition(uv + float2(0,pixel.y), hdrScene.SampleLevel(uv + float2(0,pixel.y),0).a);
  float3 dx = abs(left.z-center.z) < abs(right.z-center.z) ? center-left : right-center;
  float3 dy = abs(up.z-center.z) < abs(down.z-center.z) ? center-up : down-center;
  float3 normal = cross(dx,dy);
  if (dot(normal,normal) < 1e-10) return 1;
  normal = normalize(normal);
  if (dot(normal,-center) < 0) normal = -normal;
  const float2 directions[6] = {float2(1,0),float2(0.5,0.866025),float2(-0.5,0.866025),
    float2(-1,0),float2(-0.5,-0.866025),float2(0.5,-0.866025)};
  float radius = tone.occlusion.y;
  float2 screenRadius = float2(pixel.x / pixel.y,1) * radius / (2 * max(abs(center.z),0.1) * tone.occlusion.w);
  screenRadius = min(screenRadius, pixel * 32);
  float blocked = 0;
  [unroll] for (uint i=0;i<12;++i) {
    float2 sampleUv = uv + directions[i%6] * screenRadius * (i<6 ? 0.5 : 1);
    // Keep one induction update so legacy native HLSL compilers can unroll the fixed kernel.
    if (all(sampleUv > 0) && all(sampleUv < 1)) {
      float tapDistance = hdrScene.SampleLevel(sampleUv,0).a;
      if (tapDistance > 0) {
        float3 delta = contactPosition(sampleUv,tapDistance) - center;
        float lengthSquared = dot(delta,delta);
        if (lengthSquared > 0.000001 && lengthSquared < radius*radius) {
          float angle = saturate((dot(normal,delta)-tone.occlusion.z) / sqrt(lengthSquared));
          blocked += angle * (1-lengthSquared/(radius*radius));
        }
      }
    }
  }
  return saturate(1 - tone.occlusion.x * (1 - smoothstep(32.0,64.0,distance)) * blocked / 6);
}
[shader("fragment")]
float4 toneFragmentMain(ToneVertex input) : SV_Target {
  float4 center = hdrScene.SampleLevel(input.uv, 0);
  float3 hdr = tone.antiAlias.z > 0.5 ? filterEdges(input.uv, center.rgb) : center.rgb;
  if (tone.focus.y > 0 && center.a > 0) {
    float coc = saturate(abs(center.a - tone.focus.x) / max(center.a, 0.001) * tone.focus.y);
    if (coc > 0.05) {
      const float2 offsets[12] = {float2(-1,0),float2(1,0),float2(0,-1),float2(0,1),
        float2(-0.707,-0.707),float2(0.707,-0.707),float2(-0.707,0.707),float2(0.707,0.707),
        float2(-0.35,-0.35),float2(0.35,-0.35),float2(-0.35,0.35),float2(0.35,0.35)};
      float3 sum = hdr;
      float weights = 1;
      [unroll] for (uint i=0; i<12; ++i) {
        float4 tap = hdrScene.SampleLevel(input.uv + offsets[i] * tone.focus.zw * coc, 0);
        // Reject different depth layers to limit foreground/background color bleeding.
        float weight = saturate(1 - abs(tap.a - center.a) / max(1.0, center.a * 0.25));
        sum += tap.rgb * weight;
        weights += weight;
      }
      hdr = sum / weights;
    }
  }
  if (tone.occlusion.x > 0) {
    // Preserve radiance above the bloom threshold: contact shading is an HDR composition
    // approximation, not a replacement for per-material ambient lighting or ray tracing.
    float preserveHighlight = saturate(max(center.r,max(center.g,center.b)) - 1);
    hdr *= lerp(contactVisibility(input.uv, center.a), 1, preserveHighlight);
  }
  if (tone.bloom.x > 0) {
    float2 texel = tone.bloom.zw; // Radius normalized by the acquired physical extent.
    // Fixed bounded twelve-tap two-scale bloom. No temporal history or CPU readback.
    const float2 offsets[12] = {float2(-1,0),float2(1,0),float2(0,-1),float2(0,1),
      float2(-0.5,-0.5),float2(0.5,-0.5),float2(-0.5,0.5),float2(0.5,0.5),
      float2(-0.25,0),float2(0.25,0),float2(0,-0.25),float2(0,0.25)};
    float3 glow = float3(0,0,0);
    [unroll] for (uint i=0; i<12; ++i)
      glow += NexoraExtractBloom(hdrScene.SampleLevel(input.uv + offsets[i] * texel, 0).rgb,
                                  tone.bloom.y) / 12.0;
    hdr = NexoraApplyBloom(hdr, glow, tone.bloom.x);
  }
  float3 color = NexoraAcesApproximate(hdr * tone.settings.x);
  color = saturate(NexoraApplyColorGrade(color, float3(0), float3(1), tone.settings.z, tone.settings.w));
  if (tone.settings.y > 0.5) color = NexoraLinearToSrgb(color);
  return float4(color, 1);
}
