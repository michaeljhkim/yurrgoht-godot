#include "servers/rendering_server.h"
#include "servers/physics_server_3d.h"

class RenderingServer_Wrapper : public RenderingServer {
public:
    static RenderingServer_Wrapper *get_singleton();
    static RenderingServer_Wrapper *create();

    TypedArray<Dictionary> get_shader_parameter_list(RID p_shader) const {
        List<PropertyInfo> l;
        RenderingServer::get_shader_parameter_list(p_shader, &l);
        return convert_property_list(&l);
    }

private:
    static RenderingServer_Wrapper *singleton;
    static RenderingServer_Wrapper *(*create_func)();
};
RenderingServer_Wrapper *RenderingServer_Wrapper::singleton = nullptr;
RenderingServer_Wrapper *(*RenderingServer_Wrapper::create_func)() = nullptr;

RenderingServer_Wrapper *RenderingServer_Wrapper::get_singleton() {
    return singleton;
}


#define free_rid free
#define force_draw() draw(true, 0.0)            // only the default value defined version is needed


// generated_texture.cpp
static Vector<Ref<Image>> _get_imgvec(const TypedArray<Image> &p_layers) {
	Vector<Ref<Image>> images;
	images.resize(p_layers.size());
	for (int i = 0; i < p_layers.size(); i++) {
		images.write[i] = p_layers[i];
	}
	return images;
}
#define texture_2d_layered_create(p_layers, p_layered_type) texture_2d_layered_create(_get_imgvec(p_layers), p_layered_type)