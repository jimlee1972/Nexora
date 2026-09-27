implementing "Nexora.Common";

public static const uint NEXORA_FEATURE_NORMAL_MAP = 1U << 0U;
public static const uint NEXORA_FEATURE_EMISSION = 1U << 1U;
public static const uint NEXORA_FEATURE_ALPHA_TEST = 1U << 2U;
public static const uint NEXORA_FEATURE_SHADOW = 1U << 3U;
public static const uint NEXORA_FEATURE_IBL = 1U << 4U;
public static const uint NEXORA_FEATURE_FOG = 1U << 5U;
public static const uint NEXORA_FEATURE_PBR = 1U << 6U;
public static const uint NEXORA_FEATURE_STYLIZED_PBR = 1U << 7U;
public static const uint NEXORA_FEATURE_ANIME = 1U << 8U;
public static const uint NEXORA_FEATURE_VEGETATION = 1U << 9U;
public static const uint NEXORA_FEATURE_WATER = 1U << 10U;
public static const uint NEXORA_FEATURE_UNLIT = 1U << 11U;

public uint NexoraMakeShaderVariantKey(uint shaderId, uint backend, uint qualityTier,
                                       uint bindingTier, uint featureMask)
{
  uint key = 2166136261U;
  key = (key ^ shaderId) * 16777619U;
  key = (key ^ backend) * 16777619U;
  key = (key ^ qualityTier) * 16777619U;
  key = (key ^ bindingTier) * 16777619U;
  return (key ^ featureMask) * 16777619U;
}
