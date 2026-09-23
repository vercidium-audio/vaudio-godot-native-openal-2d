#pragma once

#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>

using namespace godot;

namespace va_godot
{

class VADebuggerPlugin : public EditorDebuggerPlugin
{
    GDCLASS(VADebuggerPlugin, EditorDebuggerPlugin);

protected:
    static void _bind_methods();

public:
    // Defined in common/va_debugger_plugin_common.cpp - identical between 2D and 3D.
    void sync_primitive(const String &scene_root_name, const NodePath &node_path, const String &material,
        const Variant &use_flat_transmission, const String &propagate);

    void sync_material_properties(const String &scene_root_name, const NodePath &node_path, const String &node_name,
        bool is_custom_material, int material_type, const String &custom_material_name, float absorption_lf,
        float absorption_hf, float scattering, float transmission_lf, float transmission_hf,
        float flat_transmission_lf, float flat_transmission_hf, const Color &color);

    // Relays the editor's viewport camera transform to every active game session, polled every frame by VAWorld's
    // sync_viewport property via Engine::get_singleton. 2D has no yaw/pitch/FOV - just a screen-space centre position,
    // a single rotation angle, and a uniform zoom scale, matching vaWorldSetCameraRotation/vaWorldSetCameraZoom's
    // shape (there is no vaWorldSetCameraPosition in the 2D native SDK at all).
    void sync_viewport_camera(const Vector2 &position, float rotation, float zoom);
};

} // namespace va_godot
