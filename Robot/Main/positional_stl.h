#ifndef POSITIONAL_STL
#define POSITIONAL_STL
#include <cmath>


struct Point {
    double x, y;

    Point(double x = 0.0, double y = 0.0);

    Point operator-(const Point& other) const;
    Point operator+(const Point& other) const;
    Point operator*(double scalar) const;
    bool operator==(const Point& other) const;
    Point& operator+=(const Point& other);

    double cross(const Point& other) const;
    double length() const;
    double dot(const Point& other) const;
    Point normalized() const;
};

inline Point operator*(double scalar, const Point& p) {
    return Point(p.x * scalar, p.y * scalar);
}

struct Pose {
  Point loc;
  double vLinear; // Linear velocity (m/s)
  double vAngular;// Angular velocity (rad/s)
  double theta; // orientation
};

class IPath {
public:
    virtual Point getClosestPoint(const Pose& pose) const = 0;
    virtual Point getLookaheadPoint(const Pose& pose, float radius) const = 0;
    virtual bool isFinished(const Pose& pose) const = 0;
    virtual ~IPath() {}
};

class IPoseProvider {
public:
    virtual Pose getPose() = 0;
    virtual ~IPoseProvider() = default;
};

class Line : public IPath {
public:
  // Infinite line defined by point and angle
  Line(const Point& point, float theta)
      : p0(point), theta(theta), finite(false), length(0.0f) {
      p1 = p0 + Point(cos(theta), sin(theta));  // dummy direction endpoint
  }

  // Finite line defined by point, direction, and length
  Line(const Point& point, float theta, float length)
      : p0(point), theta(theta), finite(true), length(length) {
      p1 = p0 + Point(cos(theta), sin(theta)) * length;
  }

  // Finite line defined by two endpoints
  Line(const Point& a, const Point& b)
      : p0(a), p1(b), finite(true) {
      Point delta = p1 - p0;
      theta = atan2(delta.y, delta.x);
      length = delta.length();
  }

    Point getClosestPoint(const Pose& pose) const override;
    Point getLookaheadPoint(const Pose& pose, float radius) const override;
    bool isFinished(const Pose& pose) const override;
    Point getLineDefPoint() const{ return p0;}
    float getLineTheta() const{ return theta;}
private:
    bool finite = false;
    float length = 0;
    float theta = 0;
    Point p0, p1; // finite segment endpoints
};


Point toWorldFrame(const Pose& pose, const Point& localPoint);

#endif // POSITIONAL_STL
