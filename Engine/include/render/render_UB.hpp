#pragma once

#include <render/renderer.hpp>
#include <render/buffer.hpp>

namespace renderer
{
	namespace UB
	{
		buffer* unifiedVertexBuffer;
		buffer* unifiedNormalBuffer;
		buffer* unifiedIndexBuffer;

		uint curVertexOffset = 0;
		uint curLodOffset = 0;
		uint curClusterOffset = 0;
	};
};