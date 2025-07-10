#include <render/render_UB.hpp>
#include <render/commandqueue.hpp>
#include <render/mesh.hpp>

#include <system/config.hpp>

namespace render
{

struct unifiedConsts
{
	uint vertexCount = 0;
	uint indexCount = 0;
	uint indexOffset = 0;
	uint vertexOffset = 0;
};


bool UBManager::init()
{
	curVertexOffset = 0;
	curLodOffset = 0;
	curClusterOffset = 0;

	unifiedVertexBuffer = e_globBufAllocator.alloc(nullptr, UVB_MAX_SIZE, 3, buf::GBF_UAV | buf::GBF_SRV);
	unifiedNormalBuffer = e_globBufAllocator.alloc(nullptr, UNB_MAX_SIZE, 3, buf::GBF_UAV | buf::GBF_SRV);
	unifiedIndexBuffer = e_globBufAllocator.alloc(nullptr, UIB_MAX_SIZE, 1, buf::GBF_UAV | buf::GBF_SRV);

	//TODO : we can make seperate upload buffer
	meshInfoBuffer = e_globBufAllocator.alloc(nullptr, MAX_MESHES, 1, buf::GBF_UAV);
	lodInfoBuffer = e_globBufAllocator.alloc(nullptr, MAX_MESHES_LOD, 1, buf::GBF_UAV);
	clusterInfoBuffer = e_globBufAllocator.alloc(nullptr, MAX_MESHES_CLUSTERS, 1, buf::GBF_UAV);
	clusterBoundBuffer = e_globBufAllocator.alloc(nullptr, MAX_MESHES_CLUSTERS * sizeof(clusterbounddata), 1, buf::GBF_UAV);
}

void UBManager::uploadMeshToUB(buffer* vertex, buffer* norm, buffer* index, meshData* meshdata, uint meshID)
{
	getCmdQueue(QUEUE_COMPUTE)->getQueue()->BeginEvent(1, "Upload MeshInfo to UB", sizeof("Upload MeshInfo to UB"));

	{
		meshInfo meshinfo;
		uint totalClusterCount = 0;

		uint lodNum = meshdata->lodNum;
		meshinfo.numLod = lodNum;
		meshinfo.lodOffset = curLodOffset;
		meshinfo.vertexOffset = curVertexOffset;

		for (uint j = 0; j < lodNum; ++j)
		{
			uint curClusterCount = meshdata->lodData[j].clusterNum;

			curClusterOffset += curClusterCount;
			totalClusterCount += curClusterCount;
		}
		uint vertexSize = vertex->getElemSize();
		curVertexOffset += vertexSize;

		struct lodInfo
		{
			uint clusterCount;
			uint clusterOffset;
			uint indexSize;
		};

		struct clusterInfo
		{
			uint indexCount;
			uint indexOffset;
		};

		lodInfo* lodInfos = new lodInfo[lodNum];
		clusterbounddata* clusterBounds = new clusterbounddata[totalClusterCount];
		clusterInfo* clusterInfos = new clusterInfo[totalClusterCount];
		uint lodIndex = 0;
		uint clusterIndex = 0;
		uint clusterInfoSize = curClusterOffset * sizeof(clusterInfo);
		curClusterOffset = 0;
		uint indexOffset = 0;
		uint vertexOffset = 0;

		uint lodNum = meshdata->lodNum;
		uint meshClusterIndex = 0;
		for (uint j = 0; j < lodNum; ++j)
		{
			uint curClusterCount = meshdata->lodData[j].clusterNum;
			lodInfos[lodIndex].clusterCount = curClusterCount;
			uint totalIndexSize = 0;

			for (uint k = 0; k < curClusterCount; ++k)
			{
				uint indexCount = meshdata->lodData[j].indexSize[k];
				clusterInfos[clusterIndex].indexCount = indexCount;
				clusterInfos[clusterIndex].indexOffset = indexOffset;

				if (!(meshinfo.flags & MESH_INFO_FLAGS_TERRAIN))
				{
					clusterBounds[clusterIndex].sphere.center[0] = meshdata->clusterBounds[meshClusterIndex].sphere.center[0];
					clusterBounds[clusterIndex].sphere.center[1] = meshdata->clusterBounds[meshClusterIndex].sphere.center[1];
					clusterBounds[clusterIndex].sphere.center[2] = meshdata->clusterBounds[meshClusterIndex].sphere.center[2];
					clusterBounds[clusterIndex].sphere.radius = meshdata->clusterBounds[meshClusterIndex].sphere.radius;

					clusterBounds[clusterIndex].aabb.center[0] = meshdata->clusterBounds[meshClusterIndex].aabb.center[0];
					clusterBounds[clusterIndex].aabb.center[1] = meshdata->clusterBounds[meshClusterIndex].aabb.center[1];
					clusterBounds[clusterIndex].aabb.center[2] = meshdata->clusterBounds[meshClusterIndex].aabb.center[2];
					clusterBounds[clusterIndex].aabb.hExtent[0] = meshdata->clusterBounds[meshClusterIndex].aabb.hExtent[0];
					clusterBounds[clusterIndex].aabb.hExtent[1] = meshdata->clusterBounds[meshClusterIndex].aabb.hExtent[1];
					clusterBounds[clusterIndex].aabb.hExtent[2] = meshdata->clusterBounds[meshClusterIndex].aabb.hExtent[2];

					++meshClusterIndex;
				}

				indexOffset += indexCount;
				totalIndexSize += indexCount;
				++clusterIndex;
			}
			lodInfos[lodIndex].clusterOffset = curClusterOffset;
			lodInfos[lodIndex].indexSize = totalIndexSize;
			curClusterOffset += curClusterCount;
			++lodIndex;
		}

		meshInfoBuffer->uploadBuffer(sizeof(meshInfo), meshID, &meshinfo);
		lodInfoBuffer->uploadBuffer(lodNum, curLodOffset, lodInfos);
		curLodOffset += lodNum;
		clusterInfoBuffer->uploadBuffer(clusterInfoSize, 0, clusterInfos);
		clusterBoundBuffer->uploadBuffer(curClusterOffset * sizeof(clusterbounddata), 0, clusterBounds);

		delete[] clusterBounds;
		delete[] lodInfos;
		delete[] clusterInfos;

		indexOffset = 0;

		{
			D3D12_BUFFER_SRV vertexDesc = {};
			vertexDesc.NumElements = vertex->getElemSize();
			vertexDesc.StructureByteStride = sizeof(float) * 3;
			D3D12_BUFFER_SRV indexDesc = {};
			indexDesc.NumElements = index->getElemSize();
			indexDesc.StructureByteStride = sizeof(uint) * 3;
			D3D12_BUFFER_SRV normDesc = {};
			normDesc.NumElements = norm->getElemSize();
			normDesc.StructureByteStride = sizeof(float) * 3;

			auto computeCmdList = getCmdQueue(QUEUE_COMPUTE)->getCmdList();

			getCmdQueue(QUEUE_COMPUTE)->bindPSO(PSO_GENUNIFIED);

			unifiedVertexBuffer->getDesc(buf::GBF_UAV);
			getCmdQueue(QUEUE_COMPUTE)->sendData(UAV_UNIFIED_VERTEX_BUFFER, unifiedVertexBuffer->getDesc(buf::GBF_UAV)->getHandle());
			getCmdQueue(QUEUE_COMPUTE)->sendData(UAV_UNIFIED_INDEX_BUFFER, unifiedIndexBuffer->getDesc(buf::GBF_UAV)->getHandle());

			getCmdQueue(QUEUE_COMPUTE)->sendData(SRV_VERTEX_BUFFER, vertex->getDesc(buf::GBF_SRV)->getHandle());
			getCmdQueue(QUEUE_COMPUTE)->sendData(SRV_INDEX_BUFFER, index->getDesc(buf::GBF_SRV)->getHandle());
			getCmdQueue(QUEUE_COMPUTE)->sendData(SRV_NORMAL_BUFFER, norm->getDesc(buf::GBF_SRV)->getHandle());
			getCmdQueue(QUEUE_COMPUTE)->sendData(SRV_UNIFIED_MESHINFO_BUFFER, meshInfoBuffer->getDesc(buf::GBF_SRV)->getHandle());

			unifiedConsts unifiedconst;

			uint indexSize = indexDesc.NumElements * 3;

			unifiedconst.vertexOffset = meshinfo.vertexOffset;
			unifiedconst.indexCount = indexSize;
			unifiedconst.indexOffset = indexOffset;
			unifiedconst.vertexCount = vertex->getElemSize();

			indexOffset += indexSize;

			getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_UNIFIEDCONSTS, 4, &unifiedconst);

			computeCmdList->Dispatch(1, 1, 1);

			getCmdQueue(QUEUE_COMPUTE)->execute({ computeCmdList });

			getCmdQueue(QUEUE_COMPUTE)->flush();
		}
	}

	getCmdQueue(QUEUE_COMPUTE)->getQueue()->EndEvent();
}

}