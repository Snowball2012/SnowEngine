#pragma once

#include <array>
#include <cetonia/parser.h>
#include "SceneImporter.h"

namespace GeomGeneration
{
    constexpr std::array<uint16_t, 6> LineIndices =
    {
        3, 2, 0,
        1, 3, 0
    };

    void MakeLineVertices( const ctTokenLine2d& line, Vertex vertices[4] );

    // left-handed coordinate system
    constexpr std::array<Vertex, 24> CubeVertices =
    {
        // yz front side
        Vertex{ DirectX::XMFLOAT3{ 1, -1, -1 }, DirectX::XMFLOAT3{  1, 0, 0 }, DirectX::XMFLOAT2{ 0, 1 } }, // 0
        Vertex{ DirectX::XMFLOAT3{ 1,  1, -1 }, DirectX::XMFLOAT3{  1, 0, 0 }, DirectX::XMFLOAT2{ 0, 0 } }, // 1
        Vertex{ DirectX::XMFLOAT3{ 1,  1,  1 }, DirectX::XMFLOAT3{  1, 0, 0 }, DirectX::XMFLOAT2{ 1, 0 } }, // 2
        Vertex{ DirectX::XMFLOAT3{ 1, -1,  1 }, DirectX::XMFLOAT3{  1, 0, 0 }, DirectX::XMFLOAT2{ 1, 1 } }, // 3
        // yz back side
        Vertex{ DirectX::XMFLOAT3{-1, -1,  1 }, DirectX::XMFLOAT3{ -1, 0, 0 }, DirectX::XMFLOAT2{ 1, 1 } }, // 4
        Vertex{ DirectX::XMFLOAT3{-1,  1,  1 }, DirectX::XMFLOAT3{ -1, 0, 0 }, DirectX::XMFLOAT2{ 1, 0 } }, // 5
        Vertex{ DirectX::XMFLOAT3{-1,  1, -1 }, DirectX::XMFLOAT3{ -1, 0, 0 }, DirectX::XMFLOAT2{ 0, 0 } }, // 6
        Vertex{ DirectX::XMFLOAT3{-1, -1, -1 }, DirectX::XMFLOAT3{ -1, 0, 0 }, DirectX::XMFLOAT2{ 0, 1 } }, // 7

        // xy back side
        Vertex{ DirectX::XMFLOAT3{ -1, -1, -1 }, DirectX::XMFLOAT3{  0, 0, -1 }, DirectX::XMFLOAT2{ 0, 1 } }, // 8
        Vertex{ DirectX::XMFLOAT3{ -1,  1, -1 }, DirectX::XMFLOAT3{  0, 0, -1 }, DirectX::XMFLOAT2{ 0, 0 } }, // 9
        Vertex{ DirectX::XMFLOAT3{  1,  1, -1 }, DirectX::XMFLOAT3{  0, 0, -1 }, DirectX::XMFLOAT2{ 1, 0 } }, // 10
        Vertex{ DirectX::XMFLOAT3{  1, -1, -1 }, DirectX::XMFLOAT3{  0, 0, -1 }, DirectX::XMFLOAT2{ 1, 1 } }, // 11
        // xy front side
        Vertex{ DirectX::XMFLOAT3{  1, -1,  1 }, DirectX::XMFLOAT3{ 0, 0, 1 }, DirectX::XMFLOAT2{ 1, 1 } }, // 12
        Vertex{ DirectX::XMFLOAT3{  1,  1,  1 }, DirectX::XMFLOAT3{ 0, 0, 1 }, DirectX::XMFLOAT2{ 1, 0 } }, // 13
        Vertex{ DirectX::XMFLOAT3{ -1,  1,  1 }, DirectX::XMFLOAT3{ 0, 0, 1 }, DirectX::XMFLOAT2{ 0, 0 } }, // 14
        Vertex{ DirectX::XMFLOAT3{ -1, -1,  1 }, DirectX::XMFLOAT3{ 0, 0, 1 }, DirectX::XMFLOAT2{ 0, 1 } }, // 15

        // xz front side
        Vertex{ DirectX::XMFLOAT3{ -1,  1, -1 }, DirectX::XMFLOAT3{  0, 1, 0 }, DirectX::XMFLOAT2{ 0, 1 } }, // 16
        Vertex{ DirectX::XMFLOAT3{ -1,  1,  1 }, DirectX::XMFLOAT3{  0, 1, 0 }, DirectX::XMFLOAT2{ 0, 0 } }, // 17
        Vertex{ DirectX::XMFLOAT3{  1,  1,  1 }, DirectX::XMFLOAT3{  0, 1, 0 }, DirectX::XMFLOAT2{ 1, 0 } }, // 18
        Vertex{ DirectX::XMFLOAT3{  1,  1, -1 }, DirectX::XMFLOAT3{  0, 1, 0 }, DirectX::XMFLOAT2{ 1, 1 } }, // 19
        // xz back side
        Vertex{ DirectX::XMFLOAT3{  1, -1, -1 }, DirectX::XMFLOAT3{ 0, 0, -1 }, DirectX::XMFLOAT2{ 1, 1 } }, // 20
        Vertex{ DirectX::XMFLOAT3{  1, -1,  1 }, DirectX::XMFLOAT3{ 0, 0, -1 }, DirectX::XMFLOAT2{ 1, 0 } }, // 21
        Vertex{ DirectX::XMFLOAT3{ -1, -1,  1 }, DirectX::XMFLOAT3{ 0, 0, -1 }, DirectX::XMFLOAT2{ 0, 0 } }, // 22
        Vertex{ DirectX::XMFLOAT3{ -1, -1, -1 }, DirectX::XMFLOAT3{ 0, 0, -1 }, DirectX::XMFLOAT2{ 0, 1 } } // 23
    };

    constexpr std::array<uint32_t, 36> CubeIndices =
    {
        0,  1,  2,  /**/ 0,  2,  3, // yz front side
        4,  5,  6,  /**/ 4,  6,  7, // yz back side

        8,  9,  10, /**/ 8,  10, 11, // xy back side
        12, 13, 14, /**/ 12, 14, 15, // xy front side

        16, 17, 18, /**/ 16, 18, 19, // xz front side
        20, 21, 22, /**/ 20, 22, 23  // xz back side
    };

    // clockwise triangles
    template<class fnVertexCollector, class fnIndexCollector, class fnUVCollector>
    void MakeGrid( size_t nx, size_t ny, float size_x, float size_y, fnVertexCollector&& vc, fnIndexCollector&& ic, fnUVCollector&& uvc );	

    constexpr std::pair<size_t/*nvertices*/, size_t/*nindices*/> GetGridSize( size_t nx, size_t ny ); // may nor work on MSVC17
    constexpr size_t GetGridNIndices( size_t nx, size_t ny );

    template<size_t nx, size_t ny>
    using GridVertices = std::array<Vertex, nx * ny>;
    template<size_t nx, size_t ny>
    using GridIndices = std::array<uint16_t, GetGridNIndices( nx, ny )>;

    // specialization of MakeGrid for static array
    template<size_t nx, size_t ny>
    std::pair<GridVertices<nx, ny>, GridIndices<nx, ny>> MakeArrayGrid( float size_x, float size_y );

    // fnNormalCollector = []( size_t vertex_idx ) -> XMFLOAT3&
    // unsafe, every index in indices must be < size(vertices)
    template<class VertexPosRandomAccessRange, class IndexRandomAccessRange, class fnNormalCollector>
    void CalcAverageNormals( const IndexRandomAccessRange& indices, const VertexPosRandomAccessRange& vertices, fnNormalCollector&& nc );
}

#include "GeomGeneration.hpp"