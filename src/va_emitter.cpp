#include "va_emitter.h"

// The rest of VAEmitter is dimension-agnostic and lives in common/va_emitter.cpp.

namespace va_godot
{

Vector2 VAEmitter::get_va_position() const
{
    VAVector position = vaEmitterGetPosition(get_handle());
    return Vector2(position.x, position.y);
}

} // namespace va_godot
