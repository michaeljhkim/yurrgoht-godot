// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#pragma once

#include "constants.h"
#include "generated_texture.h"
#include "terrain_generator.h"
#include "terrain_generator_mesh_asset.h"
#include "terrain_generator_texture_asset.h"


class TerrainGenerator;
class TerrainGeneratorInstancer;

class TerrainGeneratorAssets : public Resource {
	GDCLASS(TerrainGeneratorAssets, Resource);
	CLASS_NAME();

public: // Constants
	enum AssetType {
		TYPE_TEXTURE,
		TYPE_MESH,
	};

	static inline const int MAX_TEXTURES = 32;
	static inline const int MAX_MESHES = 256;

private:
	TerrainGenerator *_terrain = nullptr;

	TypedArray<TerrainGeneratorTextureAsset> _texture_list;
	TypedArray<TerrainGeneratorMeshAsset> _mesh_list;

	GeneratedTexture _generated_albedo_textures;
	GeneratedTexture _generated_normal_textures;
	PackedColorArray _texture_colors;
	PackedFloat32Array _texture_normal_depths;
	PackedFloat32Array _texture_ao_strengths;
	PackedFloat32Array _texture_roughness_mods;
	PackedFloat32Array _texture_uv_scales;
	uint32_t _texture_vertical_projections;
	PackedVector2Array _texture_detiles;

	// Mesh Thumbnail Generation
	RID _scenario;
	RID _viewport;
	RID _viewport_texture;
	RID _camera;
	RID _key_light;
	RID _key_light_instance;
	RID _fill_light;
	RID _fill_light_instance;
	RID _mesh_instance;

	void _swap_ids(const AssetType p_type, const int p_src_id, const int p_dst_id);
	void _set_asset_list(const AssetType p_type, const TypedArray<TerrainGeneratorAssetResource> &p_list);
	void _set_asset(const AssetType p_type, const int p_id, const Ref<TerrainGeneratorAssetResource> &p_asset);

	void _update_texture_files();
	void _update_texture_settings();
	void _setup_thumbnail_creation();
	void _update_thumbnail(const Ref<TerrainGeneratorMeshAsset> &p_mesh_asset);

public:
	TerrainGeneratorAssets() {}
	~TerrainGeneratorAssets() { destroy(); }
	void initialize(TerrainGenerator *p_terrain);
	bool is_initialized() { return _terrain != nullptr; }
	void uninitialize();
	void destroy();

	void set_texture(const int p_id, const Ref<TerrainGeneratorTextureAsset> &p_texture);
	Ref<TerrainGeneratorTextureAsset> get_texture(const int p_id) const;
	void set_texture_list(const TypedArray<TerrainGeneratorTextureAsset> &p_texture_list);
	TypedArray<TerrainGeneratorTextureAsset> get_texture_list() const { return _texture_list; }
	int get_texture_count() const { return _texture_list.size(); }
	RID get_albedo_array_rid() const { return _generated_albedo_textures.get_rid(); }
	RID get_normal_array_rid() const { return _generated_normal_textures.get_rid(); }
	PackedColorArray get_texture_colors() const { return _texture_colors; }
	PackedFloat32Array get_texture_normal_depths() const { return _texture_normal_depths; }
	PackedFloat32Array get_texture_ao_strengths() const { return _texture_ao_strengths; }
	PackedFloat32Array get_texture_roughness_mods() const { return _texture_roughness_mods; }
	PackedFloat32Array get_texture_uv_scales() const { return _texture_uv_scales; }
	uint32_t get_texture_vertical_projections() const { return _texture_vertical_projections; }
	PackedVector2Array get_texture_detiles() const { return _texture_detiles; }

	void clear_textures(const bool p_update = false);
	void update_texture_list();
	void set_mesh_asset(const int p_id, const Ref<TerrainGeneratorMeshAsset> &p_mesh_asset);
	Ref<TerrainGeneratorMeshAsset> get_mesh_asset(const int p_id) const;
	void set_mesh_list(const TypedArray<TerrainGeneratorMeshAsset> &p_mesh_list);
	TypedArray<TerrainGeneratorMeshAsset> get_mesh_list() const { return _mesh_list; }
	int get_mesh_count() const { return _mesh_list.size(); }
	void create_mesh_thumbnails(const int p_id = -1, const Vector2i &p_size = Vector2i(128, 128));
	void update_mesh_list();

	Error save(const String &p_path = "");

protected:
	static void _bind_methods();
};

VARIANT_ENUM_CAST(TerrainGeneratorAssets::AssetType);

inline Ref<TerrainGeneratorTextureAsset> TerrainGeneratorAssets::get_texture(const int p_id) const {
	if (p_id >= 0 && p_id < _texture_list.size()) {
		return _texture_list[p_id];
	} else {
		return Ref<TerrainGeneratorTextureAsset>();
	}
}
