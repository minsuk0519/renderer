#pragma once

#define GET_HLSL_LOC_SRV( x ) ( (x << 2) | 0)
#define GET_HLSL_LOC_SAMP( x ) ( (x << 2) | 1)
#define GET_HLSL_LOC_UAV( x ) ( (x << 2) | 2)
#define GET_HLSL_LOC_CBV( x ) ( (x << 2) | 3)

////////////////////Camera Buffer Begin
//t0 position
#define SRV_GBUFFER0_TEX	GET_HLSL_LOC_SRV(0)
//t1 normal
#define SRV_GBUFFER1_TEX	GET_HLSL_LOC_SRV(1)
//t2 ssao input
#define SRV_AO_FINAL		GET_HLSL_LOC_SRV(2)
//b0 projection
#define CBV_PROJECTION		GET_HLSL_LOC_CBV(0)
//b1 object
#define CBV_OBJECT			GET_HLSL_LOC_CBV(1)
//b2 screen
#define CBV_SCREEN			GET_HLSL_LOC_CBV(2)
//b3 guiDebug
#define CBV_GUIDEBUG		GET_HLSL_LOC_CBV(3)
////////////////////Camera Buffer Begin

////////////////////SSAO Buffer Begin
//u0 ssao output
#define UAV_SSAO			GET_HLSL_LOC_UAV(0)
//u1 ssao blur output
#define UAV_SSAOBLUR		GET_HLSL_LOC_UAV(1)
//b1 ssao constants
#define CBV_AOCONST			GET_HLSL_LOC_CBV(1)
////////////////////SSAO Buffer Ends

////////////////////Terrain Buffer Begin
//u0 vertex output
#define UAV_TERRAIN_VERT	GET_HLSL_LOC_UAV(0)
//u1 normal output
#define UAV_TERRAIN_NORM	GET_HLSL_LOC_UAV(1)
//u2 indices output
#define UAV_TERRAIN_INDEX	GET_HLSL_LOC_UAV(2)
//t0 position
#define SRV_TERRAIN_NOISE	GET_HLSL_LOC_SRV(0)
//b0 terrain constants
#define CBV_TERRAINCONST	GET_HLSL_LOC_CBV(0)
////////////////////Terrain Buffer Ends

////////////////////Noise Buffer Begin
//u0 noise output
#define UAV_NOISE			GET_HLSL_LOC_UAV(0)
//b0 noise constants
#define CBV_NOISECONST		GET_HLSL_LOC_CBV(0)
////////////////////Noise Buffer Ends

#define FEATURE_AO (1 << 0)

//define PSO indcies
namespace render
{
	enum PSO_INDEX
	{
		PSO_PBR,
		PSO_GBUFFER,
		PSO_WIREFRAME,
		PSO_SSAO,
		PSO_SSAOBLURX,
		PSO_SSAOBLURY,
		PSO_GENTERRAIN,
		PSO_GENNOISE,
		PSO_END,
	};
}

namespace consts
{
	constexpr uint CONST_OBJ_SIZE = sizeof(float) * (4 * 4 + 3 + 1 + 1);
	constexpr uint CONST_OBJ_SIZE_ALLIGNMENT = 96;
	constexpr uint CONST_PROJ_SIZE = sizeof(float) * (4 * 4 * 2 + 4);
}