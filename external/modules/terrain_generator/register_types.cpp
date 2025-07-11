// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "core/object/class_db.h"

#include "register_types.h"
#include "terrain_generator.h"
#include "terrain_generator_editor.h"

void initialize_terrain_generator_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<Terrain3D>();
	ClassDB::register_class<Terrain3DAssets>();
	ClassDB::register_class<Terrain3DData>();
	ClassDB::register_class<Terrain3DEditor>();
	ClassDB::register_class<Terrain3DCollision>();
	ClassDB::register_class<Terrain3DInstancer>();
	ClassDB::register_class<Terrain3DMaterial>();
	ClassDB::register_class<Terrain3DMeshAsset>();
	ClassDB::register_class<Terrain3DRegion>();
	ClassDB::register_class<Terrain3DTextureAsset>();
	ClassDB::register_class<Terrain3DUtil>();
}

void uninitialize_terrain_generator_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
