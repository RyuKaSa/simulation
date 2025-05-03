#pragma once

#include <vector>
#include <glad/glad.h>
#include "common.hpp"

namespace glimac {

class Sphere {
    // Allocates and builds the vertex data (implemented in Sphere.cpp)
    void build(GLfloat radius, GLsizei discLat, GLsizei discLong);

public:
    // Constructor: allocates the data array and builds the vertex attributes
    Sphere(GLfloat radius, GLsizei discLat, GLsizei discLong)
        : m_radius(radius), m_nVertexCount(0) {
        build(radius, discLat, discLong); // Build the sphere
    }

    // Returns the radius of the sphere
    GLfloat getRadius() const {
        return m_radius;
    }

    // Returns the pointer to the vertex data
    const ShapeVertex* getDataPointer() const {
        return &m_Vertices[0];
    }
    
    // Returns the number of vertices
    GLsizei getVertexCount() const {
        return m_nVertexCount;
    }

private:
    std::vector<ShapeVertex> m_Vertices;
    GLsizei m_nVertexCount; // Number of vertices
    GLfloat m_radius;       // Radius of the sphere
};

} // namespace glimac