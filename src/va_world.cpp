#include "va_world.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "openal/al_manager.h"
#include "va_conversion_plugin.h"
#include "va_conversions.h"
#include "va_debugger_plugin.h"
#include "va_emitter.h"
#include "va_engine_util.h"

#include <cmath>

namespace va_godot
{

void VAWorld::_bind_methods()
{
    // Rebinds the inherited Node2D "position" property to VAWorld's own accessors, so moving this node also updates vaWorldSetPosition - see va_world_properties.cpp.
    ClassDB::bind_method(D_METHOD("get_position"), &VAWorld::get_position);
    ClassDB::bind_method(D_METHOD("set_position", "value"), &VAWorld::set_position);
    ClassDB::bind_method(D_METHOD("get_bounds_size"), &VAWorld::get_bounds_size);
    ClassDB::bind_method(D_METHOD("set_bounds_size", "value"), &VAWorld::set_bounds_size);
    ClassDB::bind_method(D_METHOD("get_bounds_color"), &VAWorld::get_bounds_color);
    ClassDB::bind_method(D_METHOD("set_bounds_color", "value"), &VAWorld::set_bounds_color);
    ClassDB::bind_method(D_METHOD("get_epsilon"), &VAWorld::get_epsilon);
    ClassDB::bind_method(D_METHOD("set_epsilon", "value"), &VAWorld::set_epsilon);
    ClassDB::bind_method(D_METHOD("get_collision_layers"), &VAWorld::get_collision_layers);
    ClassDB::bind_method(D_METHOD("set_collision_layers", "value"), &VAWorld::set_collision_layers);

    ADD_GROUP("World", "");

    // Without this, ClassDB still resolves the inherited "position" property to Node2D's own accessors, so set_position (bound above) would never be called through the property system.
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position", PROPERTY_HINT_RANGE, "-1000,1000,1,or_less,or_greater"), "set_position", "get_position");

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "bounds_size", PROPERTY_HINT_RANGE, "1,1000,1,or_greater"), "set_bounds_size", "get_bounds_size");
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "bounds_color"), "set_bounds_color", "get_bounds_color");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "epsilon"), "set_epsilon", "get_epsilon");

    ADD_GROUP("Layers", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layers", PROPERTY_HINT_LAYERS_2D_PHYSICS), "set_collision_layers", "get_collision_layers");

    ClassDB::bind_method(D_METHOD("get_master_volume"), &VAWorld::get_master_volume);
    ClassDB::bind_method(D_METHOD("set_master_volume", "value"), &VAWorld::set_master_volume);
    ClassDB::bind_method(D_METHOD("get_distance_model"), &VAWorld::get_distance_model);
    ClassDB::bind_method(D_METHOD("set_distance_model", "value"), &VAWorld::set_distance_model);
    ClassDB::bind_method(D_METHOD("get_reverb_only"), &VAWorld::get_reverb_only);
    ClassDB::bind_method(D_METHOD("set_reverb_only", "value"), &VAWorld::set_reverb_only);

    // OpenAL settings, forwarded to the ALManager singleton - if multiple VAWorlds exist in a scene, the last write wins.
    ADD_GROUP("OpenAL", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "master_volume"), "set_master_volume", "get_master_volume");
    // Matches vaudio-godot-mono-openal-2d's ALDistanceModel enum order/values (AL/al.h).
    ADD_PROPERTY(PropertyInfo(Variant::INT, "distance_model", PROPERTY_HINT_ENUM, "None:0,InverseDistance:53249,InverseDistanceClamped:53250,LinearDistance:53251,LinearDistanceClamped:53252,ExponentDistance:53253,ExponentDistanceClamped:53254"), "set_distance_model", "get_distance_model");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reverb_only"), "set_reverb_only", "get_reverb_only");

    ClassDB::bind_method(D_METHOD("get_maximum_grouped_eax_count"), &VAWorld::get_maximum_grouped_eax_count);
    ClassDB::bind_method(D_METHOD("set_maximum_grouped_eax_count", "value"), &VAWorld::set_maximum_grouped_eax_count);

    ADD_GROUP("Reverb", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_grouped_eax_count", PROPERTY_HINT_RANGE, "1,32,1,or_greater"), "set_maximum_grouped_eax_count", "get_maximum_grouped_eax_count");

    ClassDB::bind_method(D_METHOD("get_meters_per_unit"), &VAWorld::get_meters_per_unit);
    ClassDB::bind_method(D_METHOD("set_meters_per_unit", "value"), &VAWorld::set_meters_per_unit);
    ClassDB::bind_method(D_METHOD("get_speed_of_sound"), &VAWorld::get_speed_of_sound);
    ClassDB::bind_method(D_METHOD("set_speed_of_sound", "value"), &VAWorld::set_speed_of_sound);
    ClassDB::bind_method(D_METHOD("get_humidity"), &VAWorld::get_humidity);
    ClassDB::bind_method(D_METHOD("set_humidity", "value"), &VAWorld::set_humidity);
    ClassDB::bind_method(D_METHOD("get_temperature"), &VAWorld::get_temperature);
    ClassDB::bind_method(D_METHOD("set_temperature", "value"), &VAWorld::set_temperature);
    ClassDB::bind_method(D_METHOD("get_pressure"), &VAWorld::get_pressure);
    ClassDB::bind_method(D_METHOD("set_pressure", "value"), &VAWorld::set_pressure);
    ClassDB::bind_method(D_METHOD("get_reference_frequency_lf"), &VAWorld::get_reference_frequency_lf);
    ClassDB::bind_method(D_METHOD("set_reference_frequency_lf", "value"), &VAWorld::set_reference_frequency_lf);
    ClassDB::bind_method(D_METHOD("get_reference_frequency_hf"), &VAWorld::get_reference_frequency_hf);
    ClassDB::bind_method(D_METHOD("set_reference_frequency_hf", "value"), &VAWorld::set_reference_frequency_hf);

    // meters_per_unit/speed_of_sound are also forwarded to the process-wide ALManager singleton (see va_world_properties.cpp), same last-write-wins caveat as OpenAL group properties above.
    ADD_GROUP("AirAbsorption", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "meters_per_unit", PROPERTY_HINT_RANGE, "0.0001,1.0,or_greater"), "set_meters_per_unit", "get_meters_per_unit");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_of_sound", PROPERTY_HINT_RANGE, "0.0001,1000.0,1,or_greater"), "set_speed_of_sound", "get_speed_of_sound");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "humidity", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_humidity", "get_humidity");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "temperature", PROPERTY_HINT_RANGE, "-273.15,100.0,1,or_greater"), "set_temperature", "get_temperature");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pressure", PROPERTY_HINT_RANGE, "0.0,1000000,1,or_greater"), "set_pressure", "get_pressure");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reference_frequency_lf", PROPERTY_HINT_RANGE, "0.0001,1000,1,or_greater"), "set_reference_frequency_lf", "get_reference_frequency_lf");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reference_frequency_hf", PROPERTY_HINT_RANGE, "0.0001,20000,1,or_greater"), "set_reference_frequency_hf", "get_reference_frequency_hf");

    ClassDB::bind_method(D_METHOD("get_emitters_outside_the_world_are_muffled"), &VAWorld::get_emitters_outside_the_world_are_muffled);
    ClassDB::bind_method(D_METHOD("set_emitters_outside_the_world_are_muffled", "value"), &VAWorld::set_emitters_outside_the_world_are_muffled);

    ADD_GROUP("Emitters", "");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emitters_outside_the_world_are_muffled"), "set_emitters_outside_the_world_are_muffled", "get_emitters_outside_the_world_are_muffled");

    ClassDB::bind_method(D_METHOD("get_maximum_concurrency_level"), &VAWorld::get_maximum_concurrency_level);
    ClassDB::bind_method(D_METHOD("set_maximum_concurrency_level", "value"), &VAWorld::set_maximum_concurrency_level);
    ClassDB::bind_method(D_METHOD("get_work_item_count"), &VAWorld::get_work_item_count);
    ClassDB::bind_method(D_METHOD("set_work_item_count", "value"), &VAWorld::set_work_item_count);
    ClassDB::bind_method(D_METHOD("get_pending_shutdown"), &VAWorld::get_pending_shutdown);
    ClassDB::bind_method(D_METHOD("set_pending_shutdown", "value"), &VAWorld::set_pending_shutdown);

    ADD_GROUP("Threading", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_concurrency_level", PROPERTY_HINT_RANGE, "0,32,1,or_greater"), "set_maximum_concurrency_level", "get_maximum_concurrency_level");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "work_item_count", PROPERTY_HINT_RANGE, "1,256,1,or_greater"), "set_work_item_count", "get_work_item_count");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pending_shutdown"), "set_pending_shutdown", "get_pending_shutdown");

    ClassDB::bind_method(D_METHOD("get_rendering_enabled"), &VAWorld::get_rendering_enabled);
    ClassDB::bind_method(D_METHOD("set_rendering_enabled", "value"), &VAWorld::set_rendering_enabled);

    ClassDB::bind_method(D_METHOD("get_sync_viewport"), &VAWorld::get_sync_viewport);
    ClassDB::bind_method(D_METHOD("set_sync_viewport", "value"), &VAWorld::set_sync_viewport);

    ADD_GROUP("Rendering", "");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rendering_enabled"), "set_rendering_enabled", "get_rendering_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sync_viewport"), "set_sync_viewport", "get_sync_viewport");

    // Read-only timing stats (milliseconds) - no ADD_PROPERTY, called directly from GDScript like methods (world.get_main_thread_time()).
    ClassDB::bind_method(D_METHOD("get_main_thread_time"), &VAWorld::get_main_thread_time);
    ClassDB::bind_method(D_METHOD("get_raytracing_time"), &VAWorld::get_raytracing_time);
    ClassDB::bind_method(D_METHOD("get_preparation_time"), &VAWorld::get_preparation_time);
    ClassDB::bind_method(D_METHOD("get_analysis_time"), &VAWorld::get_analysis_time);

    // Read-only GroupedEAX stats, same no-ADD_PROPERTY rationale as the timing stats above.
    ClassDB::bind_method(D_METHOD("get_grouped_eax_count"), &VAWorld::get_grouped_eax_count);
    ClassDB::bind_method(D_METHOD("get_grouped_eax_gain_lf", "index"), &VAWorld::get_grouped_eax_gain_lf);
    ClassDB::bind_method(D_METHOD("get_grouped_eax_gain_hf", "index"), &VAWorld::get_grouped_eax_gain_hf);
    ClassDB::bind_method(D_METHOD("get_grouped_eax_decay_time", "index"), &VAWorld::get_grouped_eax_decay_time);

    // No signal on purpose - it would fire inside vaWorldUpdate, and a handler that edits the world would re-enter it
    ClassDB::bind_method(D_METHOD("get_raytrace_count"), &VAWorld::get_raytrace_count);

    // Exports world settings/materials/primitives/emitters to a binary file (vaWorldExport) - callable from GDScript, e.g. wired to a UI button.
    ClassDB::bind_method(D_METHOD("export_to_file", "file_path"), &VAWorld::export_to_file);

    // Exposes the 23 built-in material names and their metadata key to GDScript so the "Vercidium Audio" editor plugin's material dropdown can't drift out of sync.
    ClassDB::bind_static_method("VAWorld", D_METHOD("get_builtin_material_names"), &VAWorld::get_builtin_material_names);
    ClassDB::bind_static_method("VAWorld", D_METHOD("get_material_meta_key"), &VAWorld::get_material_meta_key);
    ClassDB::bind_static_method("VAWorld", D_METHOD("get_use_flat_transmission_meta_key"), &VAWorld::get_use_flat_transmission_meta_key);
}

VAWorld::VAWorld()
{
    // The world should only exist at runtime, not in the editor; world stays nullptr and every other method already null-checks it.
    if (IS_EDITOR_HINT())
        return;

    world = vaWorldCreate();

    // These three calls are guaranteed to pass, no need to check result.
    vaWorldSetCoordinateSystem(world, VACoordinateSystemGodot2D);
    vaWorldSetUserData(world, this);
    vaWorldSetOnReverbUpdatedCallback(world, &VAWorld::on_reverb_updated_trampoline);

    // These setters handle error checking for us.
    set_position(get_position());
    set_bounds_size(bounds_size);
    set_epsilon(epsilon);
    set_maximum_grouped_eax_count(maximum_grouped_eax_count);
    set_meters_per_unit(meters_per_unit);
    set_speed_of_sound(speed_of_sound);
    set_master_volume(master_volume);
    set_distance_model(distance_model);
    set_reverb_only(reverb_only);
    set_humidity(humidity);
    set_temperature(temperature);
    set_pressure(pressure);
    set_reference_frequency_lf(reference_frequency_lf);
    set_reference_frequency_hf(reference_frequency_hf);
    set_emitters_outside_the_world_are_muffled(emitters_outside_the_world_are_muffled);
    set_maximum_concurrency_level(maximum_concurrency_level);
    set_work_item_count(work_item_count);
    set_rendering_enabled(rendering_enabled);

    // ALManager is initialized at GDExtension module-init time, so it's safe to create AL objects here.
    listener_reverb_effect.create();

    set_process(true);
}

// The bounds are always an axis-aligned box (vaWorldSetPosition/vaWorldSetSize take no rotation/scale), so hide the rest of Node2D's transform and only expose position.
void VAWorld::_validate_property(PropertyInfo &p_property) const
{
    if (p_property.name == StringName("rotation") ||
        p_property.name == StringName("rotation_degrees") ||
        p_property.name == StringName("scale") ||
        p_property.name == StringName("skew") ||
        p_property.name == StringName("transform"))
    {
        p_property.usage = PROPERTY_USAGE_NONE;
    }
}

void VAWorld::_ready()
{
    if (IS_EDITOR_HINT())
    {
        // Scan for unknown vercidium_audio_material values, so warnings appear while editing. get_tree() can be null if this node isn't inside the scene tree yet.
        Node *root = get_tree() ? get_tree()->get_root() : nullptr;

        if (root)
            validate_materials_in_editor(root);

        return;
    }

    // Wait a frame to ensure all children/siblings have been added to the scene.
    callable_mp(this, &VAWorld::init_scene).call_deferred();
}

void VAWorld::_exit_tree()
{
    if (IS_EDITOR_HINT())
        return;

    is_shutting_down = true;

    if (get_tree())
    {
        if (get_tree()->is_connected("node_added", callable_mp(this, &VAWorld::on_node_added)))
            get_tree()->disconnect("node_added", callable_mp(this, &VAWorld::on_node_added));

        if (get_tree()->is_connected("node_removed", callable_mp(this, &VAWorld::on_node_removed)))
            get_tree()->disconnect("node_removed", callable_mp(this, &VAWorld::on_node_removed));

        // get_current_scene() can be null if the tree has no scene loaded (e.g. exiting during shutdown).
        Node *scene_root = get_tree()->get_current_scene();

        if (scene_root)
            remove_primitive(scene_root, true);
    }
}

// Draws the bounds AABB as a filled+outlined rect in the 2D viewport, editor-only - matches VAWorldGizmo.cs's Node2D._draw() override in the Mono addon. 2D has no drag handles, unlike 3D's EditorNode3DGizmoPlugin approach - bounds_size is edited directly in the inspector.
void VAWorld::_draw()
{
    if (!IS_EDITOR_HINT())
        return;

    Rect2 rect(Vector2(), bounds_size);

    draw_rect(rect, bounds_color, true);
    draw_rect(rect, Color(bounds_color, 1.0f), false, 2.0f);
}

void VAWorld::_process(double delta)
{
    if (IS_EDITOR_HINT())
    {
        if (sync_viewport)
            send_viewport_camera_to_running_game();

        return;
    }

    if (listener)
    {
        ALManager *manager = ALManager::get_singleton();

        if (manager && manager->is_initialized())
        {
            Vector2 position = listener->get_global_position();
            float rotation = listener->get_global_rotation();
            // Top-down: rotation 0 faces screen-up. Up is -Z because Godot's Y-down XY plane is mirrored relative to OpenAL's right-handed space.
            Vector3 forward(sinf(rotation), -cosf(rotation), 0.0f);
            Vector3 up(0.0f, 0.0f, -1.0f);

            manager->set_listener_position(Vector3(position.x, position.y, 0.0f));
            manager->set_listener_orientation(forward, up);
        }
    }

    if (world)
    {
        VAResult result = vaWorldUpdate(world);

        if (result != VA_SUCCESS && result != VA_STILL_RUNNING)
        {
            VA_ERROR_NAMED_RESULT(result, "Update failed");
        }
    }
}

void VAWorld::send_viewport_camera_to_running_game()
{
    if (!Engine::get_singleton()->has_singleton(VAConversionPlugin::DEBUGGER_BRIDGE_SINGLETON_NAME))
        return;

    SubViewport *viewport = EditorInterface::get_singleton()->get_editor_viewport_2d();

    if (!viewport)
        return;

    Object *singleton_object = Engine::get_singleton()->get_singleton(VAConversionPlugin::DEBUGGER_BRIDGE_SINGLETON_NAME);
    VADebuggerBridge *bridge = Object::cast_to<VADebuggerBridge>(singleton_object);
    VADebuggerPlugin *debugger_plugin = bridge ? bridge->get_debugger_plugin() : nullptr;

    if (!debugger_plugin)
        return;

    // The 2D editor has no Camera2D - it pans/zooms by setting the viewport's global canvas transform, so invert that to get the world-space view centre. Matches VAWorldDebugger.cs in the Mono addon.
    Transform2D canvas = viewport->get_global_canvas_transform();
    Transform2D view = canvas.affine_inverse();

    Vector2 centre = view.xform(viewport->get_visible_rect().size / 2.0f);

    // Canvas scale is in physical pixels, but the debug window's zoom is in logical pixels (framebuffer / GLFW content scale)
    float zoom = canvas.get_scale().x / get_screen_content_scale();

    debugger_plugin->sync_viewport_camera(centre, view.get_rotation(), zoom);
}

// Matches GLFW's glfwGetWindowContentScale. Windows' screen_get_scale always returns 1, so derive it from the effective monitor DPI instead
float VAWorld::get_screen_content_scale()
{
    DisplayServer *display_server = DisplayServer::get_singleton();

    float scale = OS::get_singleton()->get_name() == "Windows"
        ? display_server->screen_get_dpi() / 96.0f
        : display_server->screen_get_scale();

    return scale > 0.0f ? scale : 1.0f;
}

void VAWorld::on_reverb_updated()
{
    if (!listener || !listener->get_handle())
    {
        // Don't warn during teardown - a reverb update can still be in flight after the listener node has unregistered but before this VAWorld node is destroyed.
        if (!warned_missing_listener && !is_shutting_down)
        {
            VA_WARN_NAMED("Has no VAListener node, so reverb cannot be updated. Add a VAListener node to this scene.");
            warned_missing_listener = true;
        }

        return;
    }

    VAEAXReverb *eax = vaEmitterGetEAX(listener->get_handle());

    if (eax)
    {
        listener_reverb_effect.set_params(CopyReverbParams(eax));
    }

    int grouped_eax_count = vaWorldGetGroupedEAXCount(world);
    const VAEAXReverb **grouped_eax = vaWorldGetGroupedEAX(world);

    for (int i = 0; i < grouped_eax_count; i++)
    {
        if ((int)grouped_reverb_effects.size() <= i)
        {
            std::unique_ptr<ALReverbEffect> new_effect = std::make_unique<ALReverbEffect>();
            new_effect->create();
            grouped_reverb_effects.push_back(std::move(new_effect));
        }

        VAEAXReverbParams params = CopyReverbParams(grouped_eax[i]);

        // Blend in this slot's gain/direction relative to the listener, so a grouped zone the listener hasn't raytraced
        // yet holds its last pan rather than snapping to silence/center - vaEAXReverbGetRelative* return NULL for "no entry yet".
        float *relative_gain = vaEAXReverbGetRelativeGain(grouped_eax[i], listener->get_handle());
        VAVector *relative_direction = vaEAXReverbGetRelativeDirection(grouped_eax[i], listener->get_handle());

        if (relative_gain)
        {
            params.effectSlotGain = MIN(1.0f, MAX(0.0f, *relative_gain));
        }

        if (relative_direction)
        {
            // Matches VAWorldReverbDimension.cs's ApplyGroupedEAXPan. The SDK returns listener space (X+ right, Y+ forward), and EFX pan is X+ right, Z+ forward
            VAVector pan = vaWorldCalculateListenerRelativePan(world, *relative_direction, listener->get_global_rotation());

            params.reflectionsPan[0] = pan.x;
            params.reflectionsPan[1] = 0.0f;
            params.reflectionsPan[2] = pan.y;
            params.lateReverbPan[0] = pan.x;
            params.lateReverbPan[1] = 0.0f;
            params.lateReverbPan[2] = pan.y;
        }

        grouped_reverb_effects[i]->set_params(params);
    }
}

} // namespace va_godot
