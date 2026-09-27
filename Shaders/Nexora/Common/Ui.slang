implementing "Nexora.Common";

public struct NexoraUiVertex
{
  public float2 position;
  public float2 uv;
  public float4 color;
};

public struct NexoraUiFrame
{
  public float2 scale;
  public float2 translate;
  public float4 clipRect;
};

public float4 NexoraTransformUiPosition(float2 position, float2 scale, float2 translate)
{
  return float4(position * scale + translate, 0.0, 1.0);
}

public float NexoraEvaluateUiClip(float2 position, float4 clipRect, float feather)
{
  float width = max(feather, 1.0e-4);
  float2 insideMin = smoothstep(clipRect.xy, clipRect.xy + width.xx, position);
  float2 insideMax = 1.0 - smoothstep(clipRect.zw - width.xx, clipRect.zw, position);
  return insideMin.x * insideMin.y * insideMax.x * insideMax.y;
}

public float2 NexoraEvaluateNineSliceUv(float2 uv, float2 rectSize, float2 textureSize,
                                        float4 borders)
{
  float2 safeTextureSize = max(textureSize, 1.0.xx);
  float2 textureBorderMin = borders.xy / safeTextureSize;
  float2 textureBorderMax = borders.zw / safeTextureSize;
  float2 destinationBorderMin = saturate(borders.xy / max(rectSize, 1.0.xx));
  float2 destinationBorderMax = saturate(borders.zw / max(rectSize, 1.0.xx));
  float2 centerDestinationSize = max(1.0.xx - destinationBorderMin - destinationBorderMax,
                                     1.0e-4.xx);
  float2 centerSourceSize = max(1.0.xx - textureBorderMin - textureBorderMax, 1.0e-4.xx);
  float2 leftUv = uv / max(destinationBorderMin, 1.0e-4.xx) * textureBorderMin;
  float2 centerUv = textureBorderMin +
                    saturate((uv - destinationBorderMin) / centerDestinationSize) * centerSourceSize;
  float2 rightUv = 1.0.xx - textureBorderMax +
                   saturate((uv - (1.0.xx - destinationBorderMax)) /
                            max(destinationBorderMax, 1.0e-4.xx)) * textureBorderMax;
  float2 rightRegion = step(1.0.xx - destinationBorderMax, uv);
  float2 centerRegion = step(destinationBorderMin, uv) * (1.0.xx - rightRegion);
  return lerp(leftUv, lerp(centerUv, rightUv, rightRegion), centerRegion);
}

public float4 NexoraCompositeUiColor(float4 vertexColor, float4 textureColor, float opacity)
{
  float4 color = vertexColor * textureColor;
  color.a *= saturate(opacity);
  return color;
}

public float4 NexoraTransformWorldUiPosition(float2 localPosition, float4x4 worldViewProjection)
{
  return mul(worldViewProjection, float4(localPosition, 0.0, 1.0));
}
