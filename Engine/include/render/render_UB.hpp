#pragma once

#include <render/renderer.hpp>
#include <render/buffer.hpp>

class renderer;

namespace render
{
	class UBManager
	{
	public:
		friend class ::renderer;

	private:
		buffer* unifiedVertexBuffer = nullptr;
		buffer* unifiedIndexBuffer = nullptr;
		buffer* vertexIDBuffer = nullptr;

		buffer* meshInfoBuffer = nullptr;
		buffer* lodInfoBuffer = nullptr;
		buffer* clusterInfoBuffer = nullptr;
		buffer* clusterBoundBuffer = nullptr;

		uint curVertexOffset = 0;
		uint curLodOffset = 0;
		uint curClusterOffset = 0;
		uint curIndexOffset = 0;

		bool init();
		void uploadMeshToUB(buffer* vertex, buffer* norm, buffer* index, meshData* meshdata, uint meshID, uint flags);
	};
};