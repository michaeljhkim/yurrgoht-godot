#pragma once

#include "core/object/object.h"
#include "scene/3d/node_3d.h"
#include "core/io/resource.h"
#include "servers/rendering_server.h"
#include "servers/rendering_server.compat.inc"
#include "servers/physics_server_3d.h"
#include "scene/resources/3d/world_3d.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/editor_interface.h"

#include "core/io/file_access.h"
#define MODULE_WRAPPER 1

#include "modules/regex/regex.h"
#include "scene/resources/shader.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/multimesh.h"

namespace godot {}

/*
constants.h -> first few lines
terrain_3d_util.cpp -> line 323     PackedStringArray({"bmp", "dds", "exr", "hdr", "jpg", "jpeg", "png", "tga", "svg", "webp"});
terrain_3d_mesh_asset.cpp -> line 30    String().right()
terrain_3d_material.cpp -> line 257, 276, 304   get_start(0), get_end(0) 
*/


// Node
#define get_node_internal(p_args) cast_to<Node>(call("get_node_internal", p_args).get_validated_object())
#define _get_configuration_warnings() get_configuration_warnings()

// Object
#define get_instance(p_id) get_instance(ObjectID(p_id))

// Resource
#define set_path(p_path) call("set_path", p_path)   
#define take_over_path(p_path) call("take_over_path", p_path)

// RenderingServer and PhysicsServer3D
#define get_shader_parameter_list(p_shader) call("get_shader_parameter_list", p_shader)
#define instance_reset_physics_interpolation(p_instance) call("instance_reset_physics_interpolation", p_instance)
#define free_rid(p_rid) call("free_rid", p_rid)
#define force_draw() RenderingServer::call("force_draw")
#define texture_2d_layered_create(p_layers, p_layered_type) call("texture_2d_layered_create", p_layers, p_layered_type)


// PhysicsDirectSpaceState3D
#define intersect_ray(p_ray_query) call("intersect_ray", p_ray_query)

// EditorPlugin
#define get_undo_redo() cast_to<EditorUndoRedoManager>(this->call("get_undo_redo").get_validated_object())

// EditorInterface
#define get_resource_filesystem() cast_to<EditorFileSystem>(this->call("get_resource_filesystem").get_validated_object())

// FileAccess
#define file_exists(p_file_name) exists(p_file_name)

// Shader
#define get_shader_uniform_list(p_arg) call("get_shader_uniform_list", p_arg)