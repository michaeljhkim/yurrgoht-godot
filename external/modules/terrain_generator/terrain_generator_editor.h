// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#pragma once

#include <godot/core/io/image.h>
#include <godot/scene/resources/image_texture.h>

#include "terrain_generator.h"
#include "terrain_generator_region.h"


class TerrainGeneratorEditor : public Object {
	GDCLASS(TerrainGeneratorEditor, Object);
	CLASS_NAME();

public: // Constants
	enum Tool {
		REGION,
		SCULPT,
		HEIGHT,
		TEXTURE,
		COLOR,
		ROUGHNESS,
		AUTOSHADER,
		HOLES,
		NAVIGATION,
		INSTANCER,
		ANGLE, // used for picking, TODO change to a picking tool
		SCALE, // used for picking
		TOOL_MAX,
	};

	static inline const char *TOOLNAME[] = {
		"Region",
		"Sculpt",
		"Height",
		"Texture",
		"Color",
		"Roughness",
		"Auto Shader",
		"Holes",
		"Navigation",
		"Instancer",
		"Angle",
		"Scale",
		"TOOL_MAX",
	};

	enum Operation {
		ADD,
		SUBTRACT,
		REPLACE,
		AVERAGE,
		GRADIENT,
		OP_MAX,
	};

	static inline const char *OPNAME[] = {
		"Add",
		"Subtract",
		"Replace",
		"Average",
		"Gradient",
		"OP_MAX",
	};

private:
	TerrainGenerator *_terrain = nullptr;

	// Painter settings & variables
	Tool _tool = REGION;
	Operation _operation = ADD;
	Dictionary _brush_data;
	Vector3 _operation_position = Vector3();
	Vector3 _operation_movement = Vector3();
	Array _operation_movement_history;
	bool _is_operating = false;
	uint64_t _last_region_bounds_error = 0;
	TypedArray<TerrainGeneratorRegion> _original_regions; // Queue for undo
	TypedArray<TerrainGeneratorRegion> _edited_regions; // Queue for redo
	TypedArray<Vector2i> _added_removed_locations; // Queue for added/removed locations
	AABB _modified_area;
	Dictionary _undo_data; // See _get_undo_data for definition
	uint64_t _last_pen_tick = 0;

	void _send_region_aabb(const Vector2i &p_region_loc, const Vector2 &p_height_range = Vector2());
	Ref<TerrainGeneratorRegion> _operate_region(const Vector2i &p_region_loc);
	void _operate_map(const Vector3 &p_global_position, const real_t p_camera_direction);
	MapType _get_map_type() const;
	bool _is_in_bounds(const Point2i &p_pixel, const Point2i &p_size) const;
	Vector2 _get_uv_position(const Vector3 &p_global_position, const int p_region_size, const real_t p_vertex_spacing) const;
	Vector2 _get_rotated_uv(const Vector2 &p_uv, const real_t p_angle) const;
	void _store_undo();
	void _apply_undo(const Dictionary &p_data);

public:
	TerrainGeneratorEditor() {}
	~TerrainGeneratorEditor() {}

	void set_terrain(TerrainGenerator *p_terrain) { _terrain = p_terrain; }
	TerrainGenerator *get_terrain() const { return _terrain; }

	void set_brush_data(const Dictionary &p_data);
	Dictionary get_brush_data() const { return _brush_data; };
	void set_tool(const Tool p_tool);
	Tool get_tool() const { return _tool; }
	void set_operation(const Operation p_operation) { _operation = CLAMP(p_operation, Operation(0), OP_MAX); }
	Operation get_operation() const { return _operation; }

	void start_operation(const Vector3 &p_global_position);
	bool is_operating() const { return _is_operating; }
	void operate(const Vector3 &p_global_position, const real_t p_camera_direction);
	void backup_region(const Ref<TerrainGeneratorRegion> &p_region);
	void stop_operation();

protected:
	static void _bind_methods();
};

VARIANT_ENUM_CAST(TerrainGeneratorEditor::Operation);
VARIANT_ENUM_CAST(TerrainGeneratorEditor::Tool);

// Inline functions

inline MapType TerrainGeneratorEditor::_get_map_type() const {
	switch (_tool) {
		case SCULPT:
		case HEIGHT:
		case INSTANCER:
			return TYPE_HEIGHT;
			break;
		case TEXTURE:
		case AUTOSHADER:
		case HOLES:
		case NAVIGATION:
		case ANGLE:
		case SCALE:
			return TYPE_CONTROL;
			break;
		case COLOR:
		case ROUGHNESS:
			return TYPE_COLOR;
			break;
		default:
			return TYPE_MAX;
	}
}

inline bool TerrainGeneratorEditor::_is_in_bounds(const Point2i &p_pixel, const Point2i &p_size) const {
	bool positive = p_pixel.x >= 0 && p_pixel.y >= 0;
	bool less_than_max = p_pixel.x < p_size.x && p_pixel.y < p_size.y;
	return positive && less_than_max;
}

inline Vector2 TerrainGeneratorEditor::_get_uv_position(const Vector3 &p_global_position, const int p_region_size, const real_t p_vertex_spacing) const {
	Vector2 descaled_position_2d = Vector2(p_global_position.x, p_global_position.z) / p_vertex_spacing;
	Vector2 region_position = descaled_position_2d / real_t(p_region_size);
	region_position = region_position.floor();
	Vector2 uv_position = (descaled_position_2d / real_t(p_region_size)) - region_position;
	return uv_position;
}

inline Vector2 TerrainGeneratorEditor::_get_rotated_uv(const Vector2 &p_uv, const real_t p_angle) const {
	Vector2 rotation_offset = Vector2(0.5f, 0.5f);
	Vector2 uv = (p_uv - rotation_offset).rotated(p_angle) + rotation_offset;
	return uv.clamp(V2_ZERO, Vector2(1.f, 1.f));
}
