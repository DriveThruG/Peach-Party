extends Node2D
# Peach Artillery — turn-based tank duel. Hotseat for now (the ACTIVE player uses the keys; turns switch
# after each shot). Multiplayer turn-sync comes later. Placeholder shapes; sprite-sheet visuals next.
#
# Controls (active player only):  A/D move (costs fuel)   W/S aim   R/F power   Q weapon   SPACE fire

const VIEW := Vector2(1280, 720)
const GROUND_Y := 600.0
const GRAVITY := 480.0
const MOVE_SPEED := 70.0
const AIM_RATE := 42.0
const POWER_RATE := 280.0
const GUIDE_STEPS := 48          # trajectory-preview length (~0.8s of flight)
const STICK_DEADZONE := 0.35

# Per-weapon stats: [Shell, Heavy].
const WEAPON_NAMES := ["Shell", "Heavy"]
const EXPL_RADIUS := [120.0, 175.0]
const DIRECT_DMG := [34.0, 52.0]      # at the centre; falls off linearly to 0 at the edge
const KNOCKBACK := [140.0, 220.0]
const PROJ_COLOR := [Color(0.95, 0.85, 0.2), Color(1.0, 0.5, 0.15)]

var tanks: Array[Tank] = []
var active := 0
var state := "aim"                    # aim | flying | settle | over
var settle := 0.0

var proj: Node2D = null
var proj_vel := Vector2.ZERO

var hud: CanvasLayer
var info: Label
var guide: Line2D

func _ready() -> void:
	_add_rect(Vector2.ZERO, Vector2(VIEW.x, GROUND_Y), Color(0.52, 0.62, 0.80), -10)        # sky
	_add_rect(Vector2(0, GROUND_Y), Vector2(VIEW.x, VIEW.y - GROUND_Y), Color(0.36, 0.27, 0.17), -5)  # ground

	_spawn_tank(220.0, Color(0.30, 0.55, 1.0), 1)
	_spawn_tank(1060.0, Color(1.0, 0.40, 0.35), -1)

	# Trajectory-preview line (world space).
	guide = Line2D.new()
	guide.width = 4.0
	guide.default_color = Color(1, 1, 1, 0.5)
	guide.z_index = 8
	add_child(guide)

	hud = CanvasLayer.new()
	add_child(hud)
	info = Label.new()
	info.add_theme_font_size_override("font_size", 22)
	info.add_theme_color_override("font_color", Color.WHITE)
	info.add_theme_color_override("font_outline_color", Color.BLACK)
	info.add_theme_constant_override("outline_size", 6)
	info.position = Vector2(20, 16)
	info.size = Vector2(VIEW.x - 40, 80)
	hud.add_child(info)
	_update_info()

func _spawn_tank(x: float, col: Color, facing: int) -> void:
	var t := Tank.new()
	add_child(t)
	t.position = Vector2(x, GROUND_Y)
	t.setup(col, facing)
	tanks.append(t)

func _process(delta: float) -> void:
	guide.visible = (state == "aim")
	match state:
		"aim":
			_aim_phase(delta)
		"flying":
			_fly_phase(delta)
		"settle":
			settle -= delta
			if settle <= 0.0:
				active = (active + 1) % tanks.size()
				state = "aim"
				_update_info()

func _aim_phase(delta: float) -> void:
	var t: Tank = tanks[active]

	# Move (A/D), costs fuel.
	var mv := 0.0
	if Input.is_physical_key_pressed(KEY_A):
		mv -= 1.0
	if Input.is_physical_key_pressed(KEY_D):
		mv += 1.0
	if mv != 0.0 and t.fuel > 0.0:
		var dx := mv * MOVE_SPEED * delta
		t.position.x = clampf(t.position.x + dx, 70.0, VIEW.x - 70.0)
		t.spend_fuel(absf(dx))

	# Aim: keys (W/S) OR the left analog stick (push it where you want to aim).
	if Input.is_physical_key_pressed(KEY_W):
		t.aim(AIM_RATE * delta)
	if Input.is_physical_key_pressed(KEY_S):
		t.aim(-AIM_RATE * delta)
	var stick := Vector2(Input.get_joy_axis(0, JOY_AXIS_LEFT_X), Input.get_joy_axis(0, JOY_AXIS_LEFT_Y))
	if stick.length() > STICK_DEADZONE:
		t.set_aim_deg(rad_to_deg(atan2(-stick.y, absf(stick.x) + 0.001)))

	# Power: R/F OR the UP/DOWN arrow keys.
	if Input.is_physical_key_pressed(KEY_R) or Input.is_physical_key_pressed(KEY_UP):
		t.change_power(POWER_RATE * delta)
	if Input.is_physical_key_pressed(KEY_F) or Input.is_physical_key_pressed(KEY_DOWN):
		t.change_power(-POWER_RATE * delta)

	_update_guide(t)
	_update_info()

func _unhandled_input(event: InputEvent) -> void:
	if state != "aim":
		return
	if event is InputEventKey and event.pressed and not event.echo:
		if event.physical_keycode == KEY_Q:
			var t: Tank = tanks[active]
			t.weapon = (t.weapon + 1) % WEAPON_NAMES.size()
			_update_info()
		elif event.physical_keycode == KEY_SPACE:
			_fire()

func _fire() -> void:
	var t: Tank = tanks[active]
	proj = _make_proj(t.weapon)
	add_child(proj)
	proj.position = t.muzzle_pos()
	proj_vel = t.muzzle_dir() * t.power
	state = "flying"
	_update_info()

func _fly_phase(delta: float) -> void:
	proj_vel.y += GRAVITY * delta
	proj.position += proj_vel * delta
	var p: Vector2 = proj.position

	# Hit a tank directly?
	for t in tanks:
		if p.distance_to(t.position + Vector2(0, -22)) < 34.0:
			_explode(p)
			return
	# Hit the ground or leave the field?
	if p.y >= GROUND_Y or p.x < -40.0 or p.x > VIEW.x + 40.0 or p.y > VIEW.y + 40.0:
		_explode(Vector2(p.x, minf(p.y, GROUND_Y)))
		return

func _explode(pos: Vector2) -> void:
	var w: int = tanks[active].weapon
	var radius: float = EXPL_RADIUS[w]
	for t in tanks:
		var c = t.position + Vector2(0, -18)
		var dist: float = pos.distance_to(c)
		if dist < radius:
			var k: float = 1.0 - dist / radius
			t.take_damage(DIRECT_DMG[w] * k)
			var dir: Vector2 = c - pos
			if dir.length() < 1.0:
				dir = Vector2(0, -1)
			t.position.x = clampf(t.position.x + dir.normalized().x * KNOCKBACK[w] * k, 70.0, VIEW.x - 70.0)
	_explosion_fx(pos, radius)

	if proj != null:
		proj.queue_free()
		proj = null

	for t in tanks:
		if t.is_dead():
			_game_over()
			return

	settle = 0.8
	state = "settle"

func _game_over() -> void:
	state = "over"
	var win := "BLUE (P1)" if tanks[1].is_dead() else "RED (P2)"
	info.text = "%s WINS!   (press F5 to restart)" % win

# ---- visuals / hud ----
func _make_proj(w: int) -> Node2D:
	var n := Node2D.new()
	var dot := Polygon2D.new()
	var pts := PackedVector2Array()
	for i in range(10):
		var a := TAU * float(i) / 10.0
		pts.append(Vector2(cos(a), sin(a)) * 7.0)
	dot.polygon = pts
	dot.color = PROJ_COLOR[w]
	n.add_child(dot)
	return n

func _explosion_fx(pos: Vector2, radius: float) -> void:
	var fx := Polygon2D.new()
	var pts := PackedVector2Array()
	for i in range(16):
		var a := TAU * float(i) / 16.0
		pts.append(Vector2(cos(a), sin(a)) * radius)
	fx.polygon = pts
	fx.color = Color(1.0, 0.7, 0.2, 0.6)
	fx.position = pos
	fx.scale = Vector2(0.2, 0.2)
	add_child(fx)
	var tw := create_tween()
	tw.set_parallel(true)
	tw.tween_property(fx, "scale", Vector2.ONE, 0.25)
	tw.tween_property(fx, "modulate:a", 0.0, 0.4)
	tw.chain().tween_callback(fx.queue_free)

func _update_guide(t: Tank) -> void:
	var pts := PackedVector2Array()
	var pos: Vector2 = t.muzzle_pos()
	var vel: Vector2 = t.muzzle_dir() * t.power
	var dt := 1.0 / 60.0
	for i in range(GUIDE_STEPS):
		pts.append(pos)
		vel.y += GRAVITY * dt
		pos += vel * dt
		if pos.y >= GROUND_Y:
			pts.append(Vector2(pos.x, GROUND_Y))
			break
	guide.points = pts

func _update_info() -> void:
	var t: Tank = tanks[active]
	var who := "BLUE (P1)" if active == 0 else "RED (P2)"
	info.text = "%s — turn    angle %d°   power %d   fuel %d   weapon: %s\nBLUE HP %d    RED HP %d    A/D move  W/S or stick aim  R/F or arrows power  Q weapon  SPACE fire" % [
		who, roundi(t.aim_deg), roundi(t.power), roundi(t.fuel), WEAPON_NAMES[t.weapon],
		roundi(tanks[0].hp), roundi(tanks[1].hp)]

func _add_rect(pos: Vector2, size: Vector2, col: Color, z: int) -> void:
	var poly := Polygon2D.new()
	poly.polygon = PackedVector2Array([Vector2.ZERO, Vector2(size.x, 0), size, Vector2(0, size.y)])
	poly.color = col
	poly.position = pos
	poly.z_index = z
	add_child(poly)
