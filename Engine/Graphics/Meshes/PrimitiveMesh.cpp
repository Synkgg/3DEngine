#include "PrimitiveMesh.h"

#include <cmath>
#include <cstdint>
#include <vector>

std::unique_ptr<Mesh> PrimitiveMesh::CreateCube()
{
    std::vector<Vertex> vertices =
    {
        // Front
        { {-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f} },
        { {-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f} },

        // Back
        { { 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f} },
        { {-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f} },
        { {-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f} },
        { { 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f} },

        // Left
        { {-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },
        { {-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
        { {-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
        { {-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },

        // Right
        { { 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },
        { { 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f, 0.0f}, {1.0f, 1.0f} },
        { { 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f, 0.0f}, {0.0f, 1.0f} },

        // Top
        { {-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f} },
        { { 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f} },
        { { 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f, 0.0f}, {1.0f, 1.0f} },
        { {-0.5f,  0.5f, -0.5f}, { 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f} },

        // Bottom
        { {-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f} },
        { { 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f} },
        { { 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f} },
        { {-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f} }
    };

    std::vector<std::uint32_t> indices =
    {
         0,  1,  2,  2,  3,  0,
         4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    return std::make_unique<Mesh>(
        vertices,
        indices
    );
}

std::unique_ptr<Mesh> PrimitiveMesh::CreatePlane()
{
    std::vector<Vertex> vertices =
    {
        // Bottom-left
        {
            {-0.5f, 0.0f, -0.5f},
            { 0.0f, 1.0f,  0.0f},
            { 0.0f, 0.0f}
        },

        // Bottom-right
        {
            { 0.5f, 0.0f, -0.5f},
            { 0.0f, 1.0f,  0.0f},
            { 1.0f, 0.0f}
        },

        // Top-right
        {
            { 0.5f, 0.0f,  0.5f},
            { 0.0f, 1.0f,  0.0f},
            { 1.0f, 1.0f}
        },

        // Top-left
        {
            {-0.5f, 0.0f,  0.5f},
            { 0.0f, 1.0f,  0.0f},
            { 0.0f, 1.0f}
        }
    };

    std::vector<std::uint32_t> indices =
    {
        0, 1, 2,
        2, 3, 0
    };

    return std::make_unique<Mesh>(
        vertices,
        indices
    );
}

std::unique_ptr<Mesh> PrimitiveMesh::CreateSphere()
{
    constexpr float radius = 0.5f;
    constexpr std::uint32_t segments = 32;
    constexpr std::uint32_t rings = 16;

    constexpr float pi = 3.14159265358979323846f;

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    vertices.reserve(
        (rings + 1) * (segments + 1)
    );

    indices.reserve(
        rings * segments * 6
    );

    for (std::uint32_t ring = 0; ring <= rings; ++ring)
    {
        float v =
            static_cast<float>(ring) /
            static_cast<float>(rings);

        float phi = v * pi;

        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (std::uint32_t segment = 0;
            segment <= segments;
            ++segment)
        {
            float u =
                static_cast<float>(segment) /
                static_cast<float>(segments);

            float theta =
                u * pi * 2.0f;

            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            float x =
                sinPhi * cosTheta;

            float y =
                cosPhi;

            float z =
                sinPhi * sinTheta;

            Vertex vertex{};

            vertex.position[0] = x * radius;
            vertex.position[1] = y * radius;
            vertex.position[2] = z * radius;

            vertex.normal[0] = x;
            vertex.normal[1] = y;
            vertex.normal[2] = z;

            vertex.uv[0] = u;
            vertex.uv[1] = 1.0f - v;

            vertices.push_back(vertex);
        }
    }

    for (std::uint32_t ring = 0; ring < rings; ++ring)
    {
        for (std::uint32_t segment = 0;
            segment < segments;
            ++segment)
        {
            std::uint32_t current =
                ring * (segments + 1) + segment;

            std::uint32_t next =
                current + segments + 1;

            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(next);

            indices.push_back(current + 1);
            indices.push_back(next + 1);
            indices.push_back(next);
        }
    }

    return std::make_unique<Mesh>(
        vertices,
        indices
    );
}

std::unique_ptr<Mesh> PrimitiveMesh::CreateCylinder()
{
    constexpr float radius = 0.5f;
    constexpr float height = 1.0f;
    constexpr std::uint32_t segments = 32;

    constexpr float pi =
        3.14159265358979323846f;

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    // Side vertices.
    for (std::uint32_t segment = 0;
        segment <= segments;
        ++segment)
    {
        float u =
            static_cast<float>(segment) /
            static_cast<float>(segments);

        float angle =
            u * pi * 2.0f;

        float x =
            std::cos(angle) * radius;

        float z =
            std::sin(angle) * radius;

        // Bottom
        {
            Vertex vertex{};

            vertex.position[0] = x;
            vertex.position[1] = -height * 0.5f;
            vertex.position[2] = z;

            vertex.normal[0] = std::cos(angle);
            vertex.normal[1] = 0.0f;
            vertex.normal[2] = std::sin(angle);

            vertex.uv[0] = u;
            vertex.uv[1] = 0.0f;

            vertices.push_back(vertex);
        }

        // Top
        {
            Vertex vertex{};

            vertex.position[0] = x;
            vertex.position[1] = height * 0.5f;
            vertex.position[2] = z;

            vertex.normal[0] = std::cos(angle);
            vertex.normal[1] = 0.0f;
            vertex.normal[2] = std::sin(angle);

            vertex.uv[0] = u;
            vertex.uv[1] = 1.0f;

            vertices.push_back(vertex);
        }
    }

    // Side indices.
    for (std::uint32_t segment = 0;
        segment < segments;
        ++segment)
    {
        std::uint32_t bottom =
            segment * 2;

        std::uint32_t top =
            bottom + 1;

        std::uint32_t nextBottom =
            (segment + 1) * 2;

        std::uint32_t nextTop =
            nextBottom + 1;

        indices.push_back(bottom);
        indices.push_back(nextBottom);
        indices.push_back(top);

        indices.push_back(top);
        indices.push_back(nextBottom);
        indices.push_back(nextTop);
    }

    // Bottom cap center.
    std::uint32_t bottomCenter =
        static_cast<std::uint32_t>(
            vertices.size()
            );

    {
        Vertex vertex{};

        vertex.position[0] = 0.0f;
        vertex.position[1] = -height * 0.5f;
        vertex.position[2] = 0.0f;

        vertex.normal[0] = 0.0f;
        vertex.normal[1] = -1.0f;
        vertex.normal[2] = 0.0f;

        vertex.uv[0] = 0.5f;
        vertex.uv[1] = 0.5f;

        vertices.push_back(vertex);
    }

    // Bottom cap ring.
    std::uint32_t bottomCapStart =
        static_cast<std::uint32_t>(
            vertices.size()
            );

    for (std::uint32_t segment = 0;
        segment <= segments;
        ++segment)
    {
        float u =
            static_cast<float>(segment) /
            static_cast<float>(segments);

        float angle =
            u * pi * 2.0f;

        Vertex vertex{};

        vertex.position[0] =
            std::cos(angle) * radius;

        vertex.position[1] =
            -height * 0.5f;

        vertex.position[2] =
            std::sin(angle) * radius;

        vertex.normal[0] = 0.0f;
        vertex.normal[1] = -1.0f;
        vertex.normal[2] = 0.0f;

        vertex.uv[0] =
            0.5f +
            std::cos(angle) * 0.5f;

        vertex.uv[1] =
            0.5f +
            std::sin(angle) * 0.5f;

        vertices.push_back(vertex);
    }

    for (std::uint32_t segment = 0;
        segment < segments;
        ++segment)
    {
        std::uint32_t current =
            bottomCapStart + segment;

        std::uint32_t next =
            current + 1;

        indices.push_back(bottomCenter);
        indices.push_back(next);
        indices.push_back(current);
    }

    // Top cap center.
    std::uint32_t topCenter =
        static_cast<std::uint32_t>(
            vertices.size()
            );

    {
        Vertex vertex{};

        vertex.position[0] = 0.0f;
        vertex.position[1] = height * 0.5f;
        vertex.position[2] = 0.0f;

        vertex.normal[0] = 0.0f;
        vertex.normal[1] = 1.0f;
        vertex.normal[2] = 0.0f;

        vertex.uv[0] = 0.5f;
        vertex.uv[1] = 0.5f;

        vertices.push_back(vertex);
    }

    // Top cap ring.
    std::uint32_t topCapStart =
        static_cast<std::uint32_t>(
            vertices.size()
            );

    for (std::uint32_t segment = 0;
        segment <= segments;
        ++segment)
    {
        float u =
            static_cast<float>(segment) /
            static_cast<float>(segments);

        float angle =
            u * pi * 2.0f;

        Vertex vertex{};

        vertex.position[0] =
            std::cos(angle) * radius;

        vertex.position[1] =
            height * 0.5f;

        vertex.position[2] =
            std::sin(angle) * radius;

        vertex.normal[0] = 0.0f;
        vertex.normal[1] = 1.0f;
        vertex.normal[2] = 0.0f;

        vertex.uv[0] =
            0.5f +
            std::cos(angle) * 0.5f;

        vertex.uv[1] =
            0.5f +
            std::sin(angle) * 0.5f;

        vertices.push_back(vertex);
    }

    for (std::uint32_t segment = 0;
        segment < segments;
        ++segment)
    {
        std::uint32_t current =
            topCapStart + segment;

        std::uint32_t next =
            current + 1;

        indices.push_back(topCenter);
        indices.push_back(current);
        indices.push_back(next);
    }

    return std::make_unique<Mesh>(
        vertices,
        indices
    );
}