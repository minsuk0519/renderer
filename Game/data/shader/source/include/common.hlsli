#include "struct.hlsli"

cbuffer projectionBuffer : register(b0)
{
    projection proj;
};

cbuffer objectBuffer : register(b1)
{
    object obj;
};

#define PI 3.14159265358979f
#define e 2.718281828459045f