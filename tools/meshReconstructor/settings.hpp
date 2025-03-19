#pragma once

#include <vector>

//one triangle per thread
const size_t kClusterSize = 64;

const size_t kGroupSize = 8;

struct Vertex
{
	float px, py, pz;
	float nx, ny, nz;
	float tx, ty;
};

struct LODBounds
{
	float center[3];
	float radius;
	float error;
};

struct clusterInfo
{
	unsigned int clusterNum;
	unsigned int clusterSize;
	unsigned int clusterOffset;
};

struct Cluster
{
	std::vector<unsigned int> indices;

	LODBounds self;
	LODBounds parent;
};