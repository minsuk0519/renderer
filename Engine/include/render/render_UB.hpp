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
		buffer* unifiedVertexBuffer;
		buffer* unifiedNormalBuffer;
		buffer* unifiedIndexBuffer;

		buffer* meshInfoBuffer;
		buffer* lodInfoBuffer;
		buffer* clusterInfoBuffer;
		buffer* clusterBoundBuffer;

		uint curVertexOffset = 0;
		uint curLodOffset = 0;
		uint curClusterOffset = 0;
		uint curIndexOffset = 0;

		bool init();
		void uploadMeshToUB(buffer* vertex, buffer* norm, buffer* index, meshData* meshdata, uint meshID);
	};
};