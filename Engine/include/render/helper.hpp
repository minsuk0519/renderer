#pragma once

//for reference
struct GaussianWeight
{
	int w;
	float weight[101];
	void build(int n)
	{
		w = n;

		float sumWeight = 0.0f;

		for (int i = -w; i <= w; ++i)
		{
			weight[i + w] = static_cast<float>(std::exp(-(2 * i * i) / (float)(w * w)));
			sumWeight += weight[i + w];
		}

		for (int i = -w; i <= w; ++i)
		{
			weight[i + w] /= sumWeight;
		}
	}
};