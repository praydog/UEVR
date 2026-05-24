#define SpriteStaticRS \
"RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"            DENY_HULL_SHADER_ROOT_ACCESS )," \
"DescriptorTable ( SRV(t0), visibility = SHADER_VISIBILITY_PIXEL ),"\
"CBV(b0), "\
"StaticSampler(s0,"\
"           filter = FILTER_MIN_MAG_MIP_LINEAR,"\
"           addressU = TEXTURE_ADDRESS_CLAMP,"\
"           addressV = TEXTURE_ADDRESS_CLAMP,"\
"           addressW = TEXTURE_ADDRESS_CLAMP,"\
"           visibility = SHADER_VISIBILITY_PIXEL )"

#define SpriteHeapRS \
"RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"            DENY_HULL_SHADER_ROOT_ACCESS )," \
"DescriptorTable ( SRV(t0), visibility = SHADER_VISIBILITY_PIXEL ),"\
"CBV(b0), " \
"DescriptorTable ( Sampler(s0), visibility = SHADER_VISIBILITY_PIXEL )"

Texture2D<float4> Texture : register(t0);
sampler TextureSampler : register(s0);


cbuffer Parameters : register(b0)
{
    row_major float4x4 MatrixTransform;
};

[RootSignature(SpriteStaticRS)]
void SpriteVertexShader(inout float4 color    : COLOR0,
    inout float2 texCoord : TEXCOORD0,
    inout float4 position : SV_Position)
{
    position = mul(position, MatrixTransform);
}

float4 SpritePixelShaderUnified(float4 color    : COLOR0,
    float2 texCoord : TEXCOORD0)
{
   float4 tex = Texture.Sample(TextureSampler, texCoord);
   float invertAmount = saturate(color.a);
   float blendedAlpha = lerp(tex.a, 1.0 - tex.a, invertAmount);
   float3 blendedColor = tex.rgb * color.rgb;

   return float4(blendedColor, blendedAlpha);
}

[RootSignature(SpriteStaticRS)]
float4 SpritePixelShader(float4 color    : COLOR0,
    float2 texCoord : TEXCOORD0) : SV_Target0
{
   return SpritePixelShaderUnified(color, texCoord);
}

[RootSignature(SpriteHeapRS)]
void SpriteVertexShaderHeap(inout float4 color    : COLOR0,
    inout float2 texCoord : TEXCOORD0,
    inout float4 position : SV_Position)
{
    position = mul(position, MatrixTransform);
}

[RootSignature(SpriteHeapRS)]
float4 SpritePixelShaderHeap(float4 color    : COLOR0,
    float2 texCoord : TEXCOORD0) : SV_Target0
{
   return SpritePixelShaderUnified(color, texCoord);
}
