#include "al_source2d.h"

#include <godot_cpp/core/class_db.hpp>

#include "openal/al_source_handle.h"

void ALSource2D::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_max_distance"), &ALSource2D::get_max_distance);
    ClassDB::bind_method(D_METHOD("set_max_distance", "value"), &ALSource2D::set_max_distance);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance", PROPERTY_HINT_RANGE, "0.0,1000.0,0.1,or_greater"), "set_max_distance", "get_max_distance");

    ClassDB::bind_method(D_METHOD("get_reference_distance"), &ALSource2D::get_reference_distance);
    ClassDB::bind_method(D_METHOD("set_reference_distance", "value"), &ALSource2D::set_reference_distance);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reference_distance", PROPERTY_HINT_RANGE, "0.0,1000.0,0.1,or_greater"), "set_reference_distance", "get_reference_distance");

    ClassDB::bind_method(D_METHOD("get_rolloff_factor"), &ALSource2D::get_rolloff_factor);
    ClassDB::bind_method(D_METHOD("set_rolloff_factor", "value"), &ALSource2D::set_rolloff_factor);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rolloff_factor", PROPERTY_HINT_RANGE, "0.0,10.0,0.1,or_greater"), "set_rolloff_factor", "get_rolloff_factor");

    // Script-only alias for `reference_distance` - not exposed in the inspector, see al_source2d.h's get_unit_size().
    ClassDB::bind_method(D_METHOD("get_unit_size"), &ALSource2D::get_unit_size);
    ClassDB::bind_method(D_METHOD("set_unit_size", "value"), &ALSource2D::set_unit_size);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_size", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_unit_size", "get_unit_size");
}

ALSource2D::ALSource2D()
{
}

ALSource2D::~ALSource2D()
{
}

void ALSource2D::configure_source(ALSourceHandle &source)
{
    source.set_max_distance(max_distance);
    source.set_reference_distance(reference_distance);
    source.set_rolloff_factor(rolloff_factor);

    // OpenAL positions are always 3-component - 2D sits on the z=0 plane, matching ALSource2D.cs's ConfigureSource.
    Vector2 position = get_global_position();
    source.set_position(Vector3(position.x, position.y, 0.0f));
}

void ALSource2D::_process(double delta)
{
    ALSource::_process(delta);

    // Keeps every live source's OpenAL position in sync each frame; play() only sets it once, so without this a source's audible position freezes.
    Vector2 position = get_global_position();
    Vector3 al_position = Vector3(position.x, position.y, 0.0f);

    for (auto &source : get_sources())
    {
        source->set_position(al_position);
    }
}

void ALSource2D::set_max_distance(float value)
{
    max_distance = value;

    for (auto &source : get_sources())
    {
        source->set_max_distance(value);
    }
}

void ALSource2D::set_reference_distance(float value)
{
    reference_distance = value;

    for (auto &source : get_sources())
    {
        source->set_reference_distance(value);
    }
}

void ALSource2D::set_rolloff_factor(float value)
{
    rolloff_factor = value;

    for (auto &source : get_sources())
    {
        source->set_rolloff_factor(value);
    }
}
