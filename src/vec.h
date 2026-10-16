// 3D integer point/vector, as used by the original engine (Point3 1038:0662, Vector 1038:253a):
// x, y on screen; z = depth (draw order).
#pragma once

struct Vec3 {
    int x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}

    Vec3& operator+=(const Vec3& o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    Vec3& operator-=(const Vec3& o) {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    friend Vec3 operator+(Vec3 a, const Vec3& b) { return a += b; }
    friend Vec3 operator-(Vec3 a, const Vec3& b) { return a -= b; }
    friend bool operator==(const Vec3& a, const Vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
    friend bool operator!=(const Vec3& a, const Vec3& b) { return !(a == b); }
};

struct Point2 {
    int x = 0, y = 0;
};
