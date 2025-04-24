#pragma once

#include "system/defines.hpp"

#define GET_HLSL_LOC_SRV( x ) ( (x << 2) | 0)
#define GET_HLSL_LOC_SAMP( x ) ( (x << 2) | 1)
#define GET_HLSL_LOC_UAV( x ) ( (x << 2) | 2)
#define GET_HLSL_LOC_CBV( x ) ( (x << 2) | 3)

////////////////////gbuffer Buffer Begin
//t0 position
#define SRV_GBUFFER_VERTEX			GET_HLSL_LOC_SRV(0)
//t1 normal
#define SRV_GBUFFER_NORM			GET_HLSL_LOC_SRV(1)
//t2 cluster args
#define SRV_GBUFFER_CLUSTERARGS		GET_HLSL_LOC_SRV(2)
//t3 view info
#define SRV_GBUFFER_VIEWINFO		GET_HLSL_LOC_SRV(3)
//t4 mesh info
#define SRV_GBUFFER_MESHINFO		GET_HLSL_LOC_SRV(4)
//b0 projection
#define CBV_PROJECTION				GET_HLSL_LOC_CBV(0)
//b1 object
#define CBV_OBJECT					GET_HLSL_LOC_CBV(1)
//b2 screen
#define CBV_SCREEN					GET_HLSL_LOC_CBV(2)
//b3 guiDebug
#define CBV_GUIDEBUG				GET_HLSL_LOC_CBV(3)
////////////////////gbuffer Buffer Begin

////////////////////light Buffer Begin
//t0 position
#define SRV_LIGHT_POSITION		GET_HLSL_LOC_SRV(0)
//t1 normal gbuffer
#define SRV_LIGHT_NORM			GET_HLSL_LOC_SRV(1)
//t2 debug gbuffer
#define SRV_LIGHT_DEBUG			GET_HLSL_LOC_SRV(2)
//t3 view info
#define SRV_LIGHT_AO			GET_HLSL_LOC_SRV(3)
////////////////////light Buffer Begin

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
//u2 normal output
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

////////////////////Unified Buffer Begin
//u0 uvb buffer output
#define UAV_UNIFIED_VERTEX_BUFFER	GET_HLSL_LOC_UAV(0)
//u1 uib buffer output
#define UAV_UNIFIED_INDDEX_BUFFER	GET_HLSL_LOC_UAV(1)
//t0 vertex buffer input
#define SRV_VERTEX_BUFFER			GET_HLSL_LOC_SRV(0)
//t1 index buffer input
#define SRV_INDEX_BUFFER			GET_HLSL_LOC_SRV(1)
//t2 normal buffer input
#define SRV_NORMAL_BUFFER			GET_HLSL_LOC_SRV(2)
//t3 meshInfo buffer input
#define SRV_UNIFIED_MESHINFO_BUFFER	GET_HLSL_LOC_SRV(3)
//b0 noise constants
#define CBV_UNIFIEDCONSTS			GET_HLSL_LOC_CBV(0)
////////////////////Unified Buffer Ends

////////////////////cmdBuf Buffer Begin
//u0 uvb buffer output
#define UAV_CMD_BUFFER				GET_HLSL_LOC_UAV(0)
//u1 vertexID buffer output
#define UAV_CMD_VERTEXID_BUFFER		GET_HLSL_LOC_UAV(1)
//t0 mesh info buffer input
#define SRV_MESH_INFO_BUFFER		GET_HLSL_LOC_SRV(0)
//t1 lod info buffer input
#define SRV_LOD_INFO_BUFFER			GET_HLSL_LOC_SRV(1)
//t2 cluster info buffer input
#define SRV_CLUSTER_INFO_BUFFER		GET_HLSL_LOC_SRV(2)
//t3 vertex ID Offsets buffer input
#define SRV_VERTEX_ID_OFFSET		GET_HLSL_LOC_SRV(3)
//b0 cmdBuf constants
#define CBV_CMDBUFCONSTS			GET_HLSL_LOC_CBV(0)
////////////////////cmdBuf Buffer Ends

////////////////////culling Buffer Begin
//u0 cmd buffer buffer output
#define UAV_CULLING_CMD_BUFFER				GET_HLSL_LOC_UAV(0)
//u1 cluster args buffer output
#define UAV_CLUSTERARGS_BUFFER				GET_HLSL_LOC_UAV(1)
//u2 cluster offset buffer output
#define UAV_CLUSTEROFFSET_BUFFER			GET_HLSL_LOC_UAV(2)
//u3 cluster size buffer output
#define UAV_CLUSTERSIZE_BUFFER				GET_HLSL_LOC_UAV(3)
//t0 mesh info buffer input
#define SRV_CULLING_MESH_INFO_BUFFER		GET_HLSL_LOC_SRV(0)
//t1 lod info buffer input
#define SRV_CULLING_LOD_INFO_BUFFER			GET_HLSL_LOC_SRV(1)
//t2 cluster info buffer input
#define SRV_CULLING_CLUSTER_INFO_BUFFER		GET_HLSL_LOC_SRV(2)
//t3 cluster args buffer input
#define SRV_CLUSTER_ARGS_BUFFER				GET_HLSL_LOC_SRV(3)
//t4 cluster bounds input
#define SRV_CLUSTER_BOUNDS_BUFFER			GET_HLSL_LOC_SRV(4)
//t5 view infos input
#define SRV_VIEW_INFOS_BUFFER				GET_HLSL_LOC_SRV(5)
//b0 cmdBuf constants
#define CBV_CULLINGCONSTS					GET_HLSL_LOC_CBV(0)
//b0 projection
#define CBV_CULLING_PROJECTION				GET_HLSL_LOC_CBV(1)
////////////////////culling Buffer Ends

////////////////////utils Buffer Begin
//u0 util src buffer output
#define UAV_UTILS_DST_BUFFER			GET_HLSL_LOC_UAV(0)
//t0 util dst buffer input
#define SRV_UTILS_SRC_BUFFER			GET_HLSL_LOC_SRV(0)
//b0 cmdBuf constants
#define CBV_UTILSCONSTS					GET_HLSL_LOC_CBV(0)
////////////////////utils Buffer Ends

////////////////////global buffer Begin
//u63 debug buffer
#define UAV_GLOBAL_DEBUG_BUFFER			GET_HLSL_LOC_UAV(63)
////////////////////global buffer Ends


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
		PSO_GENUNIFIED,
		PSO_GENCMDBUF,
		PSO_GBUFFERINDIRECT,
		PSO_AABBDEBUGDRAW,
		PSO_GENTERRAININDEX,
		PSO_UPLOADLOCALOBJ,
		PSO_CULLCLUSTER,
		PSO_INITCLUSTER,
		PSO_END,
	};
}

namespace consts
{
	constexpr uint CONST_OBJ_SIZE = sizeof(float) * (4 * 4 + 3 + 1 + 1);
	constexpr uint CONST_OBJ_SIZE_ALLIGNMENT = 96;
	constexpr uint CONST_PROJ_SIZE = sizeof(float) * (4 * 4 + 4);
}

//define flags
#define MESH_INFO_FLAGS_TERRAIN		(1U << 0)
#define MESH_INFO_FLAGS_NONORM		(1U << 1)