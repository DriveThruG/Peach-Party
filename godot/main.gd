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
const THROW_FLIGHT := 0.8
# Rim: two solid posts at the ring edges; the ball bounces off them and can only score by dropping
# through the GAP between them from above.
const RIM_HALF_W := 42.0
const POST_R := 10.0
const RIM_RESTITUTION := 0.45

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
var last_ball_y := 0.0   # previous frame's ball Y, for top-down rim scoring

# HUD.
var hud: CanvasLayer
var score_label: Label

# Collision: each character is a capsule (segment torso_lo..torso_hi + this radius).
const PLAYER_RADIUS := 30.0
const TILT_GAIN := 0.02       # how hard a collision topples the body (bigger = more)
const BOUNCE := 0.3           # player-player restitution

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

	_build_hud()

func _build_hud() -> void:
	hud = CanvasLayer.new()
	add_child(hud)

	score_label = _make_label(48, Vector2(VIEW.x * 0.5 - 200, 18), Vector2(400, 60))
	hud.add_child(score_label)
	_update_score_label()

func _make_label(font_size: int, pos: Vector2, sz: Vector2) -> Label:
	var l := Label.new()
	l.add_theme_font_size_override("font_size", font_size)
	l.add_theme_color_override("font_color", Color(1, 1, 1))
	l.add_theme_color_override("font_outline_color", Color(0, 0, 0))
	l.add_theme_constant_override("outline_size", 8)
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	l.position = pos
	l.size = sz
	return l

func _process(delta: float) -> void:
	throw_cooldown = maxf(0.0, throw_cooldown - delta)
	steal_cooldown = maxf(0.0, steal_cooldown - delta)

	for p in players:
		p.tick(delta)
	_separate_players()

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
				_set_holder(best)
		if ball_holder == null:
			ball.physics_step(delta)
			_ball_vs_players()
			_rim_interact()
	else:
		ball.position = ball_holder.hand_pos()
		# Steal by an opposing hand (cooldown stops constant ping-pong).
		if steal_cooldown <= 0.0:
			for p in players:
				if p.team != ball_holder.team and p.hand_pos().distance_squared_to(ball.position) < GRAB_RANGE * GRAB_RANGE:
					_set_holder(p)
					break
		# Throw when the holder releases the key.
		if ball_holder.just_released:
			_throw(ball_holder)

	last_ball_y = ball.position.y

func _set_holder(p) -> void:
	ball_holder = p
	ball.vel = Vector2.ZERO
	ball.z_index = 5          # between body (0) and arm (10): only the arm covers the held ball
	steal_cooldown = 0.25

func _rim_interact() -> void:
	_rim_one(rim_right, 1)
	_rim_one(rim_left, 2)

func _rim_one(c: Vector2, team: int) -> void:
	_bounce_post(c + Vector2(-RIM_HALF_W, 0.0))
	_bounce_post(c + Vector2(RIM_HALF_W, 0.0))
	# Score: dropping DOWN through the gap (crossed the rim line this frame, inside the posts).
	if ball.vel.y > 0.0 and last_ball_y <= c.y and ball.position.y > c.y and absf(ball.position.x - c.x) < RIM_HALF_W:
		_score(team)

func _bounce_post(p: Vector2) -> void:
	var d: Vector2 = ball.position - p
	var dist: float = d.length()
	var min_dist: float = ball.radius + POST_R
	if dist < min_dist and dist > 0.01:
		var n: Vector2 = d / dist
		ball.position = p + n * min_dist
		var vn: float = ball.vel.dot(n)
		if vn < 0.0:
			ball.vel -= n * (vn * (1.0 + RIM_RESTITUTION))

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
	ball.z_index = -1         # free ball renders behind the players again
	throw_cooldown = 0.35

func _score(team: int) -> void:
	if team == 1:
		score_a += 1
	else:
		score_b += 1
	_update_score_label()
	_show_goal()
	_reset_ball()

# ---- HUD ----
func _update_score_label() -> void:
	if score_label != null:
		score_label.text = "A  %d : %d  B" % [score_a, score_b]

func _show_goal() -> void:
	var label := Label.new()
	label.text = "GOAL!"
	label.add_theme_font_size_override("font_size", 110)
	label.add_theme_color_override("font_color", Color(1, 0.82, 0.2))
	label.add_theme_color_override("font_outline_color", Color(0, 0, 0))
	label.add_theme_constant_override("outline_size", 16)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.size = Vector2(640, 180)
	label.position = Vector2(VIEW.x * 0.5 - 320, 175)
	label.pivot_offset = Vector2(320, 90)
	hud.add_child(label)
	label.modulate.a = 0.0
	label.scale = Vector2(0.55, 0.55)

	var tw := create_tween()
	tw.set_parallel(true)
	tw.tween_property(label, "modulate:a", 1.0, 0.15)
	tw.tween_property(label, "scale", Vector2.ONE, 0.4).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tw.set_parallel(false)
	tw.tween_interval(0.6)
	tw.set_parallel(true)
	tw.tween_property(label, "modulate:a", 0.0, 0.4)
	tw.tween_property(label, "position:y", 120.0, 0.4)
	tw.set_parallel(false)
	tw.tween_callback(label.queue_free)

# ---- collision (around the TORSO centre, which sways with the lean — not the feet) ----
func _separate_players() -> void:
	var min_d := PLAYER_RADIUS * 2.0
	for i in range(players.size()):
		for j in range(i + 1, players.size()):
			var a = players[i]
			var b = players[j]
			# Closest points between the two body capsules' centre segments.
			var cp: Array = _seg_seg(a.torso_lo_pos(), a.torso_hi_pos(), b.torso_lo_pos(), b.torso_hi_pos())
			var d: Vector2 = cp[1] - cp[0]
			var dist: float = d.length()
			if dist < min_d and dist > 0.01:
				var n: Vector2 = d / dist     # a -> b
				var overlap: float = min_d - dist
				# Gentle position correction (no teleport-slide).
				a.position -= n * overlap * 0.3
				b.position += n * overlap * 0.3
				# Knockback + topple ONLY when approaching (closing > 0) -> no run-away explosion.
				var closing: float = (a.vel - b.vel).dot(n)
				if closing > 0.0:
					var imp: Vector2 = n * (closing * (1.0 + BOUNCE) * 0.5)
					a.bump(-imp, -n.x * closing * TILT_GAIN)
					b.bump(imp, n.x * closing * TILT_GAIN)

func _ball_vs_players() -> void:
	# The free ball bounces off the player capsule. Grabbing still wins (the hand check runs first and
	# reaches further than the body).
	var min_d: float = PLAYER_RADIUS + ball.radius
	for p in players:
		var c: Vector2 = _closest_on_seg(ball.position, p.torso_lo_pos(), p.torso_hi_pos())
		var d: Vector2 = ball.position - c
		var dist: float = d.length()
		if dist < min_d and dist > 0.01:
			var n: Vector2 = d / dist
			ball.position = c + n * min_d
			var vn: float = ball.vel.dot(n)
			if vn < 0.0:
				ball.vel -= n * (vn * 1.4)

# Closest point on segment ab to point p.
func _closest_on_seg(p: Vector2, a: Vector2, b: Vector2) -> Vector2:
	var ab: Vector2 = b - a
	var t: float = clampf((p - a).dot(ab) / maxf(ab.dot(ab), 0.0001), 0.0, 1.0)
	return a + ab * t

# Closest points between segments p1q1 and p2q2 (returns [c1, c2]).
func _seg_seg(p1: Vector2, q1: Vector2, p2: Vector2, q2: Vector2) -> Array:
	var d1: Vector2 = q1 - p1
	var d2: Vector2 = q2 - p2
	var r: Vector2 = p1 - p2
	var a: float = d1.dot(d1)
	var e: float = d2.dot(d2)
	var f: float = d2.dot(r)
	var s: float = 0.0
	var t: float = 0.0
	if a <= 0.0001 and e <= 0.0001:
		return [p1, p2]
	if a <= 0.0001:
		t = clampf(f / e, 0.0, 1.0)
	elif e <= 0.0001:
		s = clampf(-d1.dot(r) / a, 0.0, 1.0)
	else:
		var c: float = d1.dot(r)
		var b: float = d1.dot(d2)
		var denom: float = a * e - b * b
		if denom > 0.0001:
			s = clampf((b * f - c * e) / denom, 0.0, 1.0)
		t = (b * s + f) / e
		if t < 0.0:
			t = 0.0
			s = clampf(-c / a, 0.0, 1.0)
		elif t > 1.0:
			t = 1.0
			s = clampf((b - c) / a, 0.0, 1.0)
	return [p1 + d1 * s, p2 + d2 * t]

func _reset_ball() -> void:
	ball_holder = null
	ball.vel = Vector2.ZERO
	ball.position = Vector2(640, ground_y - 80)
	ball.z_index = -1
	last_ball_y = ball.position.y
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
