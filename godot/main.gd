extends Node2D
# Peach Basket — Godot port, step 1: court + 2 players that pendulum-lean and (on their key) jump while
# the arm raises. The whole point of this step: the ARM is a child of the BODY, so it follows the body's
# lean automatically (no shoulder-syncing math). Placeholder shapes for now; real PNGs come next.
#
# Controls (local test, one window):  LEFT player = SPACE,  RIGHT player = ENTER.

const PlayerScript = preload("res://player.gd")

const VIEW := Vector2(1280, 720)
const GROUND_Y := 560.0   # y (pixels from top) where the players' feet rest

func _ready() -> void:
	_add_rect(Vector2(0, 0), VIEW, Color(0.16, 0.17, 0.22), -10)                  # background
	_add_rect(Vector2(0, GROUND_Y), Vector2(VIEW.x, VIEW.y - GROUND_Y), Color(0.28, 0.29, 0.33), -5)  # floor

	# Two players: left (team A, blue, SPACE) and right (team B, red, ENTER).
	_spawn_player(Vector2(380, GROUND_Y), Color(0.30, 0.55, 1.0), KEY_SPACE, 0.0)
	_spawn_player(Vector2(900, GROUND_Y), Color(1.0, 0.40, 0.35), KEY_ENTER, 1.6)

func _spawn_player(pos: Vector2, col: Color, key: int, phase: float) -> void:
	var p := PlayerScript.new()
	add_child(p)
	p.position = pos
	p.setup(col, key, phase, GROUND_Y)

func _add_rect(pos: Vector2, size: Vector2, col: Color, z: int) -> void:
	var poly := Polygon2D.new()
	poly.polygon = PackedVector2Array([Vector2.ZERO, Vector2(size.x, 0), size, Vector2(0, size.y)])
	poly.color = col
	poly.position = pos
	poly.z_index = z
	add_child(poly)
