extends Node2D
# One basket character. Tree:
#   self (FEET; scale.x flipped for team B so it faces left)
#   └─ Body  (rotates for pendulum lean; pivots at the feet)
#      └─ Shoulder (fixed point on the body)
#         └─ Arm (hangs down; rotates LOCALLY for charge)  ← child of Body = follows the lean for free
#            └─ Hand (Marker2D, far end = grab/throw point, next step)
#
# Movement = the Original mechanic: pressing the key JUMPS ALONG THE CURRENT LEAN, so timing the jump at
# a pendulum extreme moves you sideways. Two chars per team share one key, so a press scatters both.

# ---- feel tunables ----
const GRAVITY := 1600.0
const JUMP_SPEED := 720.0         # jump impulse magnitude (along the lean)
const SIDE_FACTOR := 1.15         # how strongly the lean turns into sideways travel
const AIR_DRAG := 0.5
const GROUND_FRICTION := 7.0
const MAX_LEAN := 0.26            # radians of pendulum tilt
const LEAN_SPEED := 2.2
const ARM_RAISE_SPEED := 3.0
const ARM_REST_DEG := 0.0         # arm hangs straight down (texture's natural pose)
const ARM_RAISED_DEG := -150.0    # swing up & forward at full charge (flip sign if it swings backward)
const PLAYER_SCALE := 0.44

# Body-local anchors (feet at body origin; body image extends UP to y=-380):
const BODY_HALF_H := 190.0
const SHOULDER := Vector2(2, -262)
const ARM_HALF_H := 72.0

# ---- config ----
var charge_key := KEY_SPACE
var lean_phase := 0.0
var ground_y := 585.0
var facing := 1
var min_x := 70.0
var max_x := 1210.0

# ---- state ----
var vel := Vector2.ZERO
var arm_amt := 0.0
var sim_time := 0.0
var was_charging := false

var body: Node2D
var arm: Sprite2D

func setup(body_path: String, arm_path: String, key: int, phase: float, in_ground_y: float, in_facing: int) -> void:
	charge_key = key
	lean_phase = phase
	ground_y = in_ground_y
	facing = in_facing
	scale = Vector2(PLAYER_SCALE * facing, PLAYER_SCALE)

	body = Node2D.new()
	add_child(body)

	var body_spr := Sprite2D.new()
	body_spr.texture = load(body_path)
	body_spr.centered = true
	body_spr.offset = Vector2(0, -BODY_HALF_H)
	body.add_child(body_spr)

	var shoulder := Node2D.new()
	shoulder.position = SHOULDER
	body.add_child(shoulder)

	arm = Sprite2D.new()
	arm.texture = load(arm_path)
	arm.centered = true
	arm.offset = Vector2(0, ARM_HALF_H)
	arm.z_index = 1
	shoulder.add_child(arm)

	var hand := Marker2D.new()
	hand.position = Vector2(0, ARM_HALF_H * 2.0)
	arm.add_child(hand)

func _process(delta: float) -> void:
	if body == null:
		return
	sim_time += delta

	body.rotation = MAX_LEAN * sin(sim_time * LEAN_SPEED + lean_phase)

	var charging := Input.is_physical_key_pressed(charge_key)
	arm_amt = clampf(arm_amt + (1.0 if charging else -1.0) * ARM_RAISE_SPEED * delta, 0.0, 1.0)
	arm.rotation = deg_to_rad(lerpf(ARM_REST_DEG, ARM_RAISED_DEG, arm_amt))

	# Jump on the rising edge while grounded — along the body's CURRENT (visual) up direction.
	if charging and not was_charging and is_grounded():
		var up := body.global_transform.basis_xform(Vector2(0, -1)).normalized()
		vel = Vector2(up.x * SIDE_FACTOR, up.y) * JUMP_SPEED
	was_charging = charging

	# Integrate.
	vel.y += GRAVITY * delta
	vel.x *= maxf(0.0, 1.0 - AIR_DRAG * delta)
	position += vel * delta

	# Floor.
	if position.y >= ground_y:
		position.y = ground_y
		if vel.y > 0.0:
			vel.y = 0.0
		vel.x *= maxf(0.0, 1.0 - GROUND_FRICTION * delta)

	# Side walls.
	if position.x < min_x:
		position.x = min_x
		vel.x = absf(vel.x) * 0.3
	elif position.x > max_x:
		position.x = max_x
		vel.x = -absf(vel.x) * 0.3

func is_grounded() -> bool:
	return position.y >= ground_y - 1.0
