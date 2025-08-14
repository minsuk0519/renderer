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

bool UBManager::init()
{
	curVertexOffset = 0;
	curLodOffset = 0;
	curClusterOffset = 0;

	unifiedVertexBuffer = e_globBufAllocator.alloc(nullptr, UVB_MAX_SIZE, 3, buf::GBF_SRV);
	unifiedIndexBuffer = e_globBufAllocator.alloc(nullptr, UIB_MAX_SIZE, 1, buf::GBF_SRV);

	//todo : maybe we need to move it to renderer
	vertexIDBuffer = e_globBufAllocator.alloc(nullptr, VERTEXID_MAX_SIZE, 1, buf::GBF_UAV | buf::GBF_SRV);

	meshInfoBuffer = e_globBufAllocator.alloc(nullptr, MAX_MESHES * sizeof(meshInfo), 1, buf::GBF_SRV);
	lodInfoBuffer = e_globBufAllocator.alloc(nullptr, MAX_MESHES_LOD * sizeof(lodInfo), 1, buf::GBF_SRV);
	clusterInfoBuffer = e_globBufAllocator.alloc(nullptr, MAX_OBJECTS * MAX_MESHES_CLUSTERS * sizeof(clusterInfo), 1, buf::GBF_SRV);
	clusterBoundBuffer = e_globBufAllocator.alloc(nullptr, MAX_OBJECTS * MAX_MESHES_CLUSTERS * sizeof(clusterbounddata), 1, buf::GBF_SRV);

	if (!unifiedVertexBuffer) return false;
	if (!unifiedIndexBuffer) return false;
	if (!vertexIDBuffer) return false;
	if (!meshInfoBuffer) return false;
	if (!lodInfoBuffer) return false;
	if (!clusterInfoBuffer) return false;
	if (!clusterBoundBuffer) return false;

	return true;
}

void UBManager::uploadMeshToUB(buffer* vertex, buffer* norm, buffer* index, meshData* meshdata, uint meshID, uint flags)
{
	getCmdQueue(QUEUE_COMPUTE)->getQueue()->BeginEvent(1, "Upload MeshInfo to UB", sizeof("Upload MeshInfo to UB"));

	{
		meshInfo meshinfo;
		uint totalClusterCount = 0;

		uint lodNum = meshdata->lodNum;
		meshinfo.numLod = lodNum;
		meshinfo.lodOffset = curLodOffset;
		meshinfo.vertexOffset = curVertexOffset;
		meshinfo.flags = flags;
		uint meshIndexOffset = curIndexOffset;

		for (uint j = 0; j < lodNum; ++j)
		{
			uint curClusterCount = meshdata->lodData[j].clusterNum;

			curClusterOffset += curClusterCount;
			totalClusterCount += curClusterCount;
		}
		uint vertexSize = vertex->getElemSize();
		curVertexOffset += vertexSize;

		lodInfo* lodInfos = new lodInfo[lodNum];
		clusterbounddata* clusterBounds = new clusterbounddata[totalClusterCount];
		clusterInfo* clusterInfos = new clusterInfo[totalClusterCount];
		uint lodIndex = 0;
		uint clusterIndex = 0;
		uint clusterInfoSize = curClusterOffset * sizeof(clusterInfo);
		uint clusterBoundSize = curClusterOffset * sizeof(clusterbounddata);
		uint indexOffset = curIndexOffset;
		uint totalIndexSize = 0;

		uint meshClusterIndex = 0;
		for (uint j = 0; j < lodNum; ++j)
		{
			uint curClusterCount = meshdata->lodData[j].clusterNum;
			lodInfos[lodIndex].clusterCount = curClusterCount;
			uint lodIndexSize = 0;

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
				lodIndexSize += indexCount;
				++clusterIndex;
			}
			lodInfos[lodIndex].clusterOffset = curClusterOffset;
			lodInfos[lodIndex].indexSize = lodIndexSize;
			curClusterOffset += curClusterCount;
			totalIndexSize += lodIndexSize;
			++lodIndex;
		}

		curIndexOffset += totalIndexSize;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList = getCmdQueue(QUEUE_COPY)->getCmdList();
		cmdList->Reset(getCmdQueue(QUEUE_COPY)->getAllocator().Get(), nullptr);
		e_globRenderer.uploadGPUBuffer(cmdList, meshInfoBuffer, meshID * sizeof(meshInfo), &meshinfo, sizeof(meshInfo));
		render::getCmdQueue(render::QUEUE_COPY)->execute({ cmdList });
		render::getCmdQueue(render::QUEUE_COPY)->flush();
		cmdList->Reset(getCmdQueue(QUEUE_COPY)->getAllocator().Get(), nullptr);
		e_globRenderer.uploadGPUBuffer(cmdList, lodInfoBuffer, curLodOffset * sizeof(lodInfo), lodInfos, lodNum * sizeof(lodInfo));
		render::getCmdQueue(render::QUEUE_COPY)->execute({ cmdList });
		render::getCmdQueue(render::QUEUE_COPY)->flush();
		cmdList->Reset(getCmdQueue(QUEUE_COPY)->getAllocator().Get(), nullptr);
		curLodOffset += lodNum;
		e_globRenderer.uploadGPUBuffer(cmdList, clusterInfoBuffer, clusterInfoSize, clusterInfos, totalClusterCount * sizeof(clusterInfo));
		render::getCmdQueue(render::QUEUE_COPY)->execute({ cmdList });
		render::getCmdQueue(render::QUEUE_COPY)->flush();
		cmdList->Reset(getCmdQueue(QUEUE_COPY)->getAllocator().Get(), nullptr);
		e_globRenderer.uploadGPUBuffer(cmdList, clusterBoundBuffer, clusterBoundSize, clusterBounds, totalClusterCount * sizeof(clusterbounddata));
		render::getCmdQueue(render::QUEUE_COPY)->execute({ cmdList });
		render::getCmdQueue(render::QUEUE_COPY)->flush();
		cmdList->Reset(getCmdQueue(QUEUE_COPY)->getAllocator().Get(), nullptr);

		delete[] clusterBounds;
		delete[] lodInfos;
		delete[] clusterInfos;

		e_globRenderer.copyGPUBuffer(cmdList, unifiedVertexBuffer, meshinfo.vertexOffset * sizeof(float) * 3, vertex, 0, vertex->getHeader()->dataSize);
		render::getCmdQueue(render::QUEUE_COPY)->execute({ cmdList });
		render::getCmdQueue(render::QUEUE_COPY)->flush();
		cmdList->Reset(getCmdQueue(QUEUE_COPY)->getAllocator().Get(), nullptr);
		e_globRenderer.copyGPUBuffer(cmdList, unifiedVertexBuffer, (MAX_VERTICES + meshinfo.vertexOffset) * sizeof(float) * 3, norm, 0, vertex->getHeader()->dataSize);
		render::getCmdQueue(render::QUEUE_COPY)->execute({ cmdList });
		render::getCmdQueue(render::QUEUE_COPY)->flush();
		cmdList->Reset(getCmdQueue(QUEUE_COPY)->getAllocator().Get(), nullptr);
		e_globRenderer.copyGPUBuffer(cmdList, unifiedIndexBuffer, meshIndexOffset * sizeof(uint), index, 0, index->getHeader()->dataSize);
		render::getCmdQueue(render::QUEUE_COPY)->execute({ cmdList });
		render::getCmdQueue(render::QUEUE_COPY)->flush();

		//{
		//	D3D12_BUFFER_SRV vertexDesc = {};
		//	vertexDesc.NumElements = vertex->getElemSize();
		//	vertexDesc.StructureByteStride = sizeof(float) * 3;
		//	D3D12_BUFFER_SRV indexDesc = {};
		//	indexDesc.NumElements = index->getElemSize();
		//	indexDesc.StructureByteStride = sizeof(uint) * 3;
		//	D3D12_BUFFER_SRV normDesc = {};
		//	normDesc.NumElements = norm->getElemSize();
		//	normDesc.StructureByteStride = sizeof(float) * 3;

		//	auto computeCmdList = getCmdQueue(QUEUE_COMPUTE)->getCmdList();

		//	getCmdQueue(QUEUE_COMPUTE)->bindPSO(PSO_GENUNIFIED);

		//	unifiedVertexBuffer->getDesc(buf::GBF_UAV);
		//	getCmdQueue(QUEUE_COMPUTE)->sendData(UAV_UNIFIED_VERTEX_BUFFER, unifiedVertexBuffer->getDesc(buf::GBF_UAV)->getHandle());
		//	getCmdQueue(QUEUE_COMPUTE)->sendData(UAV_UNIFIED_INDEX_BUFFER, unifiedIndexBuffer->getDesc(buf::GBF_UAV)->getHandle());

		//	getCmdQueue(QUEUE_COMPUTE)->sendData(SRV_VERTEX_BUFFER, vertex->getDesc(buf::GBF_SRV)->getHandle());
		//	getCmdQueue(QUEUE_COMPUTE)->sendData(SRV_INDEX_BUFFER, index->getDesc(buf::GBF_SRV)->getHandle());
		//	getCmdQueue(QUEUE_COMPUTE)->sendData(SRV_NORMAL_BUFFER, norm->getDesc(buf::GBF_SRV)->getHandle());
		//	getCmdQueue(QUEUE_COMPUTE)->sendData(SRV_UNIFIED_MESHINFO_BUFFER, meshInfoBuffer->getDesc(buf::GBF_SRV)->getHandle());

		//	unifiedConsts unifiedconst;

		//	uint indexSize = indexDesc.NumElements * 3;

		//	unifiedconst.vertexOffset = meshinfo.vertexOffset;
		//	unifiedconst.indexCount = indexSize;
		//	unifiedconst.indexOffset = indexOffset;
		//	unifiedconst.vertexCount = vertex->getElemSize();

		//	indexOffset += indexSize;

		//	getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_UNIFIEDCONSTS, 4, &unifiedconst);

		//	computeCmdList->Dispatch(1, 1, 1);

		//	getCmdQueue(QUEUE_COMPUTE)->execute({ computeCmdList });

		//	getCmdQueue(QUEUE_COMPUTE)->flush();
		//}
	}

	getCmdQueue(QUEUE_COMPUTE)->getQueue()->EndEvent();
}

}