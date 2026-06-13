extends Node2D
# The peach ball. Simple custom 2D physics (gravity + floor + walls). When a character's hand is close,
# main.gd makes that character the holder and the ball follows the hand; on release it is thrown.

const GRAVITY := 1500.0

var vel := Vector2.ZERO
var ground_y := 487.0     # the floor line (= where the hoop poles end); set by main
var min_x := 55.0
var max_x := 1225.0
var radius := 21.0

func _ready() -> void:
	var spr := Sprite2D.new()
	spr.texture = load("res://art/Ball.png")
	spr.scale = Vector2(0.22, 0.22)
	add_child(spr)

func physics_step(delta: float) -> void:
	vel.y += GRAVITY * delta
	position += vel * delta

	if position.x < min_x:
		position.x = min_x
		vel.x = absf(vel.x) * 0.5
	elif position.x > max_x:
		position.x = max_x
		vel.x = -absf(vel.x) * 0.5

	var floor_y := ground_y - radius
	if position.y >= floor_y:
		position.y = floor_y
		vel.y = -absf(vel.y) * 0.35      # small bounce
		vel.x *= 0.9
		if absf(vel.y) < 25.0:
			vel.y = 0.0
	elif position.y < radius:
		position.y = radius
		vel.y = absf(vel.y) * 0.4
