//https://jcgt.org/published/0003/02/01/

//Fast lossy compression of 3D unit vector sets
//https://dl.acm.org/doi/10.1145/3145749.3149436

//should be arbitrary number that is less than 2^16
const uint quantization = 2 ^ 13;
const float inverseQuantization = 1 / (2 ^ 13);
const float PHI = 1.61803398875f;

//https://github.com/superboubek/UniQuant/blob/master/sfibonacci.h
//https://docplayer.net/40493580-Spherical-fibonacci-mapping.html
//https://jcgt.org/published/0009/04/02/paper.pdf
//https://perso.telecom-paristech.fr/boubek/papers/UVC/UVC.pdf

//we can get inversePHI by PHI - 1
const float inversePHI = PHI - 1;
const float squaredPHI = PHI + 1;

float3 unPackingSF(float i) 
{ 
	float phi = 2.0f * PI * frac(i * inversePHI);
	
	float cosTheta = 1 - (2 * i + 1) * inverseQuantization;
	float sinTheta = sqrt(saturate(1 - cosTheta * cosTheta));
	
	return float3(cos(phi)*sinTheta, sin(phi)*sinTheta, cosTheta);
}

//only unit vector
float packingSF(float3 v)
{
	float phi = min(atan2(v.y, v.x), PI);
	float cosTheta = v.z;

	float k = max(2.0f, floor(
		log(n * PI * sqrt(5.0f) * (1.0f - cosTheta*cosTheta)) / log(squaredPHI)));

	float Fk = pow(PHI, k)/sqrt(5.0f);
	float F0 = round(Fk);
	float F1 = round(Fk * PHI);

	float2x2 B = float2x2(
		2*PI*madfrac(F0+1, PHI-1) - 2*PI*(PHI-1),
		2*PI*madfrac(F1+1, PHI-1) - 2*PI*(PHI-1),
		-2*F0/n,
		-2*F1/n);
	float2x2 invB = inverse(B);
	float2 c = floor(
		mul(invB, float2(phi, cosTheta - (1-1/n))));

	float d = INFINITY, j = 0;
	for (uint s = 0; s < 4; ++s) {
		float cosTheta =
			dot(B[1], float2(s%2, s/2) + c) + (1-1/n);
		cosTheta = clamp(cosTheta, -1, +1)*2 - cosTheta;

		float i = floor(n*0.5 - cosTheta*n*0.5);
		float phi = 2*PI*madfrac(i, PHI-1);
		cosTheta = 1 - (2*i + 1)*rcp(n);
		float sinTheta = sqrt(1 - cosTheta*cosTheta);

		float3 q = float3(
			cos(phi)*sinTheta,
			sin(phi)*sinTheta,
			cosTheta);

		float squaredDistance = dot(q-p, q-p);
		if (squaredDistance < d) {
			d = squaredDistance;
			j = i;
		}
	}

	return j;
}