extends Node2D
# One basket character. main.gd drives it via tick(delta) (deterministic order, so grab/throw read fresh
# state). Tree: self(feet) > Body(lean) > Shoulder > Arm(charge rotates it) > Hand(grab/throw point).
# The Arm is a CHILD of Body, so it follows the lean for free.

# ---- feel tunables ----
const GRAVITY := 1600.0
const JUMP_SPEED := 720.0
const SIDE_FACTOR := 1.15
const AIR_DRAG := 0.5
const GROUND_FRICTION := 7.0
const MAX_LEAN := 0.26
const LEAN_SPEED := 2.2
# Wobble: a bump tips the body (tilt); a spring rights it again. Underdamped = a little wobble back.
const TILT_STIFFNESS := 90.0
const TILT_DAMPING := 8.0
const MAX_TILT := 1.5
const ARM_RAISE_SPEED := 3.0
const ARM_REST_DEG := 0.0
const ARM_RAISED_DEG := -150.0
const PLAYER_SCALE := 0.44

# Body-local anchors (feet at body origin; body extends up to y=-380):
const BODY_HALF_H := 190.0
const SHOULDER := Vector2(-34, -241)   # tuned: sits exactly on the shoulder
const ARM_HALF_H := 72.0               # half the arm image height (for the hand position)
const ARM_PIVOT_Y := 59.0              # tuned: where the arm rotates (stays pinned at the shoulder)

# ---- config ----
var charge_key := KEY_SPACE
var lean_phase := 0.0
var ground_y := 487.0
var facing := 1
var team := 1                # 1 = A (left/faces right), 2 = B (right/faces left)
var min_x := 70.0
var max_x := 1210.0

# ---- state (read by main) ----
var vel := Vector2.ZERO
var arm_amt := 0.0
var just_released := false   # true only on the frame the key is released
var sim_time := 0.0
var was_charging := false
var tilt := 0.0              # extra body rotation from bumps (springs back to 0)
var tilt_vel := 0.0

var body: Node2D
var shoulder_node: Node2D
var arm: Sprite2D
var hand: Marker2D
var torso: Marker2D

func setup(body_path: String, arm_path: String, key: int, phase: float, in_ground_y: float, in_facing: int) -> void:
	charge_key = key
	lean_phase = phase
	ground_y = in_ground_y
	facing = in_facing
	team = 1 if in_facing == 1 else 2
	scale = Vector2(PLAYER_SCALE * facing, PLAYER_SCALE)

	body = Node2D.new()
	add_child(body)

	var body_spr := Sprite2D.new()
	body_spr.texture = load(body_path)
	body_spr.centered = true
	body_spr.offset = Vector2(0, -BODY_HALF_H)
	body.add_child(body_spr)

	# Torso centre — a child of Body, so it sways with the lean. Used as the collision centre.
	torso = Marker2D.new()
	torso.position = Vector2(0, -BODY_HALF_H)
	body.add_child(torso)

	shoulder_node = Node2D.new()
	shoulder_node.position = SHOULDER
	body.add_child(shoulder_node)

	arm = Sprite2D.new()
	arm.texture = load(arm_path)
	arm.centered = true
	arm.offset = Vector2(0, ARM_PIVOT_Y)
	arm.z_index = 10        # arm in FRONT of the body AND in front of a held ball (ball sits at z=5)
	shoulder_node.add_child(arm)

	hand = Marker2D.new()
	hand.position = Vector2(0, ARM_PIVOT_Y + ARM_HALF_H)
	arm.add_child(hand)

func hand_pos() -> Vector2:
	return hand.global_position

func torso_pos() -> Vector2:
	return torso.global_position

# Live-tuning: reposition the shoulder pivot + arm-image pivot (so the arm stays pinned at the shoulder
# when raised). Called by main.gd's tuner.
func update_rig(sh: Vector2, arm_pivot_y: float) -> void:
	if shoulder_node != null:
		shoulder_node.position = sh
	if arm != null:
		arm.offset = Vector2(0, arm_pivot_y)
	if hand != null:
		hand.position = Vector2(0, arm_pivot_y + ARM_HALF_H)

func tick(delta: float) -> void:
	if body == null:
		return
	sim_time += delta

	# Pendulum lean + a spring-damped tilt (auto-rights the body after a bump).
	var lean := MAX_LEAN * sin(sim_time * LEAN_SPEED + lean_phase)
	tilt_vel += (-TILT_STIFFNESS * tilt - TILT_DAMPING * tilt_vel) * delta
	tilt = clampf(tilt + tilt_vel * delta, -MAX_TILT, MAX_TILT)
	body.rotation = lean + tilt

	var charging := Input.is_physical_key_pressed(charge_key)
	just_released = was_charging and not charging
	arm_amt = clampf(arm_amt + (1.0 if charging else -1.0) * ARM_RAISE_SPEED * delta, 0.0, 1.0)
	arm.rotation = deg_to_rad(lerpf(ARM_REST_DEG, ARM_RAISED_DEG, arm_amt))

	if charging and not was_charging and is_grounded():
		var up := body.global_transform.basis_xform(Vector2(0, -1)).normalized()
		vel = Vector2(up.x * SIDE_FACTOR, up.y) * JUMP_SPEED
	was_charging = charging

	vel.y += GRAVITY * delta
	vel.x *= maxf(0.0, 1.0 - AIR_DRAG * delta)
	position += vel * delta

	if position.y >= ground_y:
		position.y = ground_y
		if vel.y > 0.0:
			vel.y = 0.0
		vel.x *= maxf(0.0, 1.0 - GROUND_FRICTION * delta)

	if position.x < min_x:
		position.x = min_x
		vel.x = absf(vel.x) * 0.3
	elif position.x > max_x:
		position.x = max_x
		vel.x = -absf(vel.x) * 0.3

func is_grounded() -> bool:
	return position.y >= ground_y - 1.0

# Knockback + topple from a collision (main.gd calls this).
func bump(impulse: Vector2, tilt_impulse: float) -> void:
	vel += impulse
	tilt_vel += tilt_impulse
