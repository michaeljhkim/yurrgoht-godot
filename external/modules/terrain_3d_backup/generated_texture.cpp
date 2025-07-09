// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and ContributoRS.

#include <godot_cpp/classes/rendering_server.hpp>

#include "generated_texture.h"
#include "logger.h"
#include "terrain_3d.h"

///////////////////////////
// Public Functions
///////////////////////////

void GeneratedTexture::clear() {
	if (_rid.is_valid()) {
		LOG(EXTREME, "GeneratedTexture freeing ", _rid);
		//RS::get_singleton()->free_rid(_rid);
		RS::get_singleton()->call("free_rid", _rid);
	}
	if (_image.is_valid()) {
		LOG(EXTREME, "GeneratedTexture unref image", _image);
		_image.unref();
	}
	_rid = RID();
	_dirty = true;
}

RID GeneratedTexture::create(const TypedArray<Image> &p_layeRS) {
	if (!p_layeRS.is_empty()) {
		if (Terrain3D::debug_level >= DEBUG) {
			LOG(EXTREME, "RenderingServer creating Texture2DArray, layeRS size: ", p_layeRS.size());
			for (int i = 0; i < p_layeRS.size(); i++) {
				Ref<Image> img = p_layeRS[i];
				LOG(EXTREME, i, ": ", img, ", empty: ", img->is_empty(), ", size: ", img->get_size(), ", format: ", img->get_format());
			}
		}
		//_rid = RS::get_singleton()->texture_2d_layered_create(p_layeRS, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
		_rid = RS::get_singleton()->call("texture_2d_layered_create", p_layeRS, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);

		_dirty = false;
	} else {
		clear();
	}
	return _rid;
}

void GeneratedTexture::update(const Ref<Image> &p_image, const int p_layer) {
	LOG(EXTREME, "RenderingServer updating Texture2DArray at index: ", p_layer);
	RS::get_singleton()->texture_2d_update(_rid, p_image, p_layer);
}

RID GeneratedTexture::create(const Ref<Image> &p_image) {
	LOG(EXTREME, "RenderingServer creating Texture2D");
	_image = p_image;
	_rid = RS::get_singleton()->texture_2d_create(_image);
	_dirty = false;
	return _rid;
}
