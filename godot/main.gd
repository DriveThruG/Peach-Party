extends Node2D
# Peach Basket — Godot port, step 2: real art. Background + 2 players (real body + arm sprites, the arm
# PARENTED to the body so it follows the lean automatically) + 2 hoops + the ball (static for now).
# Controls (local test, one window):  LEFT player = SPACE,  RIGHT player = ENTER.
# Sizes/positions are plain constants here — easy to nudge (no editor needed); tell me what feels off.

const PlayerScript = preload("res://player.gd")

const VIEW := Vector2(1280, 720)
const GROUND_Y := 585.0     # y where the players' feet rest on the court

func _ready() -> void:
	# Background, stretched to fill the viewport.
	var bg := Sprite2D.new()
	bg.texture = load("res://art/Background.png")
	bg.centered = true
	bg.position = VIEW * 0.5
	var ts := bg.texture.get_size()
	bg.scale = Vector2(VIEW.x / ts.x, VIEW.y / ts.y)
	bg.z_index = -10
	add_child(bg)

	# Hoops (Hoop.png = pole-left / rim-right). Left hoop as-is; right hoop flipped to mirror.
	_add_hoop(Vector2(170, 360), false)
	_add_hoop(Vector2(1110, 360), true)

	# Ball — static placeholder position for now (physics + grab/throw next step).
	var ball := Sprite2D.new()
	ball.texture = load("res://art/Ball.png")
	ball.position = Vector2(640, 470)
	ball.scale = Vector2(0.32, 0.32)
	add_child(ball)

	# Players: team A (left, faces right, SPACE), team B (right, faces left, ENTER).
	_spawn_player(Vector2(440, GROUND_Y), "res://art/Player01_Body.png", "res://art/PLayer01_Arm.png", KEY_SPACE, 0.0, 1)
	_spawn_player(Vector2(840, GROUND_Y), "res://art/Player02_Body.png", "res://art/Player02_Arm.png", KEY_ENTER, 1.6, -1)

func _spawn_player(pos: Vector2, body_path: String, arm_path: String, key: int, phase: float, facing: int) -> void:
	var p := PlayerScript.new()
	add_child(p)
	p.position = pos
	p.setup(body_path, arm_path, key, phase, GROUND_Y, facing)

func _add_hoop(pos: Vector2, flip: bool) -> void:
	var h := Sprite2D.new()
	h.texture = load("res://art/Hoop.png")
	h.position = pos
	h.scale = Vector2(0.45, 0.45)
	h.flip_h = flip
	h.z_index = -2
	add_child(h)
