// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "core/object/class_db.h"

#include "register_types.h"
#include "terrain_generator.h"
#include "terrain_generator_editor.h"

void initialize_terrain_generator_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<TerrainGenerator>();
	ClassDB::register_class<TerrainGeneratorAssets>();
	ClassDB::register_class<TerrainGeneratorData>();
	ClassDB::register_class<TerrainGeneratorEditor>();
	ClassDB::register_class<TerrainGeneratorCollision>();
	ClassDB::register_class<TerrainGeneratorInstancer>();
	ClassDB::register_class<TerrainGeneratorMaterial>();
	ClassDB::register_class<TerrainGeneratorMeshAsset>();
	ClassDB::register_class<TerrainGeneratorRegion>();
	ClassDB::register_class<TerrainGeneratorTextureAsset>();
	ClassDB::register_class<TerrainGeneratorUtil>();
}

void uninitialize_terrain_generator_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
