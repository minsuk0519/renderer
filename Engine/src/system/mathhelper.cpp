#include <system/mathhelper.hpp>

#include <cmath>

namespace math
{
	point::point(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue) {}
	point::point() : x(0.0f), y(0.0f), z(0.0f) {}

    point point::operator-(const point& target) const
    {
        point result;

        result.x = x - target.x;
        result.y = y - target.y;
        result.z = z - target.z;

        return result;
    }

    point& point::operator-=(const point& target)
    {
        x -= target.x;
        y -= target.y;
        z -= target.z;

        return *this;
    }

    point& point::operator+=(const point& target)
    {
        x += target.x;
        y += target.y;
        z += target.z;

        return *this;
    }

    point point::operator=(const point& target)
    {
        point result;

        result.x = target.x;
        result.y = target.y;
        result.z = target.z;

        return result;
    }

    point point::cross(const point& target) const
    {
        point result;

        result.x = y * target.z - z * target.y;
        result.y = z * target.x - x * target.z;
        result.z = x * target.y - y * target.x;

        return result;
    }

    point point::getAvg(uint num) const
    {
        point result;

        float f_num = static_cast<float>(num);

        result.x = x / f_num;
        result.y = y / f_num;
        result.z = z / f_num;

        return result;
    }

    std::string point::to_string() const
    {
        std::string result = "";

        result += std::to_string(x) + " ";
        result += std::to_string(y) + " ";
        result += std::to_string(z);

        return result;
    }

    point point::getVec(point target) const
    {
        double xDiff = x - target.x;
        double yDiff = y - target.y;
        double zDiff = z - target.z;

        double dLengthSquare = xDiff * xDiff + yDiff * yDiff + zDiff * zDiff;

        double dLength = std::sqrt(dLengthSquare);

        if (dLength == 0.0f)
        {
            auto a = 1;
        }

        double xUnit = xDiff / dLength;
        double yUnit = yDiff / dLength;
        double zUnit = zDiff / dLength;

        point result;

        result.x = xUnit;
        result.y = yUnit;
        result.z = zUnit;

        return result;
    }

    float point::dot(point v) const
    {
        return x * v.x + y * v.y + z * v.z;
    }

    plane::plane(float v1, float v2, float v3, float constant) : v(v1, v2, v3), d(constant) {}

    plane::plane(point normal, float constant) : v(normal), d(constant) {}

    bool plane::isInPlane(point p) const
    {
        return dot(p) <= 0;
    }

    bool plane::isSphereInPlane(point c, float r) const
    {
        return (dot(c) <= -r);
    }

    float plane::dot(point p) const
    {
        return v.dot(p) + d;
    }
    bool compare_float(float a, float b)
    {
        return (std::abs(a - b) < FLT_EPSILON);
    }
};