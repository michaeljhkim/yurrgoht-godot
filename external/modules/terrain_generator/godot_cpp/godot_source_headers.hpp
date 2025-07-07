/*
#include "godot_cpp/godot_source_headers.hpp"
*/
// C++ headers
#include <array>

// gdextension
#include "core/extension/gdextension.h"

// classes
#include "scene/3d/camera_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/3d/visual_instance_3d.h"

#include "scene/resources/3d/height_map_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/compositor.h"
#include "scene/resources/environment.h"
#include "scene/resources/gradient.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/multimesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/physics_material.h"
#include "scene/resources/shader.h"
#include "scene/resources/surface_tool.h"
#include "scene/resources/texture.h"

#include "core/core_bind.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
//#include "core/io/resource_loader.h"
//#include "core/io/resource_saver.h"
#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/variant/variant.h"

#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_paths.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/editor_undo_redo_manager.h"

#include "godot/core/config/engine.h"
#include "godot/core/io/resource.h"

#include "modules/noise/fastnoise_lite.h"
#include "modules/noise/noise_texture_2d.h"
#include "modules/regex/regex.h"
#include "modules/register_module_types.h"

#include "servers/physics_server_3d.h"
#include "servers/rendering_server.h"
#include "servers/text/text_server_extension.h"

#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/viewport.h"

// core
#include "core/object/class_db.h"
#include "core/version.h"

// variant
#include "core/variant/variant_utility.h"

// not sure if I should do this, it might be cyclical
namespace godot {}
//#include "src/constants.h"
#include "misc/math.hpp"

// generated_texture.cpp
static Vector<Ref<Image>> _get_imgvec(const TypedArray<Image> &p_layers) {
	Vector<Ref<Image>> images;
	images.resize(p_layers.size());
	for (int i = 0; i < p_layers.size(); i++) {
		images.write[i] = p_layers[i];
	}
	return images;
}

#define free_rid free
#define texture_2d_layered_create(p_layers, p_layered_type) texture_2d_layered_create(_get_imgvec(p_layers), p_layered_type)

// target_node_3d.h
#define get_instance(i_id) get_instance(ObjectID(i_id))

// terrain_3d_assets.cpp
typedef CoreBind::ResourceSaver ResourceSaver;
#define take_over_path(p_path) set_path(p_path, true)
#define force_draw() draw(true, 0.0)            // only the default value defined version is needed

// terrain_3d_data.cpp
typedef CoreBind::ResourceLoader ResourceLoader;
#define get_resource_filesystem() get_resource_file_system()
#define file_exists(path) exists(path)

// terrain_3d_editor.cpp
typedef VariantUtilityFunctions UtilityFunctions;
//#define get_undo_redo() EditorUndoRedoManager::get_singleton()

// get_undo_redo() in EditorPlugin is protected, needed to gain access
class EditorPluginRedo : public EditorPlugin {
public:
    EditorUndoRedoManager *get_undo_redo() { return EditorPlugin::get_undo_redo(); }
};

#define EditorPlugin EditorPluginRedo
#include "src/terrain_3d.h"
#include "src/terrain_3d_editor.cpp"
#undef EditorPlugin