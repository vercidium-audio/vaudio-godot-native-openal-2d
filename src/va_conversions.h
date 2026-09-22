#pragma once

#include <cfloat>
#include <vector>

#include <godot_cpp/classes/line2d.hpp>
#include <godot_cpp/classes/polygon2d.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include "va_conversions_common.h"

using namespace godot;

inline VAVector ToVAudio(const Vector2 &v)
{
    return vaVectorCreate(v.x, v.y);
}

inline Vector2 FromVAudio(const VAVector &v)
{
    return Vector2(v.x, v.y);
}

// vaudio 2D primitives take position + rotation (radians) + scale separately - there is no VAMatrix in the 2D SDK, unlike 3D.
inline void Decompose(const Transform2D &transform, VAVector &out_position, float &out_rotation, Vector2 &out_scale)
{
    out_position = ToVAudio(transform.get_origin());
    out_rotation = transform.get_rotation();
    out_scale = transform.get_scale();
}

// Polygon2D's points are already local-space and directly match VAPolygonPrimitive's point array - no winding/flattening needed, unlike 3D's mesh-to-triangle conversion.
inline std::vector<VAVector> ConvertPolygonToVAudio(const PackedVector2Array &points)
{
    std::vector<VAVector> result;
    result.reserve(points.size());

    for (int i = 0; i < points.size(); i++)
        result.push_back(ToVAudio(points[i]));

    return result;
}

// Line2D's points form a connected polyline; vaudio has no polyline primitive, only per-segment VALinePrimitive - split into consecutive (start, end) pairs, one per segment.
inline std::vector<std::pair<VAVector, VAVector>> ConvertLineToVAudioSegments(const PackedVector2Array &points)
{
    std::vector<std::pair<VAVector, VAVector>> segments;

    if (points.size() < 2)
        return segments;

    segments.reserve(points.size() - 1);

    for (int i = 0; i < points.size() - 1; i++)
        segments.emplace_back(ToVAudio(points[i]), ToVAudio(points[i + 1]));

    return segments;
}
