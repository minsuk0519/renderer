#pragma once

#include <system/defines.hpp>

#include <algorithm>

namespace hzb
{
	inline uint mipDim(uint baseDim, uint mip)
	{
		if (baseDim == 0)
		{
			return 0;
		}
		if (mip >= 32)
		{
			return 1;
		}
		return (std::max)(1u, baseDim >> mip);
	}

	inline uint mipCount(uint width, uint height)
	{
		if (width == 0 || height == 0)
		{
			return 0;
		}

		uint largest = (std::max)(width, height);
		uint count = 1;
		while (largest > 1)
		{
			largest >>= 1;
			++count;
		}
		return count;
	}
}
