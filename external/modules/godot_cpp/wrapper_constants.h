#pragma once

#include "servers/rendering_server.h"

namespace godot {
    class RenderingServer : public ::RenderingServer {
	    //GDCLASS(RenderingServer, ::RenderingServer);
    public:
	    static RenderingServer *singleton;
        static RenderingServer *get_singleton();
        static RenderingServer *create();
	    static RenderingServer *(*create_func)();

        void free_rid(RID p_rid) {
            free(p_rid);
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
    };
    RenderingServer *RenderingServer::singleton = nullptr;
    RenderingServer *(*RenderingServer::create_func)() = nullptr;
    RenderingServer *RenderingServer::get_singleton() { return singleton; }
    RenderingServer *RenderingServer::create() {
        ERR_FAIL_COND_V(singleton, nullptr);
        return create_func ? create_func() : nullptr;
    }


    class ObjectDB : public ::ObjectDB {
	    //GDCLASS(ObjectDB, ::ObjectDB);
    public:
	    _ALWAYS_INLINE_ static Object *get_instance(uint64_t p_instance_id) {
            return ::ObjectDB::get_instance(ObjectID(p_instance_id));
        }
    };
}