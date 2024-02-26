#pragma once

#define GET_HLSL_LOC_SRV( x ) ( (x << 2) | 0)
#define GET_HLSL_LOC_SAMP( x ) ( (x << 2) | 1)
#define GET_HLSL_LOC_UAV( x ) ( (x << 2) | 2)
#define GET_HLSL_LOC_CBV( x ) ( (x << 2) | 3)

//t0 position
#define SRV_GBUFFER0_TEX	GET_HLSL_LOC_SRV(0)
//t1 normal
#define SRV_GBUFFER1_TEX	GET_HLSL_LOC_SRV(1)
//b0 projection
#define CBV_PROJECTION		GET_HLSL_LOC_CBV(0)
//b1 object
#define SRV_OBJECT			GET_HLSL_LOC_CBV(1)
//u0 ssao
#define UAV_SSAO			GET_HLSL_LOC_UAV(0)

//define PSO indcies
namespace render
{
	enum PSO_INDEX
	{
		PSO_PBR,
		PSO_GBUFFER,
		PSO_WIREFRAME,
		PSO_SSAO,
		PSO_END,
	};
}

namespace consts
{
	constexpr uint CONST_OBJ_SIZE = sizeof(float) * (4 * 4 + 3 + 1 + 1);
	constexpr uint CONST_OBJ_SIZE_ALLIGNMENT = 96;
	constexpr uint CONST_PROJ_SIZE = sizeof(float) * (4 * 4 * 2 + 4);
}