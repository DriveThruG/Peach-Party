extends Node2D
# One basket player. Node tree (built in code):
#   self (position = FEET on the floor)
#   └─ Body            (rotates for the pendulum lean; pivots at the feet)
#      └─ Shoulder     (a fixed point near the top of the body)
#         └─ Arm       (rotates for charge; pivots at the shoulder)
#            └─ Hand   (Marker2D at the far end = grab/throw point later)
# Because Arm is a CHILD of Body, the body's lean carries the whole arm along automatically — the exact
# thing that was painful in UE. Charging only sets the arm's LOCAL rotation.

# ---- feel tunables ----
const GRAVITY := 1600.0
const JUMP_VELOCITY := -680.0
const MAX_LEAN := 0.32          # radians of pendulum tilt
const LEAN_SPEED := 2.2
const ARM_RAISE_SPEED := 3.0    # 0..1 per second
const ARM_REST_DEG := 80.0      # arm angle when down
const ARM_RAISED_DEG := -80.0   # arm angle at full charge

# ---- per-player config (set via setup) ----
var team_color := Color.WHITE
var charge_key := KEY_SPACE
var lean_phase := 0.0
var ground_y := 560.0

# ---- state ----
var vel_y := 0.0
var arm_amt := 0.0              # 0 = arms down, 1 = arms up
var sim_time := 0.0
var was_charging := false

# ---- nodes ----
var body: Node2D
var shoulder: Node2D
var arm: Node2D

func setup(col: Color, key: int, phase: float, in_ground_y: float) -> void:
	team_color = col
	charge_key = key
	lean_phase = phase
	ground_y = in_ground_y

func _ready() -> void:
	body = Node2D.new()
	add_child(body)
	# Body placeholder: a 70x150 bar standing on the feet (feet at local y=0, head at y=-150).
	_add_rect(body, Vector2(70, 150), Vector2(0, -75), team_color)
	# Eyes-ish marker so you can see the lean direction.
	_add_rect(body, Vector2(30, 12), Vector2(0, -120), Color(1, 1, 1, 0.8))

	shoulder = Node2D.new()
	shoulder.position = Vector2(0, -120)   # near the top of the body
	body.add_child(shoulder)

	arm = Node2D.new()
	shoulder.add_child(arm)
	# Arm placeholder: a 70x16 bar extending to the RIGHT from the shoulder pivot.
	_add_rect(arm, Vector2(70, 16), Vector2(35, 0), Color(0.96, 0.86, 0.70))

	var hand := Marker2D.new()
	hand.position = Vector2(70, 0)         # far end of the arm
	arm.add_child(hand)

func _process(delta: float) -> void:
	sim_time += delta

	# Pendulum lean — the body tilts back and forth, pivoting at the feet (Body sits at local origin).
	body.rotation = MAX_LEAN * sin(sim_time * LEAN_SPEED + lean_phase)

	# Charge: hold the key to raise the arm; release lowers it. (LOCAL arm rotation only — the body lean
	# is added for free by the parent transform.)
	var charging := Input.is_physical_key_pressed(charge_key)
	arm_amt = clampf(arm_amt + (1.0 if charging else -1.0) * ARM_RAISE_SPEED * delta, 0.0, 1.0)
	arm.rotation = deg_to_rad(lerpf(ARM_REST_DEG, ARM_RAISED_DEG, arm_amt))

	# Jump on the rising edge of the key while grounded.
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

func _add_rect(parent: Node, size: Vector2, offset: Vector2, col: Color) -> void:
	var poly := Polygon2D.new()
	poly.polygon = PackedVector2Array([
		Vector2(-size.x * 0.5, -size.y * 0.5), Vector2(size.x * 0.5, -size.y * 0.5),
		Vector2(size.x * 0.5, size.y * 0.5), Vector2(-size.x * 0.5, size.y * 0.5)
	])
	poly.color = col
	poly.position = offset
	parent.add_child(poly)
