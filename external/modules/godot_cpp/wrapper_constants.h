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

namespace godot {
    class ObjectDB : public ::ObjectDB {
    public:
	    _ALWAYS_INLINE_ static Object *get_instance(uint64_t p_instance_id) {
            return ::ObjectDB::get_instance(ObjectID(p_instance_id));
        }
    };


    class Resource : public ::Resource {
        GDCLASS(Resource, ::Resource);
        static EditorInterface *singleton;
    public:
	    void take_over_path(const String &p_path) {
            _take_over_path(p_path);
        }
    };


    class RenderingServer : public ::RenderingServer {
	    GDCLASS(RenderingServer, ::RenderingServer);
    public:
	    static RenderingServer *singleton;
        static RenderingServer *get_singleton() { return singleton; }
	    static RenderingServer *(*create_func)();
        static RenderingServer *create() {
            ERR_FAIL_COND_V(singleton, nullptr);
            return create_func ? create_func() : nullptr;
        }

        void free_rid(RID p_rid) {
            free(p_rid);
        }

        void force_draw(bool p_swap_buffers = true, double frame_step = 0.0) {
            draw(p_swap_buffers, frame_step);
        }

        static Vector<Ref<Image>> _get_imgvec(const TypedArray<Image> &p_layers) {
            Vector<Ref<Image>> images;
            images.resize(p_layers.size());
            for (int i = 0; i < p_layers.size(); i++) {
                images.write[i] = p_layers[i];
            }
            return images;
        }

        RID texture_2d_layered_create(const TypedArray<Image> &p_layers, TextureLayeredType p_layered_type) {
            return ::RenderingServer::texture_2d_layered_create(_get_imgvec(p_layers), p_layered_type);
        }

        TypedArray<Dictionary> get_shader_parameter_list(RID p_shader) const {
            List<PropertyInfo> l;
            ::RenderingServer::get_shader_parameter_list(p_shader, &l);
            return convert_property_list(&l);
        }

        void instance_reset_physics_interpolation(RID p_instance) {
            _instance_reset_physics_interpolation_bind_compat_104269(p_instance);
        }

    };
    RenderingServer *RenderingServer::singleton = nullptr;
    RenderingServer *(*RenderingServer::create_func)() = nullptr;



    class PhysicsServer3D : public ::PhysicsServer3D {
	    GDCLASS(PhysicsServer3D, ::PhysicsServer3D);
    public:
	    static PhysicsServer3D *singleton;
        static PhysicsServer3D *get_singleton() { return singleton; }

        void free_rid(RID p_rid) {
            free(p_rid);
        }
    };
    PhysicsServer3D *PhysicsServer3D::singleton = nullptr;


    class EditorPlugin : public ::EditorPlugin {
        GDCLASS(EditorPlugin, ::EditorPlugin);
    public:
        EditorUndoRedoManager *get_undo_redo() {
            return ::EditorPlugin::get_undo_redo();
        }
    };

    class EditorInterface : public ::EditorInterface {
        GDCLASS(EditorInterface, ::EditorInterface);
        static EditorInterface *singleton;
    public:
	    static EditorInterface *get_singleton() { return singleton; }
        EditorFileSystem *get_resource_filesystem() const {
            return get_resource_file_system();
        }
        
    };

    class ResourceLoader : public CoreBind::ResourceLoader {
        GDCLASS(ResourceLoader, CoreBind::ResourceLoader);
    public:
    };


    class ResourceSaver : public CoreBind::ResourceSaver {
        GDCLASS(ResourceSaver, CoreBind::ResourceSaver);
    public:
    };


    class Shader : public ::Shader {
        GDCLASS(Shader, ::Shader);
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
        GDCLASS(PhysicsRayQueryParameters3D, ::PhysicsRayQueryParameters3D);
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
        GDCLASS(PhysicsDirectSpaceState3D, ::PhysicsDirectSpaceState3D);
    public:
        Dictionary intersect_ray(const Ref<PhysicsRayQueryParameters3D> &p_ray_query) {
            ERR_FAIL_COND_V(p_ray_query.is_null(), Dictionary());

            RayResult result;
            bool res = ::PhysicsDirectSpaceState3D::intersect_ray(p_ray_query->get_parameters(), result);

            if (!res) {
                return Dictionary();
            }

            Dictionary d;
            d["position"] = result.position;
            d["normal"] = result.normal;
            d["face_index"] = result.face_index;
            d["collider_id"] = result.collider_id;
            d["collider"] = result.collider;
            d["shape"] = result.shape;
            d["rid"] = result.rid;

            return d;
        }
    };


    class Node : public ::Node {
        GDCLASS(Node, ::Node);
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