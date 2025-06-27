#pragma once

#include <render/renderer.hpp>
#include <render/buffer.hpp>

namespace render
{
	class UBManger
	{
	public:
		friend class renderer;

	private:
		buffer* unifiedVertexBuffer;
		buffer* unifiedNormalBuffer;
		buffer* unifiedIndexBuffer;

		uint curVertexOffset = 0;
		uint curLodOffset = 0;
		uint curClusterOffset = 0;

		bool init();
		void uploadMeshToUB(buffer* vertex, buffer* norm, buffer* index, meshData* meshdata, uint meshID);
	};
};