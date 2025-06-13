#pragma once

#include <system/defines.hpp>

#include <string>

namespace math
{
    struct point
    {
        float x;
        float y;
        float z;

        point(float xValue, float yValue, float zValue);
        point();

        point operator-(const point& target) const;
        point& operator-=(const point& target);
        point& operator+=(const point& target);
        point operator=(const point& target);

        point cross(const point& target) const;
        point getAvg(uint num) const;
        std::string to_string() const;
        point getVec(point target) const;

        float dot(point v) const;
    };

    struct plane
    {
        point v;
        float d;

        plane(float v1, float v2, float v3, float constant);
        plane(point normal, float constant);

        bool isInPlane(point p) const;
        bool isSphereInPlane(point c, float r) const;
        float dot(point p) const;
    };

    bool compare_float(float a, float b);

    uint count_bits(uint value);
};
