#include "struct.hlsli"

cbuffer projectionBuffer : register(b0)
{
    projection proj;
};

cbuffer objectBuffer : register(b1)
{
    object obj;
};

