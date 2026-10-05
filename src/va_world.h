#pragma once

#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/line2d.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/polygon2d.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>

extern "C"
{
#include <vaudio.h>
}

#include "openal/al_reverb.h"
#include "va_conversions.h"
#include "va_engine_util.h"
#include "va_primitive_ref.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

using namespace godot;

// Metadata keys used to tag scene-tree nodes with vaudio material/primitive/propagate state. Defined once in common/va_world_primitives_common.cpp since they're dimension-agnostic; declared here (file scope, not namespaced, matching how they're called unqualified from within namespace va_godot) so each repo's own va_world_primitives.cpp (whose create_primitive/add_primitive/remove_primitive overloads differ per dimension) can still read/write the same metadata.
const StringName &PrimitiveMetaKey();
const StringName &MaterialMetaKey();
const StringName &UseFlatTransmissionMetaKey();
const StringName &PropagateMetaKey();

namespace va_godot
{

class VACustomMaterial;
class VAEmitter;
class VAListener;

// Controls which child nodes a material applies to. Does not affect child nodes that have their own material
enum class PropagateMode
{
    All,
    Colliders,
    Visuals,
};

// Copies a raw ::VAEAXReverb's fields into an OpenAL-facing VAEAXReverbParams. Defined once in common/va_world_common.cpp since it's dimension-agnostic; declared here so each repo's own va_world.cpp (which computes the listener-relative pan differently) can still call it from on_reverb_updated().
VAEAXReverbParams CopyReverbParams(const VAEAXReverb *eax);

// Name collision: the vaudio C SDK's opaque handle type is also called "VAWorld" (global namespace); inside va_godot, 'VAWorld' means this class and '::VAWorld' the SDK handle.
// This is a Node2D purely so the editor can draw a gizmo for the bounds_position/bounds_size AABB - the node's own transform is otherwise unused by vaudio.
class VAWorld : public Node2D
{
    GDCLASS(VAWorld, Node2D);

private:
    ::VAWorld *world = nullptr;

    // Material id -> name. Names rather than node pointers, since a VACustomMaterial can be freed while the world keeps using its material
    std::unordered_map<int, String> custom_materials;

    // The current VAListener. All listeners share one SDK emitter handle, and only this one points at it.
    va_godot::VAEmitter *listener = nullptr;

    // Every VAListener attached to this world, current or not. The first is promoted when the current listener leaves the tree.
    std::vector<va_godot::VAListener *> listeners;

    // Contains every non-listener emitter. When the listener is finally added, this list is processed
    std::vector<va_godot::VAEmitter *> registered_emitters;

    // (Re-)adds every registered_emitters entry as a target of the current listener. Safe to call repeatedly - vaEmitterAddTarget treats an existing target as a no-op.
    void wire_pending_targets();

    bool wire_pending_targets_queued = false;

    ALReverbEffect listener_reverb_effect;
    std::vector<std::unique_ptr<ALReverbEffect>> grouped_reverb_effects;

    static void on_reverb_updated_trampoline(::VAWorld *world);
    void on_reverb_updated();

    bool warned_missing_listener = false;

    bool is_shutting_down = false;

    bool pending_shutdown = false;
    int raytrace_count = 0;
    bool rendering_enabled = true;
    bool sync_viewport = true;

    void init_scene();
    void on_node_added(Node *node);
    void on_node_removed(Node *node);

    // Editor-only: relays the 2D editor viewport's camera transform/zoom to every active game session, polled from _process while IS_EDITOR_HINT() via sync_viewport.
    void send_viewport_camera_to_running_game();
    static float get_screen_content_scale();

    // Reports unknown material metadata strings when in the editor, not used at runtime.
    void validate_materials_in_editor(Node *node);

    VAMaterialType get_material(Node *node, bool warn_unknown = true);
    void resolve_inherited(Node *node, VAMaterialType &material, bool &use_flat_transmission, PropagateMode &filter);

    void add_primitive(Node *node, VAMaterialType material, bool use_flat_transmission, PropagateMode filter, bool recursive);
    void remove_primitive(Node *node, bool recursive);

    // The highest ancestor of node sitting directly under the scene tree root, or nullptr if node isn't under the tree.
    Node *top_level_scene_node(Node *node);

    // Re-scans the scene tree and rebuilds every primitive. Invoked when collision_layers changes at runtime.
    void rebuild_primitives();

    // Reads this node's own propagate metadata, falling back to the mode inherited from an ancestor.
    static PropagateMode read_propagate_filter(Node *node, PropagateMode inherited);

    // Whether a cascading material reaches this node, given a filter declared on an ancestor. Not static - reads collision_layers. Unlike 3D, visuals (Polygon2D/Line2D) are never filtered by layer mask - only colliders are.
    bool passes_propagate_filter(Node *node, PropagateMode filter);

    VAPrimitiveRef *attach_watcher(Node2D *node, void *primitive, VAPrimitiveKind kind, std::function<void()> update);

    void create_primitive(CollisionShape2D *collision_shape, VAMaterialType material);
    void create_primitive(Polygon2D *polygon, VAMaterialType material, bool use_flat_transmission);
    void create_primitive(Line2D *line, VAMaterialType material);

    void update_collision_shape_primitive(CollisionShape2D *collision_shape, VAPrimitiveRef *ref);

protected:
    static void _bind_methods();

public:
    VAWorld();
    ~VAWorld();

    void _ready() override;
    void _exit_tree() override;
    void _process(double delta) override;
    void _draw() override;
    void _validate_property(PropertyInfo &p_property) const;

    ::VAWorld *get_handle() const
    {
        return world;
    }

    bool register_custom_material(va_godot::VACustomMaterial *material);

    void sync_primitive(Node *node);

    static PackedStringArray get_builtin_material_names();

    static String get_material_meta_key();

    static String get_use_flat_transmission_meta_key();

    static String get_propagate_meta_key();

    void register_emitter(va_godot::VAEmitter *emitter, bool is_main_listener);

    void unregister_pending_target(va_godot::VAEmitter *emitter);

    void register_listener(va_godot::VAListener *node);

    void unregister_listener(va_godot::VAListener *node);

    // Hands the shared listener handle over to node
    void set_current_listener(va_godot::VAListener *node);

    // If node is the current listener, hands the shared listener handle over to another attached listener (if any)
    void release_current_listener(va_godot::VAListener *node);

    bool export_to_file(const String &file_path);

    va_godot::VAEmitter *get_listener() const
    {
        return listener;
    }

    ALReverbEffect *get_reverb_effect(::VAEmitter *emitter);

    int get_grouped_eax_count() const;
    float get_grouped_eax_gain_lf(int index) const;
    float get_grouped_eax_gain_hf(int index) const;
    float get_grouped_eax_decay_time(int index) const;

    // Number of completed raytracing passes (OnReverbUpdated callbacks), so tests can wait for fresh results after changing the scene
    int get_raytrace_count() const
    {
        return raytrace_count;
    }

    bool get_pending_shutdown() const
    {
        return pending_shutdown;
    }

    void set_pending_shutdown(bool value)
    {
        pending_shutdown = value;

        if (world)
            vaWorldSetPendingShutdown(world, value);
    }

    bool get_rendering_enabled() const
    {
        return rendering_enabled;
    }

    void set_rendering_enabled(bool value)
    {
        rendering_enabled = value;

        if (world)
            vaWorldSetRenderingEnabled(world, value);
    }

    bool get_sync_viewport() const
    {
        return sync_viewport;
    }

    void set_sync_viewport(bool value)
    {
        sync_viewport = value;
    }

    Vector2 get_position() const
    {
        return Node2D::get_position();
    }
    void set_position(const Vector2 &value);

    Vector2 get_bounds_size() const
    {
        return bounds_size;
    }
    void set_bounds_size(Vector2 value);

    uint32_t get_collision_layers() const
    {
        return collision_layers;
    }
    void set_collision_layers(uint32_t value);

    Color get_bounds_color() const
    {
        return bounds_color;
    }
    void set_bounds_color(Color value);

    float get_epsilon() const
    {
        return epsilon;
    }
    void set_epsilon(float value);

    int get_maximum_grouped_eax_count() const
    {
        return maximum_grouped_eax_count;
    }
    void set_maximum_grouped_eax_count(int value);

    float get_meters_per_unit() const
    {
        return meters_per_unit;
    }
    void set_meters_per_unit(float value);

    float get_speed_of_sound() const
    {
        return speed_of_sound;
    }
    void set_speed_of_sound(float value);

    float get_master_volume() const
    {
        return master_volume;
    }
    void set_master_volume(float value);

    int get_distance_model() const
    {
        return distance_model;
    }
    void set_distance_model(int value);

    bool get_reverb_only() const
    {
        return reverb_only;
    }
    void set_reverb_only(bool value);

    float get_humidity() const
    {
        return humidity;
    }
    void set_humidity(float value);

    float get_temperature() const
    {
        return temperature;
    }
    void set_temperature(float value);

    float get_pressure() const
    {
        return pressure;
    }
    void set_pressure(float value);

    float get_reference_frequency_lf() const
    {
        return reference_frequency_lf;
    }
    void set_reference_frequency_lf(float value);

    float get_reference_frequency_hf() const
    {
        return reference_frequency_hf;
    }
    void set_reference_frequency_hf(float value);

    bool get_emitters_outside_the_world_are_muffled() const
    {
        return emitters_outside_the_world_are_muffled;
    }
    void set_emitters_outside_the_world_are_muffled(bool value);

    bool get_occlusion_rays_lose_energy_from_world_bounds() const
    {
        return occlusion_rays_lose_energy_from_world_bounds;
    }
    void set_occlusion_rays_lose_energy_from_world_bounds(bool value);

    int get_maximum_concurrency_level() const
    {
        return maximum_concurrency_level;
    }
    void set_maximum_concurrency_level(int value);

    int get_work_item_count() const
    {
        return work_item_count;
    }
    void set_work_item_count(int value);

    double get_main_thread_time() const;
    double get_preparation_time() const;
    double get_raytracing_time() const;
    double get_analysis_time() const;

private:
    Vector2 bounds_size = Vector2(200, 200);
    Color bounds_color = Color(0.0f, 0.0f, 0.0f, 0.25f);
    float epsilon = 0.01f;
    // A collider only inherits a cascading material if its body's collision layer is in this mask. A collider with its own material is always included. 0xFFFFF = all 20 layers. Unlike 3D, there is no render_layers - visuals are never filtered by layer mask in 2D.
    uint32_t collision_layers = 0xFFFFF;
    int maximum_grouped_eax_count = 3;
    float meters_per_unit = 1.0f;
    float speed_of_sound = 343.0f;
    float master_volume = 1.0f;
    int distance_model = AL_INVERSE_DISTANCE_CLAMPED;
    bool reverb_only = false;
    float humidity = 0.1f;
    float temperature = 26.0f;
    float pressure = 101325.0f;
    float reference_frequency_lf = 300.0f;
    float reference_frequency_hf = 4000.0f;
    bool emitters_outside_the_world_are_muffled = true;
    bool occlusion_rays_lose_energy_from_world_bounds = false;
    int maximum_concurrency_level = 0;
    int work_item_count = 128;
};

} // namespace va_godot
