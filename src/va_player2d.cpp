// AI-created

#include "va_player2d.h"

#include <godot_cpp/core/class_db.hpp>

void VAPlayer2D::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_max_distance"), &VAPlayer2D::get_max_distance);
    ClassDB::bind_method(D_METHOD("set_max_distance", "value"), &VAPlayer2D::set_max_distance);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance", PROPERTY_HINT_RANGE, "0.0,1000.0,0.1,or_greater"), "set_max_distance", "get_max_distance");

    ClassDB::bind_method(D_METHOD("get_reference_distance"), &VAPlayer2D::get_reference_distance);
    ClassDB::bind_method(D_METHOD("set_reference_distance", "value"), &VAPlayer2D::set_reference_distance);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reference_distance", PROPERTY_HINT_RANGE, "0.0,1000.0,0.1,or_greater"), "set_reference_distance", "get_reference_distance");

    ClassDB::bind_method(D_METHOD("get_rolloff_factor"), &VAPlayer2D::get_rolloff_factor);
    ClassDB::bind_method(D_METHOD("set_rolloff_factor", "value"), &VAPlayer2D::set_rolloff_factor);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rolloff_factor", PROPERTY_HINT_RANGE, "0.0,10.0,0.1,or_greater"), "set_rolloff_factor", "get_rolloff_factor");

    // Script-only alias for `reference_distance` - not exposed in the inspector, see va_player2d.h's get_unit_size().
    ClassDB::bind_method(D_METHOD("get_unit_size"), &VAPlayer2D::get_unit_size);
    ClassDB::bind_method(D_METHOD("set_unit_size", "value"), &VAPlayer2D::set_unit_size);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_size", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_unit_size", "get_unit_size");
}

VAPlayer2D::VAPlayer2D()
{
}

VAPlayer2D::~VAPlayer2D()
{
}

void VAPlayer2D::configure_voice(VAVoice &voice)
{
    voice.set_max_distance(max_distance);
    voice.set_reference_distance(reference_distance);
    voice.set_rolloff(rolloff_factor);

    // Backend positions are always 3-component - 2D sits on the z=0 plane, matching VAPlayer2D.cs's ConfigureVoice.
    Vector2 position = get_global_position();
    voice.set_position(Vector3(position.x, position.y, 0.0f));
}

void VAPlayer2D::_process(double delta)
{
    VAPlayer::_process(delta);

    // Keeps every live voice's position in sync each frame; play() only sets it once, so without this a voice's audible position freezes.
    Vector2 position = get_global_position();
    Vector3 voice_position = Vector3(position.x, position.y, 0.0f);

    for (auto &voice : voices)
        voice.get()->set_position(voice_position);
}

void VAPlayer2D::set_max_distance(float value)
{
    max_distance = value;

    for (auto &voice : voices)
        voice.get()->set_max_distance(value);
}

void VAPlayer2D::set_reference_distance(float value)
{
    reference_distance = value;

    for (auto &voice : voices)
        voice.get()->set_reference_distance(value);
}

void VAPlayer2D::set_rolloff_factor(float value)
{
    rolloff_factor = value;

    for (auto &voice : voices)
        voice.get()->set_rolloff(value);
}
