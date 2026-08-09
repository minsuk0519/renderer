#pragma once

#include "system/defines.hpp"

#define GET_HLSL_LOC_SRV( x ) ( (x << 2) | 0)
#define GET_HLSL_LOC_SAMP( x ) ( (x << 2) | 1)
#define GET_HLSL_LOC_UAV( x ) ( (x << 2) | 2)
#define GET_HLSL_LOC_CBV( x ) ( (x << 2) | 3)



////////////////////global Buffer Begin
//u63 debug buffer
#define UAV_GLOBAL_DEBUG_BUFFER		GET_HLSL_LOC_UAV(63)

//t63 position
#define SRV_VERTEX_BUFFER			GET_HLSL_LOC_SRV(63)
//t62 index
#define SRV_INDEX_BUFFER			GET_HLSL_LOC_SRV(62)
//t61 mesh info
#define SRV_MESHINFO_BUFFER			GET_HLSL_LOC_SRV(61)
//t60 lod info
#define SRV_LOD_INFO_BUFFER			GET_HLSL_LOC_SRV(60)
//t59 cluster info
#define SRV_CLUSTER_INFO_BUFFER		GET_HLSL_LOC_SRV(59)
//t58 cluster bounds
#define SRV_CLUSTER_BOUNDS_BUFFER	GET_HLSL_LOC_SRV(58)
//t57 view info
#define SRV_VIEWINFO_BUFFER			GET_HLSL_LOC_SRV(57)
//t56 material
#define SRV_MATERIAL_BUFFER			GET_HLSL_LOC_SRV(56)

//b63 cmdBuf constants
#define CBV_CMDBUFCONSTS			GET_HLSL_LOC_CBV(63)
//b62 projection
#define CBV_PROJECTION				GET_HLSL_LOC_CBV(62)
//b61 object
#define CBV_OBJECT					GET_HLSL_LOC_CBV(61)
//b59 screen
#define CBV_SCREEN					GET_HLSL_LOC_CBV(59)
////////////////////global Buffer End

////////////////////gbuffer Buffer Begin
//t0 cluster args
#define SRV_GBUFFER_CLUSTERARGS		GET_HLSL_LOC_SRV(0)
//t1 visible triangles
#define SRV_GBUFFER_VISIBLE_TRIS		GET_HLSL_LOC_SRV(1)

//b3 guiDebug
#define CBV_GUIDEBUG				GET_HLSL_LOC_CBV(3)
////////////////////gbuffer Buffer Begin

////////////////////light Buffer Begin
//t0 position
#define SRV_LIGHT_POSITION		GET_HLSL_LOC_SRV(0)
//t1 normal gbuffer
#define SRV_LIGHT_NORM			GET_HLSL_LOC_SRV(1)
//t2 object ID buffer
#define SRV_LIGHT_OBJID			GET_HLSL_LOC_SRV(2)
//t4 debug gbuffer
#define SRV_LIGHT_DEBUG			GET_HLSL_LOC_SRV(3)
//t5 view info
#define SRV_LIGHT_AO			GET_HLSL_LOC_SRV(4)
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
#define UAV_UNIFIED_INDEX_BUFFER	GET_HLSL_LOC_UAV(1)
//b0 noise constants
#define CBV_UNIFIEDCONSTS			GET_HLSL_LOC_CBV(0)
////////////////////Unified Buffer Ends

////////////////////cmdBuf Buffer Begin
//u0 uvb buffer output
#define UAV_CMD_BUFFER				GET_HLSL_LOC_UAV(0)
//u1 vertexID buffer output
#define UAV_CMD_VERTEXID_BUFFER		GET_HLSL_LOC_UAV(1)
//t3 vertex ID Offsets buffer input
#define SRV_VERTEX_ID_OFFSET		GET_HLSL_LOC_SRV(3)
//b0 cmdBuf constants
//#define CBV_CMDBUFCONSTS			GET_HLSL_LOC_CBV(0)
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
//t0 cluster args buffer input
#define SRV_CLUSTER_ARGS_BUFFER				GET_HLSL_LOC_SRV(0)
//u5 visible triangle buffer output
#define UAV_VISIBLE_TRI_BUFFER				GET_HLSL_LOC_UAV(5)
//t1 hzb (previous frame depth pyramid)
#define SRV_CULLING_HZB						GET_HLSL_LOC_SRV(1)
//b0 hzb cull constants
#define CBV_CULLING_HZBCONST				GET_HLSL_LOC_CBV(0)
//u6 occluded cluster list output
#define UAV_OCCLUDED_CLUSTERS				GET_HLSL_LOC_UAV(6)
////////////////////culling Buffer Ends

////////////////////utils Buffer Begin
//u0 util src buffer output
#define UAV_UTILS_DST_BUFFER			GET_HLSL_LOC_UAV(0)
//t0 util dst buffer input
#define SRV_UTILS_SRC_BUFFER			GET_HLSL_LOC_SRV(0)
//b0 cmdBuf constants
#define CBV_UTILSCONSTS					GET_HLSL_LOC_CBV(0)
////////////////////utils Buffer Ends

////////////////////HZB Buffer Begin
//t0 src mip
#define SRV_HZB_SRC			GET_HLSL_LOC_SRV(0)
//u0 dst mip
#define UAV_HZB_DST			GET_HLSL_LOC_UAV(0)
//b0 hzb constants
#define CBV_HZBCONST		GET_HLSL_LOC_CBV(0)
////////////////////HZB Buffer Ends

#if ENGINE_DEBUG_MESH
////////////////////meshviewer Buffer Begin
//b0 meshviewer constants
#define CBV_DEBUG_MESHVIEW_ID			GET_HLSL_LOC_CBV(0)
////////////////////meshviewer Buffer Ends
#endif // #if ENGINE_DEBUG_MESH

#define FEATURE_AO (1 << 0)

//define PSO indcies
namespace render
{
	enum PSO_INDEX
	{
		PSO_PBR,               //PBR
		PSO_GBUFFER,           //Gbuffer
		PSO_WIREFRAME,         //WireframeDebug
		PSO_SSAO,              //SSAO
		PSO_SSAOBLURX,         //SSAO_BlurHorizontal
		PSO_SSAOBLURY,         //SSAO_BlurVertical
		PSO_GENTERRAIN,        //GenTerrain
		PSO_GENNOISE,          //GenNoise
		PSO_GENUNIFIED,        //GenUnified
		PSO_GENCMDBUF,         //GenCmdBuf
		PSO_GBUFFERINDIRECT,   //GbufferIndirect
		PSO_AABBDEBUGDRAW,     //WireframeAABB
		PSO_GENTERRAININDEX,   //GenTerrainIndex
		PSO_UPLOADLOCALOBJ,    //UploadLocalObj
		PSO_CULLCLUSTER,       //CullCluster
		PSO_INITCLUSTER,       //initCluster
		PSO_GENHZB,            //genHZB
		PSO_RASTERIZER,        //Rasterizer
		PSO_CULLCLUSTER_POST,  //CullClusterPost
		PSO_PREPPOSTARGS,      //PrepPostArgs
		PSO_UPLOADPOSTOBJ,     //UploadPostObj
		PSO_END,
	};
}

namespace consts
{
	constexpr uint CONST_OBJ_SIZE = sizeof(float) * (4 * 4 + 3 + 1 + 1);
	constexpr uint CONST_OBJ_SIZE_ALLIGNMENT = 96;
	constexpr uint CONST_PROJ_SIZE = sizeof(float) * (4 * 4 + 4 + 4 * 4);
}

//define flags
#define MESH_INFO_FLAGS_TERRAIN		(1U << 0)
#define MESH_INFO_FLAGS_NONORM		(1U << 1)