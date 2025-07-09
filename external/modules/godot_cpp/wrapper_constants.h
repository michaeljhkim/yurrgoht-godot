#pragma once

#include "core/math/math_defs.h"

namespace godot {}

/*
constants.h -> first few lines
terrain_3d_util.cpp -> line 323     PackedStringArray({"bmp", "dds", "exr", "hdr", "jpg", "jpeg", "png", "tga", "svg", "webp"});
terrain_3d_mesh_asset.cpp -> line 30    String().right()
terrain_3d_material.cpp -> line 257, 276, 304   get_start(0), get_end(0) 
*/

// Node
///#define _get_configuration_warnings() call("get_configuration_warnings")

// Object
//#define get_instance(args...) get_instance(ObjectID(args))

// Resource
//#define set_path _set_path
//#define take_over_path _take_over_path

// RenderingServer and PhysicsServer3D
//#define get_shader_parameter_list(args...) call("get_shader_parameter_list", args)
/*
#define instance_reset_physics_interpolation(args...) call("instance_reset_physics_interpolation", args)
#define free_rid(args...) call("free_rid", args)
#define force_draw() call("force_draw")
#define texture_2d_layered_create(args...) call("texture_2d_layered_create", args)
*/

// PhysicsDirectSpaceState3D
//#define intersect_ray(args...) call("intersect_ray", args)

// EditorPlugin
//#define get_undo_redo() cast_to<EditorUndoRedoManager>(call("get_undo_redo").get_validated_object())

// EditorInterface
//#define get_resource_filesystem get_resource_file_system

// FileAccess
//#undef file_exists
//#define file_exists(args...) call("file_exists", args)

// Shader
//#define get_shader_uniform_list(args...) call("get_shader_uniform_list", args)

//#define Math_PI Math::PI
//#define Math_TAU Math::TAU

//using namespace CoreBind;
/*
#undef ResourceLoader
#undef ResourceSaver
#define ResourceLoader CoreBind::ResourceLoader
#define ResourceSaver CoreBind::ResourceSaver
*/
//#undef ResourceLoader
//#undef ResourceSaver
//#undef Engine

//#define ResourceLoader CoreBind::ResourceLoader
//#define ResourceSaver CoreBind::ResourceSaver
//#define Engine CoreBind::Engine