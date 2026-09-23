#pragma once

#include <godot_cpp/classes/node2d.hpp>
#include <functional>

using namespace godot;

// A lightweight child node that fires a callback whenever its parent's global transform changes. Attach to any Node2D, then set on_transform_changed.
class TransformWatcher : public Node2D
{
    GDCLASS(TransformWatcher, Node2D);

private:
    std::function<void()> on_transform_changed;

protected:
    static void _bind_methods();

public:
    void set_on_transform_changed(std::function<void()> callback);

    void _ready() override;
    void _notification(int what);
};
