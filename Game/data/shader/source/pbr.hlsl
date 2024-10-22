#include "include\common.hlsli"
#include "include\packing.hlsli"

struct PSInput
{
    float4 position 	: SV_POSITION;
    float2 texCoord 	: TEXTURE_COORD;
};

Texture2D positionGbuffer 			: register(t0);
Texture2D<uint> normalTexGbuffer 	: register(t1);

Texture2D aoTexBuffer 		: register(t2);

SamplerState samp 			: register(s0);

#define FEATURE_AO (1 << 0)

cbuffer cb_debugInfo : register(b3)
{
	uint features = FEATURE_AO;
	uint debugDraw = 0;
}

PSInput pbr_vs(float2 position : VPOSITION)
{
    PSInput result;

    result.position = float4(position, 0.0f, 1.0f);
    result.texCoord = float2(position.x, -position.y);

    return result;
}

float4 pbr_ps(PSInput input) : SV_TARGET
{
	float2 uv = (input.texCoord.xy + float2(1.0f, 1.0f)) * 0.5f;
	uint2 texPos = uint2(uv.x * screenWidth, uv.y * screenHeight);	
	float3 position = positionGbuffer.Sample(samp, uv).xyz;
	float3 normal = decodeOct((normalTexGbuffer[texPos].x));
	float ao = aoTexBuffer.Sample(samp, uv).x;
		
	if(debugDraw == 1)
	{
		return float4(position, 1.0f);
	}
	else if(debugDraw == 2)
	{
		return float4(normal, 1.0f);
	}
	else if(debugDraw == 3)
	{
		return float4(ao, ao, ao, 1.0f);
	}
	
	if(length(normal) == 0.0f)
	{
		discard;
	}
	
	normal = normalize(normal);
    
	float3 viewDir = normalize(proj.camPos - position);
	
	float ambientStrength = 0.1;
    float3 ambient = ambientStrength * float3(1,1,1);
  	
    // diffuse 
    float3 lightDir = normalize(float3(-0.5f,-1.0f,-0.2f));
    float diff = max(dot(normal, -lightDir), 0.0);
    float3 diffuse = diff;
    
    // specular
    float specularStrength = 0.5;
    float3 reflectDir = reflect(-lightDir, normal);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    float3 specular = specularStrength * spec;  
       
	if(!(features & FEATURE_AO))
	{
		ao = 1.0f;
	}
	
    float3 result = ((ambient + diffuse) * ao + specular);
	
    return float4(result, 1.0f);
}