#define 	PACKING_BIT_SIZE 	32
#define 	HALF_SIZE			( PACKING_BIT_SIZE / 2 )

float2 signNotZero(in float2 v) 
{
    return float2((v.x >= 0.0f) ? 1.0f : -1.0f, (v.y >= 0.0f) ? 1.0f : -1.0f);
}

//convert octahedral coordinate to cartesian coordinate
float3 octToCart(float2 oct) 
{
    float3 v = float3(oct, 1.0 - abs(oct.x) - abs(oct.y));
	
    if (v.z < 0) v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
	
    return normalize(v);
}

//convert cartesian coordiante to ocatahedral coordinate
float2 cartToOct(in float3 v) 
{
    float sum = abs(v.x) + abs(v.y) + abs(v.z);
    float2 oct = v.xy * (1.0 / sum);
	
    return (v.z <= 0.0f) ? ((1.0f - abs(oct.yx)) * signNotZero(oct.xy)) : oct;
}

//decode 32 bits uint to original float3
float3 decodeOct(in uint p)
{
    uint2 data;
	
    data.x = p >> HALF_SIZE;
    data.y = p & ((1 << HALF_SIZE) - 1);
	
	float2 oct;
	
	//convert uint to float with range [-1, 1]
    oct.x = clamp((float(data.x) / float(32767)) - 1.0, -1.0, 1.0);
    oct.y = clamp((float(data.y) / float(32767)) - 1.0, -1.0, 1.0);
	
    return octToCart(oct);	
}

//encode float3 into 32 bits uint
uint encodeOct(float3 v)
{
    float3 normv = normalize(v);
    float2 oct = cartToOct(v);
	
	float x = floor(clamp(oct.x + 1.0, 0.0, 2.0) * float(32767));
	float y = floor(clamp(oct.y + 1.0, 0.0, 2.0) * float(32767));

    uint optimal;

    float closeness = 0.0;

    float3 trialResult = float3(0,0,0);

    for (int i = 0; i < 2; ++i) 
	{
        for (int j = 0; j < 2; ++j) 
		{
			uint trial = ((int(x) + i) << HALF_SIZE) + (int(y) + j);
			
			uint x_int = trial >> HALF_SIZE;
			uint y_int = trial & ((1 << HALF_SIZE) - 1);
			float x = clamp((float(x_int) / float(32767)) - 1.0, -1.0, 1.0);
			float y = clamp((float(y_int) / float(32767)) - 1.0, -1.0, 1.0);
			trialResult = octToCart(float2(x, y));
			
			float trialCloseness = dot(trialResult, normv);
						
			if(closeness < trialCloseness) 
			{
				closeness = trialCloseness;
				optimal = trial;
			}
        }
    }
	
    return optimal;
}