extends Node2D
# Peach Basket — Godot port, step 3: 4 characters (2 per team, each team's two share ONE key, like the
# original), pendulum-jump horizontal movement, bigger hoops, smaller ball/players.
# Controls (local test):  LEFT team (A) = SPACE,  RIGHT team (B) = ENTER.  Each key jumps BOTH of its chars.

const PlayerScript = preload("res://player.gd")

const VIEW := Vector2(1280, 720)
const GROUND_Y := 590.0

func _ready() -> void:
	var bg := Sprite2D.new()
	bg.texture = load("res://art/Background.png")
	bg.centered = true
	bg.position = VIEW * 0.5
	var ts := bg.texture.get_size()
	bg.scale = Vector2(VIEW.x / ts.x, VIEW.y / ts.y)
	bg.z_index = -10
	add_child(bg)

	# Hoops (bigger now). Hoop.png = pole-left / rim-right; right hoop flipped.
	_add_hoop(Vector2(205, 315), false)
	_add_hoop(Vector2(1075, 315), true)

	# Ball — smaller, static for now.
	var ball := Sprite2D.new()
	ball.texture = load("res://art/Ball.png")
	ball.position = Vector2(640, 500)
	ball.scale = Vector2(0.22, 0.22)
	add_child(ball)

	# Team A (left, faces right, SPACE) — two chars sharing the key, different lean phases.
	_spawn(Vector2(330, GROUND_Y), "res://art/Player01_Body.png", "res://art/PLayer01_Arm.png", KEY_SPACE, 0.0, 1)
	_spawn(Vector2(500, GROUND_Y), "res://art/Player03_Body.png", "res://art/Player03_Arm.png", KEY_SPACE, 1.1, 1)
	# Team B (right, faces left, ENTER).
	_spawn(Vector2(780, GROUND_Y), "res://art/Player02_Body.png", "res://art/Player02_Arm.png", KEY_ENTER, 0.6, -1)
	_spawn(Vector2(950, GROUND_Y), "res://art/Player04_Body.png", "res://art/Player04_Arm.png", KEY_ENTER, 1.7, -1)

func _spawn(pos: Vector2, body_path: String, arm_path: String, key: int, phase: float, facing: int) -> void:
	var p := PlayerScript.new()
	add_child(p)
	p.position = pos
	p.setup(body_path, arm_path, key, phase, GROUND_Y, facing)

func _add_hoop(pos: Vector2, flip: bool) -> void:
	var h := Sprite2D.new()
	h.texture = load("res://art/Hoop.png")
	h.position = pos
	h.scale = Vector2(0.7, 0.7)
	h.flip_h = flip
	h.z_index = -2
	add_child(h)
