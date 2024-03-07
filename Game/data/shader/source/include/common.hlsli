#include "struct.hlsli"

cbuffer projectionBuffer : register(b0)
{
    projection proj;
};

cbuffer objectBuffer : register(b1)
{
    object obj;
};

cbuffer cb_window : register(b2)
{
	uint screenWidth;
	uint screenHeight;
};

#define PI 3.14159265358979f
#define e 2.718281828459045f