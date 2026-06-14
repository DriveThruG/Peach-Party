extends Node2D
class_name DashedLine
# A dashed polyline. Set the points each frame; _draw walks them drawing dash/gap segments.

var pts: PackedVector2Array = PackedVector2Array()
var dash := 16.0
var gap := 12.0
var line_width := 5.0
var col := Color(1, 1, 1, 0.7)

func set_points(p: PackedVector2Array) -> void:
	pts = p
	queue_redraw()

func _draw() -> void:
	if pts.size() < 2:
		return
	var on := true
	var rem := dash
	for i in range(pts.size() - 1):
		var a := pts[i]
		var b := pts[i + 1]
		var seg := b - a
		var seglen := seg.length()
		if seglen < 0.001:
			continue
		var dir := seg / seglen
		var d := 0.0
		while d < seglen:
			var step: float = minf(rem, seglen - d)
			if on:
				draw_line(a + dir * d, a + dir * (d + step), col, line_width)
			d += step
			rem -= step
			if rem <= 0.001:
				on = not on
				rem = dash if on else gap
