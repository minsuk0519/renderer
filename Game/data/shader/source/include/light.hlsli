cbuffer sunBuffer : register(b2)
{
    float3 sunDir;
}

cbuffer randomBuffer : register(b3)
{
	float4 randomPos[50];
	float randomNum;
}

Texture2D irradianceMap : register(t0);
Texture2D enviornmentMap : register(t1);

SamplerState samp : register(s0);

float calNormalDistribution_GGX(float NdotH, float roughness)
{	
	float roughnesssquare = roughness * roughness;
	float value = NdotH * NdotH * (roughnesssquare - 1.0) + 1.0;
	value = value * value * PI;
	
	return roughnesssquare / value;
}

float calOptimizedGeometry(float NdotV, float NdotL, float roughness)
{
	float alphaplusone = (roughness + 1.0);
    float k = (alphaplusone * alphaplusone) / 8.0;

    float viewG = NdotV / (NdotV * (1.0 - k) + k);
    float lightG = NdotL / (NdotL * (1.0 - k) + k);
	
	return viewG * lightG;
}

float3 calFresnel(float cosine, float3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosine, 5.0);
}

float3 calcImageBasedLight(float3 viewDir, float3 normal, float roughness, float metal, float3 albedo, float3 F0)
{
	float3 R = (2 * dot(normal, viewDir) * normal - viewDir);
	float3 A = normalize(float3(R.z, 0, -R.x));
	float3 B = normalize(cross(R, A));
	
	uint2 texSize;
	enviornmentMap.GetDimensions(texSize.x, texSize.y);
	float level = 0.5 * log2(texSize.x*texSize.y/randomNum);
	
	float NdotV = dot(normal, viewDir);
	float3 specularcolor = float3(0,0,0);
	
	//for(int i = 0; i < randomNum; ++i)
	{
		float x = 0;//(i % 2 == 0) ? randomPos[i].x : randomPos[i].z;
		float y = 0;//(i % 2 == 0) ? randomPos[i].y : randomPos[i].w;

		float theta = atan(roughness * sqrt(y) / sqrt(1.0 - y));
		float2 uv = float2(x, theta / PI);
		
		float3 L = EquirectangularToSpherical(uv);
		float3 wk = normalize(L.x * A + L.y * B + L.z * R);
		
		float3 halfway = normalize(viewDir + wk);
		
		float NdotH =  dot(normal, halfway);
		float wkdotH = dot(wk, halfway);
		float wkdotN = dot(wk, normal);
		
		//GGX
		float DH = calNormalDistribution_GGX(NdotH, roughness);
	
		float lod = level - 0.5 * log2(DH);
		if(DH <= 0) lod = 0;
		
		float3 environmentValue = enviornmentMap.SampleLevel(samp, SphericalToEquirectangular(wk), lod).xyz;
		//float3 environmentValue = enviornmentMap.Sample(samp, SphericalToEquirectangular(wk)).xyz;
				    
		float3 specular = environmentValue * cos(theta);
		
		float denom = 4 * wkdotN * NdotV;
		float3 FG = (calOptimizedGeometry(NdotV, wkdotN, roughness) * calFresnel(NdotV, F0));
		
		specularcolor += specular * FG / denom;
	}
	specularcolor = specularcolor;// / randomNum;
	
	float3 kS = calFresnel(NdotV, F0);
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - metal;

	float3 irradiance = irradianceMap.Sample(samp, SphericalToEquirectangular(normal)).xyz;
				
	return specularcolor + kD * albedo * (1 / PI) * irradiance;
}

float3 calcLight(float3 lightDir, float3 viewDir, float3 normal, float3 albedo, float3 lightColor, float roughness, float metal, float3 F0)
{
	float3 halfway = normalize(viewDir + lightDir);
	
	float NdotV = max(dot(normal, viewDir), 0.0);
	float NdotL = max(dot(normal, lightDir), 0.0);
	float NdotH = max(dot(normal, halfway), 0.0);

	float D = calNormalDistribution_GGX(NdotH, roughness);
	float G = calOptimizedGeometry(NdotV, NdotL, roughness);      
	float3 F = calFresnel(NdotH, F0);		   
	float3 specular = D * G * F / (4.0 * NdotV * NdotL + 0.0001);
	
	float3 kS = F;
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - metal;

	return (kD * albedo / PI + specular) * NdotL * lightColor;
}