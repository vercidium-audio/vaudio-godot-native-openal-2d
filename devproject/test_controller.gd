extends Node2D

# Lives on the scene root rather than on VAListener/VASource - a script's _process replaces the native VAEmitter::_process instead of chaining to it, which stops the emitter's position syncing to vaudio.

@onready var listener: Node2D = $Listener
@onready var source: Node2D = $VASource

func _process(_delta: float) -> void:
	if Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
		source.global_position = get_global_mouse_position()
	else:
		listener.global_position = get_global_mouse_position()
