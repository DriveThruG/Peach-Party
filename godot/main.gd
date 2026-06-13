extends Node2D
# Peach Basket — Godot port, step 4: floor aligned to the hoop bases; ball can be GRABBED (hand near ->
# hold), STOLEN (enemy hand near, with cooldown), THROWN on key release (auto-aim at the enemy rim,
# scaled by arm height) and SCORED (drops near a rim). Score is printed for now (HUD later).
# Controls: LEFT team (A) = SPACE, RIGHT team (B) = ENTER. Each key drives BOTH of that team's chars.

const PlayerScript = preload("res://player.gd")
const BallScript = preload("res://ball.gd")

const VIEW := Vector2(1280, 720)
const HOOP_SCALE := 0.7
const GRAB_RANGE := 78.0
const SCORE_RANGE := 48.0
const THROW_FLIGHT := 0.8

var players: Array = []
var ball
var ground_y := 487.0
var rim_left := Vector2.ZERO
var rim_right := Vector2.ZERO
var score_a := 0
var score_b := 0
var throw_cooldown := 0.0
var steal_cooldown := 0.0
var ball_holder = null   # which player currently holds the ball (null = free)

func _ready() -> void:
	var bg := Sprite2D.new()
	bg.texture = load("res://art/Background.png")
	bg.centered = true
	bg.position = VIEW * 0.5
	var ts := bg.texture.get_size()
	bg.scale = Vector2(VIEW.x / ts.x, VIEW.y / ts.y)
	bg.z_index = -10
	add_child(bg)

	# Hoops. Floor = where the hoop pole base ends (bottom of the hoop sprite).
	var hoop_tex: Texture2D = load("res://art/Hoop.png")
	var hoop_h := hoop_tex.get_size().y * HOOP_SCALE
	var left_pos := Vector2(205, 315)
	var right_pos := Vector2(1075, 315)
	_add_hoop(left_pos, false, hoop_tex)
	_add_hoop(right_pos, true, hoop_tex)
	ground_y = left_pos.y + hoop_h * 0.5
	# Rim (orange ring) sits at ~(300,155) in the 385x493 image; mirror x for the flipped right hoop.
	rim_left = left_pos + Vector2(75, -64)
	rim_right = right_pos + Vector2(-75, -64)

	ball = BallScript.new()
	ball.ground_y = ground_y
	add_child(ball)
	_reset_ball()

	_spawn(Vector2(330, ground_y), "res://art/Player01_Body.png", "res://art/PLayer01_Arm.png", KEY_SPACE, 0.0, 1)
	_spawn(Vector2(500, ground_y), "res://art/Player03_Body.png", "res://art/Player03_Arm.png", KEY_SPACE, 1.1, 1)
	_spawn(Vector2(780, ground_y), "res://art/Player02_Body.png", "res://art/Player02_Arm.png", KEY_ENTER, 0.6, -1)
	_spawn(Vector2(950, ground_y), "res://art/Player04_Body.png", "res://art/Player04_Arm.png", KEY_ENTER, 1.7, -1)

func _process(delta: float) -> void:
	throw_cooldown = maxf(0.0, throw_cooldown - delta)
	steal_cooldown = maxf(0.0, steal_cooldown - delta)

	for p in players:
		p.tick(delta)

	if ball_holder == null:
		# Grab: nearest hand within range (after the throw cooldown).
		if throw_cooldown <= 0.0:
			var best = null
			var best_d := GRAB_RANGE * GRAB_RANGE
			for p in players:
				var d: float = p.hand_pos().distance_squared_to(ball.position)
				if d < best_d:
					best_d = d
					best = p
			if best != null:
				ball_holder = best
				ball.vel = Vector2.ZERO
				steal_cooldown = 0.25
		if ball_holder == null:
			ball.physics_step(delta)
			_check_score()
	else:
		ball.position = ball_holder.hand_pos()
		# Steal by an opposing hand (cooldown stops constant ping-pong).
		if steal_cooldown <= 0.0:
			for p in players:
				if p.team != ball_holder.team and p.hand_pos().distance_squared_to(ball.position) < GRAB_RANGE * GRAB_RANGE:
					ball_holder = p
					steal_cooldown = 0.25
					break
		# Throw when the holder releases the key.
		if ball_holder.just_released:
			_throw(ball_holder)

func _throw(holder) -> void:
	var target: Vector2 = rim_right if holder.team == 1 else rim_left
	var s: Vector2 = ball.position
	var tf := THROW_FLIGHT
	var g := BallScript.GRAVITY
	var vx := (target.x - s.x) / tf
	var vy := (target.y - s.y - 0.5 * g * tf * tf) / tf
	var quality := clampf(holder.arm_amt, 0.18, 1.0)   # low arms -> weak throw -> falls short
	ball.vel = Vector2(vx, vy) * quality + Vector2(randf_range(-18.0, 18.0), randf_range(-18.0, 18.0))
	ball_holder = null
	throw_cooldown = 0.35

func _check_score() -> void:
	if ball.vel.y <= 0.0:
		return  # only when falling
	if ball.position.distance_to(rim_right) < SCORE_RANGE:
		_score(1)
	elif ball.position.distance_to(rim_left) < SCORE_RANGE:
		_score(2)

func _score(team: int) -> void:
	if team == 1:
		score_a += 1
	else:
		score_b += 1
	print("SCORE  A %d : %d B" % [score_a, score_b])
	_reset_ball()

func _reset_ball() -> void:
	ball_holder = null
	ball.vel = Vector2.ZERO
	ball.position = Vector2(640, ground_y - 80)
	throw_cooldown = 0.3

func _spawn(pos: Vector2, body_path: String, arm_path: String, key: int, phase: float, facing: int) -> void:
	var p := PlayerScript.new()
	add_child(p)
	p.position = pos
	p.setup(body_path, arm_path, key, phase, ground_y, facing)
	players.append(p)

func _add_hoop(pos: Vector2, flip: bool, tex: Texture2D) -> void:
	var h := Sprite2D.new()
	h.texture = tex
	h.position = pos
	h.scale = Vector2(HOOP_SCALE, HOOP_SCALE)
	h.flip_h = flip
	h.z_index = -2
	add_child(h)
