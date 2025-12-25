#include "Vec2.h"

float Vec2::length() const {
    return std::sqrt(x * x + y * y);
}

void Vec2::normalize() {
    float len = length();
    if (len != 0) {
        x /= len;
        y /= len;
    }
}

float Vec2::dot(const Vec2& other) const {
    return x * other.x + y * other.y;
}

float Vec2::cross(const Vec2& other) const {
    return x * other.y - y * other.x;
}

float Vec2::angle(const Vec2& other) const {
    float lenProduct = length() * other.length();
    if (lenProduct == 0) return 0.f;
    float cosTheta = dot(other) / lenProduct;
    return std::acos(std::fmax(-1.f, std::fmin(1.f, cosTheta)));
}