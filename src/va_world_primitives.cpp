#include "va_world.h"

#include <godot_cpp/classes/capsule_shape2d.hpp>
#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/collision_object2d.hpp>
#include <godot_cpp/classes/concave_polygon_shape2d.hpp>
#include <godot_cpp/classes/convex_polygon_shape2d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/segment_shape2d.hpp>
#include <godot_cpp/classes/separation_ray_shape2d.hpp>
#include <godot_cpp/classes/shape2d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/world_boundary_shape2d.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "va_conversions.h"
#include "va_engine_util.h"

// Port of VAWorldPrimitives.cs. CollisionShape2D rectangle/circle/capsule/segment/separation-ray/world-boundary/convex-polygon/concave-polygon, Polygon2D, and Line2D are covered.

namespace va_godot
{

// Unlike 3D (which filters both render_layers and collision_layers), 2D only filters colliders by collision_layers - visuals (Polygon2D/Line2D) are never filtered by layer mask, matching PassesPropagationFilter in VAWorldPrimitives.cs.
bool VAWorld::passes_propagate_filter(Node *node, PropagateMode filter)
{
    bool is_collider = Object::cast_to<CollisionShape2D>(node) != nullptr;

    if (filter == PropagateMode::Colliders && !is_collider)
        return false;
    if (filter == PropagateMode::Visuals && is_collider)
        return false;

    if (is_collider)
    {
        if (CollisionObject2D *body = Object::cast_to<CollisionObject2D>(node->get_parent()))
            return (body->get_collision_layer() & collision_layers) != 0;
    }

    return true;
}

VAPrimitiveRef *VAWorld::attach_watcher(Node2D *node, void *primitive, VAPrimitiveKind kind, std::function<void()> update)
{
    TransformWatcher *watcher = memnew(TransformWatcher);
    watcher->set_on_transform_changed(update);
    node->add_child(watcher);

    VAPrimitiveRef *ref = memnew(VAPrimitiveRef);
    ref->primitive = primitive;
    ref->kind = kind;
    ref->watcher = watcher;
    return ref;
}

// WorldBoundaryShape2D is an infinite half-plane edge - approximate with a long line along the boundary, sized to comfortably cover the world, matching CreateVAudioPrimitive(WorldBoundaryShape2D) in VAWorldPrimitives.cs.
static void ComputeWorldBoundaryLine(const Ref<WorldBoundaryShape2D> &world_boundary, CollisionShape2D *collision_shape, ::VAWorld *world, Vector2 &out_start, Vector2 &out_end)
{
    Vector2 normal = world_boundary->get_normal();
    Vector2 along(-normal.y, normal.x);
    Vector2 origin = collision_shape->get_global_position() + normal * world_boundary->get_distance();

    VAVector world_size = vaWorldGetSize(world);
    float half = Vector2(world_size.x, world_size.y).length() * 2.0f;

    out_start = origin - along * half;
    out_end = origin + along * half;
}

void VAWorld::create_primitive(CollisionShape2D *collision_shape, VAMaterialType material)
{
    if (collision_shape->has_meta(PrimitiveMetaKey()))
    {
        return;
    }

    Ref<Shape2D> shape = collision_shape->get_shape();
    if (shape.is_null())
    {
        return;
    }

    Transform2D global_transform = collision_shape->get_global_transform();

    VAVector position;
    float rotation;
    Vector2 scale;
    Decompose(global_transform, position, rotation, scale);

    void *prim = nullptr;
    VAPrimitiveKind kind;

    if (Ref<RectangleShape2D> rect = shape; rect.is_valid())
    {
        VABoxPrimitive *p = vaBoxPrimitiveCreate();

        VAResult material_result = vaBoxPrimitiveSetMaterial(p, material);
        if (material_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D rectangle) failed to set material for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
        }

        vaBoxPrimitiveSetPosition(p, position);
        vaBoxPrimitiveSetSize(p, ToVAudio(rect->get_size() * scale));
        vaBoxPrimitiveSetRotation(p, rotation);

        VAResult add_result = vaWorldAddPrimitive_(world, p);
        if (add_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D rectangle) failed to add '",
                collision_shape->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
            vaBoxPrimitiveDestroy(p);
            return;
        }

        prim = p;
        kind = VAPrimitiveKind::Box;
    }
    else if (Ref<CircleShape2D> circle = shape; circle.is_valid())
    {
        VACirclePrimitive *p = vaCirclePrimitiveCreate();

        VAResult material_result = vaCirclePrimitiveSetMaterial(p, material);
        if (material_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D circle) failed to set material for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
        }

        vaCirclePrimitiveSetCenter(p, position);
        vaCirclePrimitiveSetRadius(p, circle->get_radius() * scale.x);

        VAResult add_result = vaWorldAddPrimitive_(world, p);
        if (add_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D circle) failed to add '",
                collision_shape->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
            vaCirclePrimitiveDestroy(p);
            return;
        }

        prim = p;
        kind = VAPrimitiveKind::Circle;
    }
    else if (Ref<CapsuleShape2D> capsule = shape; capsule.is_valid())
    {
        // TODO - make a capsule primitive in 2D; approximated with an oval, matching VAWorldPrimitives.cs.
        VAOvalPrimitive *p = vaOvalPrimitiveCreate();

        VAResult material_result = vaOvalPrimitiveSetMaterial(p, material);
        if (material_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D capsule) failed to set material for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
        }

        vaOvalPrimitiveSetCenter(p, position);
        vaOvalPrimitiveSetRadiusX(p, capsule->get_radius() * scale.x);
        vaOvalPrimitiveSetRadiusY(p, (capsule->get_height() / 2.0f) * scale.y);
        vaOvalPrimitiveSetRotation(p, rotation);

        VAResult add_result = vaWorldAddPrimitive_(world, p);
        if (add_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D capsule) failed to add '",
                collision_shape->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
            vaOvalPrimitiveDestroy(p);
            return;
        }

        prim = p;
        kind = VAPrimitiveKind::Oval;
    }
    else if (Ref<SegmentShape2D> segment = shape; segment.is_valid())
    {
        VALinePrimitive *p = vaLinePrimitiveCreate();

        VAResult material_result = vaLinePrimitiveSetMaterial(p, material);
        if (material_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D segment) failed to set material for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
        }

        vaLinePrimitiveSetStart(p, ToVAudio(global_transform.xform(segment->get_a())));
        vaLinePrimitiveSetEnd(p, ToVAudio(global_transform.xform(segment->get_b())));

        VAResult add_result = vaWorldAddPrimitive_(world, p);
        if (add_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D segment) failed to add '",
                collision_shape->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
            vaLinePrimitiveDestroy(p);
            return;
        }

        prim = p;
        kind = VAPrimitiveKind::Line;
    }
    else if (Ref<SeparationRayShape2D> ray = shape; ray.is_valid())
    {
        VALinePrimitive *p = vaLinePrimitiveCreate();

        VAResult material_result = vaLinePrimitiveSetMaterial(p, material);
        if (material_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D separation ray) failed to set material for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
        }

        vaLinePrimitiveSetStart(p, position);
        vaLinePrimitiveSetEnd(p, ToVAudio(global_transform.xform(Vector2(0, ray->get_length()))));

        VAResult add_result = vaWorldAddPrimitive_(world, p);
        if (add_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D separation ray) failed to add '",
                collision_shape->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
            vaLinePrimitiveDestroy(p);
            return;
        }

        prim = p;
        kind = VAPrimitiveKind::Line;
    }
    else if (Ref<WorldBoundaryShape2D> world_boundary = shape; world_boundary.is_valid())
    {
        Vector2 start, end;
        ComputeWorldBoundaryLine(world_boundary, collision_shape, world, start, end);

        VALinePrimitive *p = vaLinePrimitiveCreate();

        VAResult material_result = vaLinePrimitiveSetMaterial(p, material);
        if (material_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D world boundary) failed to set material for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
        }

        vaLinePrimitiveSetStart(p, ToVAudio(start));
        vaLinePrimitiveSetEnd(p, ToVAudio(end));

        VAResult add_result = vaWorldAddPrimitive_(world, p);
        if (add_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D world boundary) failed to add '",
                collision_shape->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
            vaLinePrimitiveDestroy(p);
            return;
        }

        prim = p;
        kind = VAPrimitiveKind::Line;
    }
    else if (Ref<ConvexPolygonShape2D> convex_polygon = shape; convex_polygon.is_valid())
    {
        std::vector<VAVector> points = ConvertPolygonToVAudio(convex_polygon->get_points());

        if ((int)points.size() < 3)
        {
            return;
        }

        VAPolygonPrimitive *p = nullptr;
        VAResult create_result = vaPolygonPrimitiveCreate(points.data(), (int)points.size(), &p);
        if (create_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D convex polygon) failed to create the polygon primitive for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(create_result), ")");
            return;
        }

        VAResult material_result = vaPolygonPrimitiveSetMaterial(p, material);
        if (material_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D convex polygon) failed to set material for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
        }

        vaPolygonPrimitiveSetPosition(p, position);
        vaPolygonPrimitiveSetRotation(p, rotation);
        vaPolygonPrimitiveSetScale(p, ToVAudio(scale));
        vaPolygonPrimitiveSetEnclosed(p, true);

        VAResult add_result = vaWorldAddPrimitive_(world, p);
        if (add_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D convex polygon) failed to add '",
                collision_shape->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
            vaPolygonPrimitiveDestroy(p);
            return;
        }

        prim = p;
        kind = VAPrimitiveKind::Polygon;
    }
    else if (Ref<ConcavePolygonShape2D> concave_polygon = shape; concave_polygon.is_valid())
    {
        std::vector<VAVector> points = ConvertPolygonToVAudio(concave_polygon->get_segments());

        if ((int)points.size() < 3)
        {
            return;
        }

        VAPolygonPrimitive *p = nullptr;
        VAResult create_result = vaPolygonPrimitiveCreate(points.data(), (int)points.size(), &p);
        if (create_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D concave polygon) failed to create the polygon primitive for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(create_result), ")");
            return;
        }

        VAResult material_result = vaPolygonPrimitiveSetMaterial(p, material);
        if (material_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D concave polygon) failed to set material for '",
                collision_shape->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
        }

        vaPolygonPrimitiveSetPosition(p, position);
        vaPolygonPrimitiveSetRotation(p, rotation);
        vaPolygonPrimitiveSetScale(p, ToVAudio(scale));
        vaPolygonPrimitiveSetEnclosed(p, false);

        VAResult add_result = vaWorldAddPrimitive_(world, p);
        if (add_result != VA_SUCCESS)
        {
            VA_ERROR(
                "VAWorld::create_primitive(CollisionShape2D concave polygon) failed to add '",
                collision_shape->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
            vaPolygonPrimitiveDestroy(p);
            return;
        }

        prim = p;
        kind = VAPrimitiveKind::Polygon;
    }
    else
    {
        return;
    }

    VAPrimitiveRef *ref = memnew(VAPrimitiveRef);
    ref->primitive = prim;
    ref->kind = kind;

    TransformWatcher *watcher = memnew(TransformWatcher);
    watcher->set_on_transform_changed([this, collision_shape, ref]()
    {
        update_collision_shape_primitive(collision_shape, ref);
    });
    collision_shape->add_child(watcher);
    ref->watcher = watcher;

    collision_shape->set_meta(PrimitiveMetaKey(), ref);
}

void VAWorld::create_primitive(Polygon2D *polygon, VAMaterialType material, bool use_flat_transmission)
{
    if (polygon->has_meta(PrimitiveMetaKey()))
    {
        return;
    }

    std::vector<VAVector> points = ConvertPolygonToVAudio(polygon->get_polygon());

    if ((int)points.size() < 3)
    {
        VA_WARN("Polygon2D ", polygon->get_name(), " will not affect raytracing as it has fewer than 3 points");
        return;
    }

    VAVector position;
    float rotation;
    Vector2 scale;
    Decompose(polygon->get_global_transform(), position, rotation, scale);

    VAPolygonPrimitive *prim = nullptr;
    VAResult create_result = vaPolygonPrimitiveCreate(points.data(), (int)points.size(), &prim);
    if (create_result != VA_SUCCESS)
    {
        VA_ERROR(
            "VAWorld::create_primitive(Polygon2D) failed to create the polygon primitive for '",
            polygon->get_name(), "' (VAResult=", VAResultToString(create_result), ")");
        return;
    }

    VAResult material_result = vaPolygonPrimitiveSetMaterial(prim, material);
    if (material_result != VA_SUCCESS)
    {
        VA_ERROR(
            "VAWorld::create_primitive(Polygon2D) failed to set material for '",
            polygon->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
    }

    vaPolygonPrimitiveSetPosition(prim, position);
    vaPolygonPrimitiveSetRotation(prim, rotation);
    vaPolygonPrimitiveSetScale(prim, ToVAudio(scale));
    vaPolygonPrimitiveSetEnclosed(prim, true);
    vaPolygonPrimitiveSetUseFlatTransmission(prim, use_flat_transmission);

    VAResult add_result = vaWorldAddPrimitive_(world, prim);
    if (add_result != VA_SUCCESS)
    {
        VA_ERROR(
            "VAWorld::create_primitive(Polygon2D) failed to add '",
            polygon->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
        vaPolygonPrimitiveDestroy(prim);
        return;
    }

    VAPrimitiveRef *ref = attach_watcher(polygon, prim, VAPrimitiveKind::Polygon, [this, polygon, prim]()
    {
        VAVector updated_position;
        float updated_rotation;
        Vector2 updated_scale;
        Decompose(polygon->get_global_transform(), updated_position, updated_rotation, updated_scale);

        // TransformWatcher only fires on an actual transform change, so VA_UNCHANGED just means this setter's value happened to stay the same - not an error.
        VAResult position_result = vaPolygonPrimitiveSetPosition(prim, updated_position);
        if (position_result != VA_SUCCESS && position_result != VA_UNCHANGED)
        {
            VA_ERROR(
                "VAWorld::create_primitive(Polygon2D) failed to update position for '",
                polygon->get_name(), "' (VAResult=", VAResultToString(position_result), ")");
        }

        VAResult rotation_result = vaPolygonPrimitiveSetRotation(prim, updated_rotation);
        if (rotation_result != VA_SUCCESS && rotation_result != VA_UNCHANGED)
        {
            VA_ERROR(
                "VAWorld::create_primitive(Polygon2D) failed to update rotation for '",
                polygon->get_name(), "' (VAResult=", VAResultToString(rotation_result), ")");
        }

        VAResult scale_result = vaPolygonPrimitiveSetScale(prim, ToVAudio(updated_scale));
        if (scale_result != VA_SUCCESS && scale_result != VA_UNCHANGED)
        {
            VA_ERROR(
                "VAWorld::create_primitive(Polygon2D) failed to update scale for '",
                polygon->get_name(), "' (VAResult=", VAResultToString(scale_result), ")");
        }
    });

    polygon->set_meta(PrimitiveMetaKey(), ref);
}

void VAWorld::create_primitive(Line2D *line, VAMaterialType material)
{
    if (line->has_meta(PrimitiveMetaKey()))
    {
        return;
    }

    std::vector<VAVector> points = ConvertPolygonToVAudio(line->get_points());

    if ((int)points.size() < 3)
    {
        VA_WARN("Line2D ", line->get_name(), " will not affect raytracing as it has fewer than 3 points");
        return;
    }

    VAVector position;
    float rotation;
    Vector2 scale;
    Decompose(line->get_global_transform(), position, rotation, scale);

    VAPolygonPrimitive *prim = nullptr;
    VAResult create_result = vaPolygonPrimitiveCreate(points.data(), (int)points.size(), &prim);
    if (create_result != VA_SUCCESS)
    {
        VA_ERROR(
            "VAWorld::create_primitive(Line2D) failed to create the polygon primitive for '",
            line->get_name(), "' (VAResult=", VAResultToString(create_result), ")");
        return;
    }

    VAResult material_result = vaPolygonPrimitiveSetMaterial(prim, material);
    if (material_result != VA_SUCCESS)
    {
        VA_ERROR(
            "VAWorld::create_primitive(Line2D) failed to set material for '",
            line->get_name(), "' (VAResult=", VAResultToString(material_result), ")");
    }

    vaPolygonPrimitiveSetPosition(prim, position);
    vaPolygonPrimitiveSetRotation(prim, rotation);
    vaPolygonPrimitiveSetScale(prim, ToVAudio(scale));

    // A Line2D is an open polyline, never a closed loop.
    vaPolygonPrimitiveSetEnclosed(prim, false);

    VAResult add_result = vaWorldAddPrimitive_(world, prim);
    if (add_result != VA_SUCCESS)
    {
        VA_ERROR(
            "VAWorld::create_primitive(Line2D) failed to add '",
            line->get_name(), "' to the world (VAResult=", VAResultToString(add_result), ")");
        vaPolygonPrimitiveDestroy(prim);
        return;
    }

    VAPrimitiveRef *ref = attach_watcher(line, prim, VAPrimitiveKind::Polygon, [this, line, prim]()
    {
        VAVector updated_position;
        float updated_rotation;
        Vector2 updated_scale;
        Decompose(line->get_global_transform(), updated_position, updated_rotation, updated_scale);

        // TransformWatcher only fires on an actual transform change, so VA_UNCHANGED just means this setter's value happened to stay the same - not an error.
        VAResult position_result = vaPolygonPrimitiveSetPosition(prim, updated_position);
        if (position_result != VA_SUCCESS && position_result != VA_UNCHANGED)
        {
            VA_ERROR(
                "VAWorld::create_primitive(Line2D) failed to update position for '",
                line->get_name(), "' (VAResult=", VAResultToString(position_result), ")");
        }

        VAResult rotation_result = vaPolygonPrimitiveSetRotation(prim, updated_rotation);
        if (rotation_result != VA_SUCCESS && rotation_result != VA_UNCHANGED)
        {
            VA_ERROR(
                "VAWorld::create_primitive(Line2D) failed to update rotation for '",
                line->get_name(), "' (VAResult=", VAResultToString(rotation_result), ")");
        }

        VAResult scale_result = vaPolygonPrimitiveSetScale(prim, ToVAudio(updated_scale));
        if (scale_result != VA_SUCCESS && scale_result != VA_UNCHANGED)
        {
            VA_ERROR(
                "VAWorld::create_primitive(Line2D) failed to update scale for '",
                line->get_name(), "' (VAResult=", VAResultToString(scale_result), ")");
        }
    });

    line->set_meta(PrimitiveMetaKey(), ref);
}

void VAWorld::update_collision_shape_primitive(CollisionShape2D *collision_shape, VAPrimitiveRef *ref)
{
    Transform2D global_transform = collision_shape->get_global_transform();

    VAVector position;
    float rotation;
    Vector2 scale;
    Decompose(global_transform, position, rotation, scale);

    Ref<Shape2D> shape = collision_shape->get_shape();

    switch (ref->kind)
    {
        case VAPrimitiveKind::Box:
        {
            VABoxPrimitive *p = (VABoxPrimitive *)ref->primitive;
            Ref<RectangleShape2D> rect = shape;

            // TransformWatcher only fires on an actual transform change, so VA_UNCHANGED just means this setter's value happened to stay the same - not an error.
            VAResult position_result = vaBoxPrimitiveSetPosition(p, position);
            if (position_result != VA_SUCCESS && position_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(rectangle) failed to update position for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(position_result), ")");
            }

            VAResult size_result = vaBoxPrimitiveSetSize(p, ToVAudio(rect->get_size() * scale));
            if (size_result != VA_SUCCESS && size_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(rectangle) failed to update size for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(size_result), ")");
            }

            VAResult rotation_result = vaBoxPrimitiveSetRotation(p, rotation);
            if (rotation_result != VA_SUCCESS && rotation_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(rectangle) failed to update rotation for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(rotation_result), ")");
            }
            break;
        }
        case VAPrimitiveKind::Circle:
        {
            VACirclePrimitive *p = (VACirclePrimitive *)ref->primitive;
            Ref<CircleShape2D> circle = shape;

            VAResult center_result = vaCirclePrimitiveSetCenter(p, position);
            if (center_result != VA_SUCCESS && center_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(circle) failed to update center for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(center_result), ")");
            }

            VAResult radius_result = vaCirclePrimitiveSetRadius(p, circle->get_radius() * scale.x);
            if (radius_result != VA_SUCCESS && radius_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(circle) failed to update radius for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(radius_result), ")");
            }
            break;
        }
        case VAPrimitiveKind::Oval:
        {
            VAOvalPrimitive *p = (VAOvalPrimitive *)ref->primitive;
            Ref<CapsuleShape2D> capsule = shape;

            VAResult center_result = vaOvalPrimitiveSetCenter(p, position);
            if (center_result != VA_SUCCESS && center_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(capsule) failed to update center for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(center_result), ")");
            }

            VAResult radius_x_result = vaOvalPrimitiveSetRadiusX(p, capsule->get_radius() * scale.x);
            if (radius_x_result != VA_SUCCESS && radius_x_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(capsule) failed to update radiusX for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(radius_x_result), ")");
            }

            VAResult radius_y_result = vaOvalPrimitiveSetRadiusY(p, (capsule->get_height() / 2.0f) * scale.y);
            if (radius_y_result != VA_SUCCESS && radius_y_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(capsule) failed to update radiusY for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(radius_y_result), ")");
            }

            VAResult rotation_result = vaOvalPrimitiveSetRotation(p, rotation);
            if (rotation_result != VA_SUCCESS && rotation_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(capsule) failed to update rotation for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(rotation_result), ")");
            }
            break;
        }
        case VAPrimitiveKind::Line:
        {
            VALinePrimitive *p = (VALinePrimitive *)ref->primitive;

            if (Ref<SegmentShape2D> segment = shape; segment.is_valid())
            {
                VAResult start_result = vaLinePrimitiveSetStart(p, ToVAudio(global_transform.xform(segment->get_a())));
                if (start_result != VA_SUCCESS && start_result != VA_UNCHANGED)
                {
                    VA_ERROR(
                        "VAWorld::update_collision_shape_primitive(segment) failed to update start for '",
                        collision_shape->get_name(), "' (VAResult=", VAResultToString(start_result), ")");
                }

                VAResult end_result = vaLinePrimitiveSetEnd(p, ToVAudio(global_transform.xform(segment->get_b())));
                if (end_result != VA_SUCCESS && end_result != VA_UNCHANGED)
                {
                    VA_ERROR(
                        "VAWorld::update_collision_shape_primitive(segment) failed to update end for '",
                        collision_shape->get_name(), "' (VAResult=", VAResultToString(end_result), ")");
                }
            }
            else if (Ref<SeparationRayShape2D> ray = shape; ray.is_valid())
            {
                VAResult start_result = vaLinePrimitiveSetStart(p, position);
                if (start_result != VA_SUCCESS && start_result != VA_UNCHANGED)
                {
                    VA_ERROR(
                        "VAWorld::update_collision_shape_primitive(separation ray) failed to update start for '",
                        collision_shape->get_name(), "' (VAResult=", VAResultToString(start_result), ")");
                }

                VAResult end_result = vaLinePrimitiveSetEnd(p, ToVAudio(global_transform.xform(Vector2(0, ray->get_length()))));
                if (end_result != VA_SUCCESS && end_result != VA_UNCHANGED)
                {
                    VA_ERROR(
                        "VAWorld::update_collision_shape_primitive(separation ray) failed to update end for '",
                        collision_shape->get_name(), "' (VAResult=", VAResultToString(end_result), ")");
                }
            }
            else if (Ref<WorldBoundaryShape2D> world_boundary = shape; world_boundary.is_valid())
            {
                Vector2 start, end;
                ComputeWorldBoundaryLine(world_boundary, collision_shape, world, start, end);

                VAResult start_result = vaLinePrimitiveSetStart(p, ToVAudio(start));
                if (start_result != VA_SUCCESS && start_result != VA_UNCHANGED)
                {
                    VA_ERROR(
                        "VAWorld::update_collision_shape_primitive(world boundary) failed to update start for '",
                        collision_shape->get_name(), "' (VAResult=", VAResultToString(start_result), ")");
                }

                VAResult end_result = vaLinePrimitiveSetEnd(p, ToVAudio(end));
                if (end_result != VA_SUCCESS && end_result != VA_UNCHANGED)
                {
                    VA_ERROR(
                        "VAWorld::update_collision_shape_primitive(world boundary) failed to update end for '",
                        collision_shape->get_name(), "' (VAResult=", VAResultToString(end_result), ")");
                }
            }
            break;
        }
        case VAPrimitiveKind::Polygon:
        {
            VAPolygonPrimitive *p = (VAPolygonPrimitive *)ref->primitive;

            VAResult position_result = vaPolygonPrimitiveSetPosition(p, position);
            if (position_result != VA_SUCCESS && position_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(polygon) failed to update position for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(position_result), ")");
            }

            VAResult rotation_result = vaPolygonPrimitiveSetRotation(p, rotation);
            if (rotation_result != VA_SUCCESS && rotation_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(polygon) failed to update rotation for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(rotation_result), ")");
            }

            VAResult scale_result = vaPolygonPrimitiveSetScale(p, ToVAudio(scale));
            if (scale_result != VA_SUCCESS && scale_result != VA_UNCHANGED)
            {
                VA_ERROR(
                    "VAWorld::update_collision_shape_primitive(polygon) failed to update scale for '",
                    collision_shape->get_name(), "' (VAResult=", VAResultToString(scale_result), ")");
            }
            break;
        }
    }
}

void VAWorld::add_primitive(Node *node, VAMaterialType material, bool use_flat_transmission, PropagateMode filter, bool recursive)
{
    // A node's own material meta always wins.
    bool has_own_material = node->has_meta(MaterialMetaKey());

    if (has_own_material)
    {
        material = get_material(node);
    }

    // A propagation filter declared here constrains the cascade into this node's descendants. Defaults to the inherited filter.
    filter = read_propagate_filter(node, filter);

    // Use this specific transmission override rather than the parent's.
    if (node->has_meta(UseFlatTransmissionMetaKey()))
    {
        use_flat_transmission = node->get_meta(UseFlatTransmissionMetaKey());
    }

    // Ignore nodes without materials.
    if (material != VAMaterialAir)
    {
        // An inherited material only applies to this node if it passes the inherited filter. The cascade into children still uses the unfiltered material - a filtered-out visual can still have a collider descendant that should receive the material.
        if (has_own_material || passes_propagate_filter(node, filter))
        {
            if (CollisionShape2D *collision_shape = Object::cast_to<CollisionShape2D>(node))
                create_primitive(collision_shape, material);
            else if (Polygon2D *polygon = Object::cast_to<Polygon2D>(node))
                create_primitive(polygon, material, use_flat_transmission);
            else if (Line2D *line = Object::cast_to<Line2D>(node))
                create_primitive(line, material);
        }
    }

    if (recursive)
    {
        TypedArray<Node> children = node->get_children();
        for (int i = 0; i < children.size(); i++)
        {
            add_primitive(Object::cast_to<Node>(children[i]), material, use_flat_transmission, filter, true);
        }
    }
}

void VAWorld::remove_primitive(Node *node, bool recursive)
{
    if (node->has_meta(PrimitiveMetaKey()))
    {
        Ref<VAPrimitiveRef> ref = node->get_meta(PrimitiveMetaKey());

        if (ref.is_valid())
        {
            if (ref->watcher)
            {
                ref->watcher->queue_free();
            }

            // vaWorldRemovePrimitive_ failing with VA_NOT_ADDED_TO_WORLD means our bookkeeping disagrees with the SDK about this primitive's membership - still proceed to Destroy so we don't leak it, but surface the mismatch.
            switch (ref->kind)
            {
                case VAPrimitiveKind::Box:
                {
                    VAResult remove_result = vaWorldRemovePrimitive_(world, ref->primitive);
                    if (remove_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(box) failed to remove '",
                            node->get_name(), "' from the world (VAResult=", VAResultToString(remove_result), ")");
                    }

                    VAResult destroy_result = vaBoxPrimitiveDestroy((VABoxPrimitive *)ref->primitive);
                    if (destroy_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(box) failed to destroy '",
                            node->get_name(), "' (VAResult=", VAResultToString(destroy_result), ")");
                    }
                    break;
                }
                case VAPrimitiveKind::Circle:
                {
                    VAResult remove_result = vaWorldRemovePrimitive_(world, ref->primitive);
                    if (remove_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(circle) failed to remove '",
                            node->get_name(), "' from the world (VAResult=", VAResultToString(remove_result), ")");
                    }

                    VAResult destroy_result = vaCirclePrimitiveDestroy((VACirclePrimitive *)ref->primitive);
                    if (destroy_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(circle) failed to destroy '",
                            node->get_name(), "' (VAResult=", VAResultToString(destroy_result), ")");
                    }
                    break;
                }
                case VAPrimitiveKind::Oval:
                {
                    VAResult remove_result = vaWorldRemovePrimitive_(world, ref->primitive);
                    if (remove_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(oval) failed to remove '",
                            node->get_name(), "' from the world (VAResult=", VAResultToString(remove_result), ")");
                    }

                    VAResult destroy_result = vaOvalPrimitiveDestroy((VAOvalPrimitive *)ref->primitive);
                    if (destroy_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(oval) failed to destroy '",
                            node->get_name(), "' (VAResult=", VAResultToString(destroy_result), ")");
                    }
                    break;
                }
                case VAPrimitiveKind::Line:
                {
                    VAResult remove_result = vaWorldRemovePrimitive_(world, ref->primitive);
                    if (remove_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(line) failed to remove '",
                            node->get_name(), "' from the world (VAResult=", VAResultToString(remove_result), ")");
                    }

                    VAResult destroy_result = vaLinePrimitiveDestroy((VALinePrimitive *)ref->primitive);
                    if (destroy_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(line) failed to destroy '",
                            node->get_name(), "' (VAResult=", VAResultToString(destroy_result), ")");
                    }
                    break;
                }
                case VAPrimitiveKind::Polygon:
                {
                    VAResult remove_result = vaWorldRemovePrimitive_(world, ref->primitive);
                    if (remove_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(polygon) failed to remove '",
                            node->get_name(), "' from the world (VAResult=", VAResultToString(remove_result), ")");
                    }

                    VAResult destroy_result = vaPolygonPrimitiveDestroy((VAPolygonPrimitive *)ref->primitive);
                    if (destroy_result != VA_SUCCESS)
                    {
                        VA_ERROR(
                            "VAWorld::remove_primitive(polygon) failed to destroy '",
                            node->get_name(), "' (VAResult=", VAResultToString(destroy_result), ")");
                    }
                    break;
                }
            }
        }

        node->remove_meta(PrimitiveMetaKey());
    }

    if (recursive)
    {
        TypedArray<Node> children = node->get_children();
        for (int i = 0; i < children.size(); i++)
        {
            remove_primitive(Object::cast_to<Node>(children[i]), true);
        }
    }
}

} // namespace va_godot
