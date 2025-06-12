#include "register_types.h"

#include "core/object/class_db.h"
#include "terrain_generator.h"

void initialize_terrain_generator_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<TerrainGenerator>();
}

void uninitialize_terrain_generator_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
   // Nothing to do here in this example.
}