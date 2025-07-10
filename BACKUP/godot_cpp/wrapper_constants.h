#pragma once

#include <array>
#include "servers/rendering_server.h"
#include "servers/physics_server_3d.h"
#include "scene/resources/multimesh.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/main/node.h"

#include "core/io/resource.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/editor_interface.h"

#include "core/core_bind.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/editor_undo_redo_manager.h"

#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"

namespace godot {
    class ObjectDB : public ::ObjectDB {
    public:
	    _ALWAYS_INLINE_ static Object *get_instance(uint64_t p_instance_id) {
            return ::ObjectDB::get_instance(ObjectID(p_instance_id));
        }
    };


    class Resource : public ::Resource {
        GDSOFTCLASS(Resource, ::Resource);
        static EditorInterface *singleton;
    public:
	    void take_over_path(const String &p_path) {
            _take_over_path(p_path);
        }

        //virtual void set_name(const String &p_name) = 0;
        //virtual String get_name() const = 0;
    };


    class RenderingServer : public ::RenderingServer {
	    GDSOFTCLASS(RenderingServer, ::RenderingServer);

    public:
        void free_rid(RID p_rid) {
            call("free_rid", p_rid);
        }

        void force_draw(bool p_swap_buffers = true, double frame_step = 0.0) {
            call("force_draw", p_swap_buffers, frame_step);
        }

        RID texture_2d_layered_create(const TypedArray<Image> &p_layers, TextureLayeredType p_layered_type) {
            return call("_texture_2d_layered_create", p_layers, p_layered_type);
        }

        TypedArray<Dictionary> get_shader_parameter_list(RID p_shader) const {
            return get_singleton()->call("_shader_get_shader_parameter_list", p_shader);
        }

        void instance_reset_physics_interpolation(RID p_instance) {
            _instance_reset_physics_interpolation_bind_compat_104269(p_instance);
        }

    };



    class PhysicsServer3D : public ::PhysicsServer3D {
	    GDSOFTCLASS(PhysicsServer3D, ::PhysicsServer3D);
    public:
        void free_rid(RID p_rid) {
            call("free_rid", p_rid);
        }
    };


    class EditorPlugin : public ::EditorPlugin {
        GDSOFTCLASS(EditorPlugin, ::EditorPlugin);
    public:
        EditorUndoRedoManager *get_undo_redo() {
            return EditorUndoRedoManager::get_singleton();
        }
    };

    class EditorInterface : public ::EditorInterface {
        GDSOFTCLASS(EditorInterface, ::EditorInterface);
    public:
        EditorFileSystem *get_resource_filesystem() const {
            return get_resource_file_system();
        }
        
    };


    class ResourceLoader : public CoreBind::ResourceLoader {
        GDSOFTCLASS(ResourceLoader, CoreBind::ResourceLoader);
    public:
    };


    class ResourceSaver : public CoreBind::ResourceSaver {
        GDSOFTCLASS(ResourceSaver, CoreBind::ResourceSaver);
    public:
    };


    class Shader : public ::Shader {
        GDSOFTCLASS(Shader, ::Shader);
    public:
        Array get_shader_uniform_list(bool p_get_groups) {
            List<PropertyInfo> uniform_list;
            ::Shader::get_shader_uniform_list(&uniform_list, p_get_groups);
            Array ret;
            for (const PropertyInfo &pi : uniform_list) {
                ret.push_back(pi.operator Dictionary());
            }
            return ret;
        }
    };



    class PhysicsRayQueryParameters3D : public ::PhysicsRayQueryParameters3D {
        GDSOFTCLASS(PhysicsRayQueryParameters3D, ::PhysicsRayQueryParameters3D);
    public:
        static Ref<PhysicsRayQueryParameters3D> create(Vector3 p_from, Vector3 p_to, uint32_t p_mask = UINT32_MAX, const TypedArray<RID> &p_exclude = TypedArray<RID>()) {
            Ref<PhysicsRayQueryParameters3D> params;
            params.instantiate();
            params->set_from(p_from);
            params->set_to(p_to);
            params->set_collision_mask(p_mask);
            params->set_exclude(p_exclude);
            return params;
        }
    };



    class PhysicsDirectSpaceState3D : public ::PhysicsDirectSpaceState3D {
        GDSOFTCLASS(PhysicsDirectSpaceState3D, ::PhysicsDirectSpaceState3D);
    public:
        Dictionary intersect_ray(const Ref<PhysicsRayQueryParameters3D> &p_ray_query) {
            return call("_intersect_ray", p_ray_query);
        }
    };


    class Node : public ::Node {
        GDSOFTCLASS(Node, ::Node);
    public:
        Node *get_node_internal(const NodePath &p_path) const {
            return cast_to<Node>(::Node::get_node(p_path));
        }
    };
}

#define RenderingServer godot::RenderingServer
#define PhysicsServer3D godot::PhysicsServer3D
#define ObjectDB godot::ObjectDB
#define Resource godot::Resource
#define Shader godot::Shader
#define EditorPlugin godot::EditorPlugin
#define EditorInterface godot::EditorInterface

#define ResourceLoader godot::ResourceLoader
#define ResourceSaver godot::ResourceSaver

#define PhysicsRayQueryParameters3D godot::PhysicsRayQueryParameters3D
//#define PhysicsDirectSpaceState3D godot::PhysicsDirectSpaceState3D
#define Node godot::Node


#define Math_PI Math::PI
#define Math_TAU Math::TAU