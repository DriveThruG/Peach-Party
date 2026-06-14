extends Node2D
class_name Tank
# Peach Artillery — one tank, built from the Artillery.png sheet.
# Tree: self(feet) > hull + turret (sprites) > barrel(Node2D, rotates to aim) > barrel sprite.
# Origin = ground-contact point (bottom centre).

const SHEET := preload("res://artillery/art/Artillery.png")
const TANK_SCALE := 0.36
const MAX_HP := 100.0

# Sheet regions — index 0 = green (P1), 1 = orange (P2).
const HULL_REGION := [Rect2(44, 84, 256, 164), Rect2(44, 292, 256, 168)]
const TURRET_REGION := [Rect2(336, 84, 188, 148), Rect2(336, 292, 188, 148)]
const BARREL_REGION := [Rect2(556, 156, 156, 48), Rect2(556, 364, 160, 48)]
const PEACH_REGION := Rect2(768, 212, 132, 136)

# Turret mount point in world px (post-scale) — where the turret dome + barrel sit in the hull hole.
const TURRET_POS := Vector2(0, -44)
const BAR_W := 98.0

var hp := MAX_HP
var fuel := 160.0
var max_fuel := 160.0
var facing := 1
var aim_deg := 45.0
var power := 520.0
var weapon := 0
var team_color := Color.WHITE
var barrel_len := 56.0

var barrel: Node2D
var turret_sprite: Sprite2D
var hp_fill: Polygon2D
var fuel_fill: Polygon2D

func setup(variant: int, in_facing: int) -> void:
	facing = in_facing
	team_color = Color(0.45, 0.8, 0.3) if variant == 0 else Color(1.0, 0.55, 0.2)

	# Hull (with the turret hole), z=0.
	var hr: Rect2 = HULL_REGION[variant]
	var hull := _sprite(hr, Vector2(0, -hr.size.y * TANK_SCALE * 0.5), self)
	hull.z_index = 0
	if facing == -1:
		hull.flip_h = true

	# Barrel (z=1) mounts at the turret; the turret dome (z=2) sits in the hole and covers the barrel base.
	barrel = Node2D.new()
	barrel.position = TURRET_POS
	barrel.z_index = 1
	add_child(barrel)
	var br: Rect2 = BARREL_REGION[variant]
	barrel_len = br.size.x * TANK_SCALE
	_sprite(br, Vector2(barrel_len * 0.5, 0), barrel)

	turret_sprite = _sprite(TURRET_REGION[variant], TURRET_POS, self)
	turret_sprite.z_index = 2
	if facing == -1:
		turret_sprite.flip_h = true

	hp_fill = _make_bar(-108.0, Color(0.2, 0.9, 0.3))
	fuel_fill = _make_bar(-94.0, Color(0.3, 0.7, 1.0))
	_refresh()

func set_turret_pos(p: Vector2) -> void:
	if turret_sprite != null:
		turret_sprite.position = p
	if barrel != null:
		barrel.position = p

func muzzle_dir() -> Vector2:
	var a := deg_to_rad(aim_deg)
	return Vector2(cos(a) * facing, -sin(a))

func muzzle_pos() -> Vector2:
	return barrel.global_position + muzzle_dir() * barrel_len

func aim(d: float) -> void:
	aim_deg = clampf(aim_deg + d, 0.0, 85.0)
	_refresh()

func set_aim_deg(d: float) -> void:
	aim_deg = clampf(d, 0.0, 85.0)
	_refresh()

func change_power(d: float) -> void:
	power = clampf(power + d, 150.0, 950.0)

func take_damage(dmg: float) -> void:
	hp = maxf(0.0, hp - dmg)
	_refresh()

func spend_fuel(amt: float) -> void:
	fuel = maxf(0.0, fuel - amt)
	_refresh()

func is_dead() -> bool:
	return hp <= 0.0

func _refresh() -> void:
	barrel.rotation = muzzle_dir().angle()
	var frac := clampf(hp / MAX_HP, 0.0, 1.0)
	hp_fill.scale.x = frac
	hp_fill.color = Color(0.9, 0.2, 0.2).lerp(Color(0.2, 0.9, 0.3), frac)
	fuel_fill.scale.x = clampf(fuel / max_fuel, 0.0, 1.0)

func _sprite(region: Rect2, pos: Vector2, parent: Node) -> Sprite2D:
	var s := Sprite2D.new()
	s.texture = SHEET
	s.region_enabled = true
	s.region_rect = region
	s.scale = Vector2(TANK_SCALE, TANK_SCALE)
	s.position = pos
	parent.add_child(s)
	return s

func _make_bar(y: float, col: Color) -> Polygon2D:
	var bg := Polygon2D.new()
	bg.polygon = PackedVector2Array([Vector2(-BAR_W * 0.5, -5), Vector2(BAR_W * 0.5, -5), Vector2(BAR_W * 0.5, 5), Vector2(-BAR_W * 0.5, 5)])
	bg.color = Color(0, 0, 0, 0.6)
	bg.position = Vector2(0, y)
	add_child(bg)
	var fill := Polygon2D.new()
	fill.polygon = PackedVector2Array([Vector2(0, -3.5), Vector2(BAR_W - 4, -3.5), Vector2(BAR_W - 4, 3.5), Vector2(0, 3.5)])
	fill.color = col
	fill.position = Vector2(-(BAR_W - 4) * 0.5, y)
	add_child(fill)
	return fill
