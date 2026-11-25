// Human-reviewed

#pragma once

// TODO - what's this file for? It's missing primitive types below

// Which concrete ::VAXxxPrimitive type is stored in VAPrimitiveRef::primitive - needed since the C SDK's remove/destroy calls are typed, with no single untyped entry point.
enum class VAPrimitiveKind
{
    Box,
    Circle,
    Capsule,
    Line,
    Polygon,
};
