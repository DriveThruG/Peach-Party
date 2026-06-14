extends Node2D
# Peach Artillery — turn-based tank duel. Hotseat for now (the ACTIVE player uses the keys; turns switch
# after each shot). Multiplayer turn-sync comes later. Placeholder shapes; sprite-sheet visuals next.
#
# Controls (active player only):  A/D move (costs fuel)   W/S aim   R/F power   Q weapon   SPACE fire

const VIEW := Vector2(1280, 720)
# Ground surface sampled from Background.png (the wavy sand), evenly spaced 0..1280. terrain_y() lerps it.
const TERRAIN := [
	Vector2(0, 432), Vector2(40, 427), Vector2(80, 422), Vector2(120, 417), Vector2(160, 412), Vector2(200, 408),
	Vector2(240, 406), Vector2(280, 407), Vector2(320, 415), Vector2(360, 425), Vector2(400, 435), Vector2(440, 439),
	Vector2(480, 437), Vector2(520, 433), Vector2(560, 431), Vector2(600, 433), Vector2(640, 441), Vector2(680, 452),
	Vector2(720, 462), Vector2(760, 468), Vector2(800, 469), Vector2(840, 468), Vector2(880, 465), Vector2(920, 461),
	Vector2(960, 456), Vector2(1000, 450), Vector2(1040, 443), Vector2(1080, 438), Vector2(1120, 437), Vector2(1160, 438),
	Vector2(1200, 439), Vector2(1240, 439), Vector2(1280, 438)
]
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
var tune_label: Label
var guide: DashedLine
var tune_pivot := Tank.TURRET_POS

# Camera (zooms onto the active player; zooms out between turns).
const ZOOM_IN := Vector2(1.7, 1.7)
const ZOOM_OUT := Vector2(1.0, 1.0)
var cam: Camera2D
var cam_tween: Tween

func _ready() -> void:
	var bg := Sprite2D.new()
	bg.texture = load("res://artillery/art/Background.png")
	bg.centered = true
	bg.position = VIEW * 0.5
	var ts := bg.texture.get_size()
	bg.scale = Vector2(VIEW.x / ts.x, VIEW.y / ts.y)
	bg.z_index = -20
	add_child(bg)

	_spawn_tank(220.0, 0, 1)      # green tank, faces right
	_spawn_tank(1060.0, 1, -1)    # orange tank, faces left

	guide = DashedLine.new()
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

	tune_label = Label.new()
	tune_label.add_theme_font_size_override("font_size", 18)
	tune_label.add_theme_color_override("font_color", Color.WHITE)
	tune_label.add_theme_color_override("font_outline_color", Color.BLACK)
	tune_label.add_theme_constant_override("outline_size", 5)
	tune_label.position = Vector2(20, VIEW.y - 40)
	hud.add_child(tune_label)

	cam = Camera2D.new()
	cam.limit_left = 0
	cam.limit_top = 0
	cam.limit_right = int(VIEW.x)
	cam.limit_bottom = int(VIEW.y)
	cam.position = VIEW * 0.5
	cam.zoom = ZOOM_OUT
	add_child(cam)
	cam.make_current()

	_update_info()
	_focus_active()

# ---- camera ----
func _focus(pos: Vector2, zoom: Vector2, dur: float) -> void:
	if cam_tween != null:
		cam_tween.kill()
	cam_tween = create_tween()
	cam_tween.set_parallel(true)
	cam_tween.set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	cam_tween.tween_property(cam, "position", pos, dur)
	cam_tween.tween_property(cam, "zoom", zoom, dur)

func _focus_active() -> void:
	var t: Tank = tanks[active]
	_focus(t.position + Vector2(0, -40), ZOOM_IN, 0.6)

func _zoom_out() -> void:
	_focus(VIEW * 0.5, ZOOM_OUT, 0.5)

func _spawn_tank(x: float, variant: int, facing: int) -> void:
	var t := Tank.new()
	add_child(t)
	t.position = Vector2(x, terrain_y(x))
	t.rotation = terrain_angle(x)
	t.setup(variant, facing)
	tanks.append(t)

# Ground height at world x (lerp the sampled TERRAIN curve).
func terrain_y(x: float) -> float:
	var step := VIEW.x / float(TERRAIN.size() - 1)
	var fx := clampf(x, 0.0, VIEW.x) / step
	var i := int(fx)
	if i >= TERRAIN.size() - 1:
		return TERRAIN[TERRAIN.size() - 1].y
	return lerpf(TERRAIN[i].y, TERRAIN[i + 1].y, fx - float(i))

# Slope angle of the ground at world x — used to tilt the tank so it sits on the hill.
func terrain_angle(x: float) -> float:
	return atan2(terrain_y(x + 12.0) - terrain_y(x - 12.0), 24.0)

func _process(delta: float) -> void:
	guide.visible = (state == "aim")
	_tune_barrel(delta)
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
				_focus_active()      # zoom onto the new active player

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
		t.position.y = terrain_y(t.position.x)      # follow the wavy ground
		t.rotation = terrain_angle(t.position.x)    # tilt to the slope
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
	proj_vel = t.muzzle_world_dir() * t.power
	state = "flying"
	_zoom_out()                      # zoom out to watch the shot
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
	if p.y >= terrain_y(p.x) or p.x < -40.0 or p.x > VIEW.x + 40.0 or p.y > VIEW.y + 40.0:
		_explode(Vector2(p.x, minf(p.y, terrain_y(p.x))))
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
			t.position.y = terrain_y(t.position.x)
			t.rotation = terrain_angle(t.position.x)
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
	_zoom_out()
	info.visible = false
	tune_label.visible = false
	guide.visible = false
	if tanks[0].is_dead():
		_show_winner("ORANGE", Color(1.0, 0.55, 0.2))
	else:
		_show_winner("GREEN", Color(0.5, 0.9, 0.4))

func _show_winner(team_name: String, col: Color) -> void:
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.0)
	dim.size = VIEW
	hud.add_child(dim)
	create_tween().tween_property(dim, "color:a", 0.5, 0.5)

	var label := Label.new()
	label.text = "%s WINS!" % team_name
	label.add_theme_font_size_override("font_size", 100)
	label.add_theme_color_override("font_color", col)
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", 16)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.size = Vector2(900, 200)
	label.position = Vector2(VIEW.x * 0.5 - 450, VIEW.y * 0.5 - 140)
	label.pivot_offset = Vector2(450, 100)
	hud.add_child(label)
	label.modulate.a = 0.0
	label.scale = Vector2(0.5, 0.5)
	var tw := create_tween()
	tw.set_parallel(true)
	tw.tween_property(label, "modulate:a", 1.0, 0.3)
	tw.tween_property(label, "scale", Vector2.ONE, 0.55).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)

	var sub := Label.new()
	sub.text = "press F5 to restart"
	sub.add_theme_font_size_override("font_size", 30)
	sub.add_theme_color_override("font_color", Color.WHITE)
	sub.add_theme_color_override("font_outline_color", Color.BLACK)
	sub.add_theme_constant_override("outline_size", 6)
	sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	sub.size = Vector2(900, 40)
	sub.position = Vector2(VIEW.x * 0.5 - 450, VIEW.y * 0.5 + 55)
	hud.add_child(sub)
	sub.modulate.a = 0.0
	var tw2 := create_tween()
	tw2.tween_interval(0.55)
	tw2.tween_property(sub, "modulate:a", 1.0, 0.4)

# ---- visuals / hud ----
func _make_proj(_w: int) -> Node2D:
	var n := Node2D.new()
	var s := Sprite2D.new()
	s.texture = Tank.SHEET
	s.region_enabled = true
	s.region_rect = Tank.PEACH_REGION
	s.scale = Vector2(0.15, 0.15)
	n.add_child(s)
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
	var vel: Vector2 = t.muzzle_world_dir() * t.power
	var dt := 1.0 / 60.0
	for i in range(GUIDE_STEPS):
		pts.append(pos)
		vel.y += GRAVITY * dt
		pos += vel * dt
		if pos.y >= terrain_y(pos.x):
			pts.append(Vector2(pos.x, terrain_y(pos.x)))
			break
	guide.set_points(pts)

# Live-tune the barrel mount point on both tanks (J/L = x, I/K = y). Read off the value, tell me, I bake it.
func _tune_barrel(delta: float) -> void:
	var step := 30.0 * delta
	var changed := false
	if Input.is_physical_key_pressed(KEY_J):
		tune_pivot.x -= step
		changed = true
	if Input.is_physical_key_pressed(KEY_L):
		tune_pivot.x += step
		changed = true
	if Input.is_physical_key_pressed(KEY_I):
		tune_pivot.y -= step
		changed = true
	if Input.is_physical_key_pressed(KEY_K):
		tune_pivot.y += step
		changed = true
	if changed:
		for t in tanks:
			t.set_turret_pos(tune_pivot)
	tune_label.text = "turret tune  J/L x  I/K y   ->   TURRET_POS=(%d, %d)" % [roundi(tune_pivot.x), roundi(tune_pivot.y)]

func _update_info() -> void:
	var t: Tank = tanks[active]
	info.text = "Weapon: %s        A/D move   W/S aim   R/F or arrows power   Q weapon   SPACE fire" % WEAPON_NAMES[t.weapon]

func _add_rect(pos: Vector2, size: Vector2, col: Color, z: int) -> void:
	var poly := Polygon2D.new()
	poly.polygon = PackedVector2Array([Vector2.ZERO, Vector2(size.x, 0), size, Vector2(0, size.y)])
	poly.color = col
	poly.position = pos
	poly.z_index = z
	add_child(poly)
