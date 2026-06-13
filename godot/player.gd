extends Node2D
# One basket player, built from your real sprites. Node tree:
#   self (FEET on the floor; scale flips x for team B so it faces left)
#   └─ Body  (Sprite2D, rotates for the pendulum lean; pivots at the feet)
#      └─ Shoulder (Node2D, a fixed point on the body)
#         └─ Arm (Sprite2D, hangs down; rotates LOCALLY for charge)
#            └─ Hand (Marker2D, far end = grab/throw point, used next step)
# Arm is a CHILD of Body, so the lean carries it along for free. Charge only sets the arm's local angle.

# ---- feel tunables ----
const GRAVITY := 1600.0
const JUMP_VELOCITY := -680.0
const MAX_LEAN := 0.26           # radians of pendulum tilt
const LEAN_SPEED := 2.2
const ARM_RAISE_SPEED := 3.0     # 0..1 per second
const ARM_REST_DEG := 0.0        # arm hangs straight down (the texture's natural pose)
const ARM_RAISED_DEG := -150.0   # swing up & forward at full charge (flip sign if it swings the wrong way)
const PLAYER_SCALE := 0.5

# Body-local anchors (feet at body origin (0,0); the body image extends UP to y=-380):
const BODY_HALF_H := 190.0       # half of the 380px body height -> feet offset
const SHOULDER := Vector2(8, -250)
const ARM_HALF_H := 72.0         # half of the 145px arm height -> shoulder-at-top offset

# ---- per-player config ----
var charge_key := KEY_SPACE
var lean_phase := 0.0
var ground_y := 580.0
var facing := 1                  # +1 = faces right, -1 = faces left (mirrored)

# ---- state ----
var vel_y := 0.0
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
	scale = Vector2(PLAYER_SCALE * facing, PLAYER_SCALE)   # x-flip mirrors team B

	body = Node2D.new()
	add_child(body)

	var body_spr := Sprite2D.new()
	body_spr.texture = load(body_path)
	body_spr.centered = true
	body_spr.offset = Vector2(0, -BODY_HALF_H)             # feet (bottom-centre) at body origin
	body.add_child(body_spr)

	var shoulder := Node2D.new()
	shoulder.position = SHOULDER
	body.add_child(shoulder)

	arm = Sprite2D.new()
	arm.texture = load(arm_path)
	arm.centered = true
	arm.offset = Vector2(0, ARM_HALF_H)                    # shoulder (top-centre) at arm origin = pivot
	arm.z_index = 1                                        # arm drawn in front of body
	shoulder.add_child(arm)

	var hand := Marker2D.new()
	hand.position = Vector2(0, ARM_HALF_H * 2.0)           # far end of the arm
	arm.add_child(hand)

func _process(delta: float) -> void:
	sim_time += delta

	# Pendulum lean — body tilts back and forth, pivoting at the feet.
	body.rotation = MAX_LEAN * sin(sim_time * LEAN_SPEED + lean_phase)

	# Charge raises the arm (local rotation only; the body lean is added for free by the parent).
	var charging := Input.is_physical_key_pressed(charge_key)
	arm_amt = clampf(arm_amt + (1.0 if charging else -1.0) * ARM_RAISE_SPEED * delta, 0.0, 1.0)
	arm.rotation = deg_to_rad(lerpf(ARM_REST_DEG, ARM_RAISED_DEG, arm_amt))

	# Jump on the rising edge while grounded.
	if charging and not was_charging and is_grounded():
		vel_y = JUMP_VELOCITY
	was_charging = charging

	# Gravity + floor.
	vel_y += GRAVITY * delta
	position.y += vel_y * delta
	if position.y >= ground_y:
		position.y = ground_y
		vel_y = 0.0

func is_grounded() -> bool:
	return position.y >= ground_y - 1.0
