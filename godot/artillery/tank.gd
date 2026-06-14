extends Node2D
class_name Tank
# Peach Artillery — one tank. Placeholder shapes for now (swap to your sprite-sheet regions later).
# Origin = the ground-contact point (bottom centre). Barrel pivots at the turret.

const W := 78.0
const H := 30.0
const BARREL_LEN := 50.0
const MAX_HP := 100.0

var hp := MAX_HP
var fuel := 100.0
var max_fuel := 100.0
var facing := 1            # +1 = aims/fires right, -1 = left
var aim_deg := 45.0        # 0..85 above horizontal
var power := 520.0
var weapon := 0
var team_color := Color.WHITE

var barrel: Node2D
var hp_fill: Polygon2D

func setup(col: Color, in_facing: int) -> void:
	team_color = col
	facing = in_facing

	_add_rect(self, Vector2(W, H), Vector2(0, -H * 0.5), col)                 # hull
	_add_rect(self, Vector2(40, 22), Vector2(0, -H - 7), col.darkened(0.18))  # turret dome

	barrel = Node2D.new()
	barrel.position = Vector2(0, -H - 7)
	add_child(barrel)
	_add_rect(barrel, Vector2(BARREL_LEN, 9), Vector2(BARREL_LEN * 0.5, 0), Color(0.14, 0.14, 0.16))

	# Health bar (left-anchored: the fill's left edge stays put, it shrinks to the right).
	_add_rect(self, Vector2(W, 9), Vector2(0, -H - 40), Color(0, 0, 0, 0.6))
	hp_fill = Polygon2D.new()
	hp_fill.polygon = PackedVector2Array([Vector2(0, -2.5), Vector2(W - 4, -2.5), Vector2(W - 4, 2.5), Vector2(0, 2.5)])
	hp_fill.color = Color(0.2, 0.9, 0.3)
	hp_fill.position = Vector2(-(W - 4) * 0.5, -H - 40)
	add_child(hp_fill)

	_refresh()

func muzzle_dir() -> Vector2:
	var a := deg_to_rad(aim_deg)
	return Vector2(cos(a) * facing, -sin(a))

func muzzle_pos() -> Vector2:
	return barrel.global_position + muzzle_dir() * BARREL_LEN

func aim(d: float) -> void:
	aim_deg = clampf(aim_deg + d, 0.0, 85.0)
	_refresh()

func change_power(d: float) -> void:
	power = clampf(power + d, 150.0, 950.0)

func take_damage(dmg: float) -> void:
	hp = maxf(0.0, hp - dmg)
	_refresh()

func is_dead() -> bool:
	return hp <= 0.0

func _refresh() -> void:
	barrel.rotation = muzzle_dir().angle()
	var frac := clampf(hp / MAX_HP, 0.0, 1.0)
	hp_fill.scale.x = frac
	hp_fill.color = Color(0.9, 0.2, 0.2).lerp(Color(0.2, 0.9, 0.3), frac)

func _add_rect(parent: Node, sz: Vector2, off: Vector2, col: Color) -> Polygon2D:
	var poly := Polygon2D.new()
	poly.polygon = PackedVector2Array([
		Vector2(-sz.x * 0.5, -sz.y * 0.5), Vector2(sz.x * 0.5, -sz.y * 0.5),
		Vector2(sz.x * 0.5, sz.y * 0.5), Vector2(-sz.x * 0.5, sz.y * 0.5)])
	poly.color = col
	poly.position = off
	parent.add_child(poly)
	return poly
