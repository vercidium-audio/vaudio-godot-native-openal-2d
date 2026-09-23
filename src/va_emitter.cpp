#include "va_emitter.h"

#include "va_engine_util.h"

// The rest of VAEmitter is dimension-agnostic and lives in common/va_emitter.cpp.

namespace va_godot
{

// Brand green, matching VAEmitterGizmo.cs's GizmoColor in the Mono addon and icons/vercidium.svg.
static const Color VA_EMITTER_GIZMO_COLOR = Color(0x85 / 255.0f, 0xff / 255.0f, 0xa4 / 255.0f);

static constexpr float VA_EMITTER_GIZMO_RADIUS = 6.0f;

Vector2 VAEmitter::get_va_position() const
{
    VAVector position = vaEmitterGetPosition(get_handle());
    return Vector2(position.x, position.y);
}

// Draws a filled circle at this node's origin in the 2D viewport, editor-only - matches VAEmitterGizmo.cs's Node2D._draw() override in the Mono addon. 3D has no equivalent override; it draws a solid sphere via VANodeGizmoPlugin (an EditorNode3DGizmoPlugin) instead, since Node3D has no _draw().
void VAEmitter::_draw()
{
    if (!IS_EDITOR_HINT())
        return;

    draw_circle(Vector2(), VA_EMITTER_GIZMO_RADIUS, VA_EMITTER_GIZMO_COLOR);
    draw_arc(Vector2(), VA_EMITTER_GIZMO_RADIUS, 0.0f, Math_TAU, 24, Color(VA_EMITTER_GIZMO_COLOR, 1.0f), 1.5f);
}

} // namespace va_godot
