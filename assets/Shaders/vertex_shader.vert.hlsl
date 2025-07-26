struct Input
{
    uint VertexIndex : SV_VertexID;
};

struct Output
{
    float4 Color : TEXCOORD0;
    float4 Position : SV_Position;
};

[[vk::binding(0, 1)]]
cbuffer UniformBlock : register(b0)
{
    float4x4 projection;
};

Output main(Input input)
{
    Output output;
    float2 pos;
    
    // Define triangle in normalized device coordinates (NDC)
    if (input.VertexIndex == 0)
    {
        pos = float2(-1.0f, -1.0f);
        output.Color = float4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    }
    else if (input.VertexIndex == 1)
    {
        pos = float2(1.0f, -1.0f);
        output.Color = float4(0.0f, 1.0f, 0.0f, 1.0f); // Green
    }
    else
    {
        pos = float2(0.0f, 1.0f);
        output.Color = float4(0.0f, 0.0f, 1.0f, 1.0f); // Blue
    }


    output.Position = mul(projection, float4(pos, 0.0f, 1.0f));

    return output;
}
