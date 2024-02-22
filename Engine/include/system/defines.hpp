#pragma once

typedef unsigned int uint;

namespace shader_loc
{
	enum SHADER_DEFINE
	{
		UAV_PROJECTION = 0,
		SRV_GBUFFER_0,
		SRV_GBUFFER_1,
		SHADER_DEFINE_MAX,
	};
}

#define PI 3.14159265358979f
#define PI_HALF PI / 2.0f