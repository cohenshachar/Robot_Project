#include "positional_stl.h"
#include <iostream>

Point::Point(double x, double y) : x(x), y(y) {}

Point Point::operator-(const Point& other) const {
    return {x - other.x, y - other.y};
}

Point Point::operator+(const Point& other) const {
    return {x + other.x, y + other.y};
}

Point Point::operator*(double scalar) const {
    return {x * scalar, y * scalar};
}

bool Point::operator==(const Point& other) const {
    return (x == other.x && y == other.y);
}

Point& Point::operator+=(const Point& other) {
    x += other.x;
    y += other.y;
    return *this;
}

double Point::cross(const Point& other) const {
    return x * other.y - y * other.x;
}

double Point::length() const {
    return std::sqrt(x * x + y * y);
}

double Point::dot(const Point& other) const {
    return x * other.x + y * other.y;
}

Point Point::normalized() const {
    double len = length();
    return len > 0 ? (*this) * (1.0 / len) : Point(0, 0);
}


Point toWorldFrame(const Pose& pose, const Point& localPoint) {
    double cos_theta = std::cos(pose.theta);
    double sin_theta = std::sin(pose.theta);

    return {
        pose.loc.x + cos_theta * localPoint.x - sin_theta * localPoint.y,
        pose.loc.y + sin_theta * localPoint.x + cos_theta * localPoint.y
    };
}


Point Line::getClosestPoint(const Pose& pose) const {
    float dx = cos(theta);
    float dy = sin(theta);
    Point dir(dx, dy);
    Point r = pose.loc - p0;
    float t = r.dot(dir);

    if (finite) {
        // Clamp t to [0, length] to restrict closest point on finite segment
        t = fmax(0.0f, fmin(length, t));
    }

    return p0 + dir * t;
}
Point Line::getLookaheadPoint(const Pose& pose, float radius) const {
    float dx = cos(theta);
    float dy = sin(theta);
    Point dir(dx, dy);

    // Vector from pose to p0
    float cx = p0.x - pose.loc.x;
    float cy = p0.y - pose.loc.y;

    float a = 1.0f;
    float b = 2.0f * (cx * dx + cy * dy);
    float c = (cx * cx + cy * cy) - radius * radius;

    float discriminant = b * b - 4 * a * c;
if (discriminant >= 0.0f) {
    float sqrtDisc = sqrt(discriminant);
    float t1 = (-b + sqrtDisc) / (2 * a);
    float t2 = (-b - sqrtDisc) / (2 * a);

    bool t1_valid = (t1 >= 0.0f && t1 <= length);
    bool t2_valid = (t2 >= 0.0f && t2 <= length);

    if (finite) {
        if (t1_valid && t2_valid) {
            // Return the further one along the direction
            return (t1 > t2) ? p0 + dir * t1 : p0 + dir * t2;
        } else if (t1_valid) {
            if (t2 > length) return p1;  // t2 would’ve been beyond p1
            return p0 + dir * t1;
        } else if (t2_valid) {
            if (t1 > length) return p1;
            return p0 + dir * t2;
        } else {
            // Both are out of bounds — fallback to projection logic
        }
    } else {
        // Infinite line — use both
        Point p1_ = p0 + dir * t1;
        Point p2_ = p0 + dir * t2;
        float dp1 = (p1_ - p0).dot(dir);
        float dp2 = (p2_ - p0).dot(dir);
        return (dp1 >= dp2) ? p1_ : p2_;
    }
}


  float closestT = (pose.loc - p0).dot(dir);

  // Clamp closestT to [0, length] if finite
  if (finite) {
      closestT = fmax(0.0f, fmin(length, closestT));
  }

  float lookaheadT = closestT + radius;

  // If lookahead would go past the end of a finite line, just return p1
  if (finite && lookaheadT >= length) {
      return p1;
  }

  // Clamp lookaheadT if finite
  if (finite) {
      lookaheadT = fmax(0.0f, fmin(length, lookaheadT));
  }

  return p0 + dir * lookaheadT;

}

bool Line::isFinished(const Pose& pose) const {
    return false; // Placeholder — define criteria if needed
}
