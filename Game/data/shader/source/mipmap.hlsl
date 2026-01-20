Texture2D<float> src : register(t0);
RWTexture2D<float> dst : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    float d1 = src.Load(int3(dtid.xy * 2 + int2(0, 0), 0));
    float d2 = src.Load(int3(dtid.xy * 2 + int2(1, 0), 0));
    float d3 = src.Load(int3(dtid.xy * 2 + int2(0, 1), 0));
    float d4 = src.Load(int3(dtid.xy * 2 + int2(1, 1), 0));
    
    float result = max(max(d1, d2), max(d3, d4));
    
    dst[dtid.xy] = result;
}