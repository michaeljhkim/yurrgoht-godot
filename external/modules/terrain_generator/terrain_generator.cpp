#include "terrain_generator.h"

void TerrainGenerator::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			process(get_process_delta_time());
			break;

		case NOTIFICATION_READY:
			ready();
			break;
	}
}

// processes that only need to be done on intialization - generates the basic values needed going forward
void TerrainGenerator::ready() {
	partition_distance = 4;

	for(int8_t z = -partition_distance; z <= partition_distance; z++) {
		for(int8_t x = -partition_distance; x <= partition_distance; x++) {
			Partition* partition = memnew(Partition(Vector2i(x, z), x, z));
			terrain_grid.insert(Vector2i(x, z), partition);
			
			add_child(partition);
		}
	}
}

void TerrainGenerator::process(float delta) {
	if (player_character == nullptr) return;

	Vector3 player_location = player_character->get_global_position().snapped(Vector3(1.f, 1.f, 1.f) * 64.f) * Vector3(1.f, 0.f, 1.f);

	if (player_location != get_global_position()) {
		set_global_position(player_location);

		for(int8_t z = -partition_distance; z <= partition_distance; z++) {
			for(int8_t x = -partition_distance; x <= partition_distance; x++) {
				terrain_grid.get(Vector2i(x, z))->update_xz(player_location);
				terrain_grid.get(Vector2i(x, z))->generate_mesh();
			}
		}
	}
	else {
		set_global_position(player_location);
	}

}

void TerrainGenerator::set_player_character(Node3D* p_node) {
	if (p_node == nullptr) return;

	player_character = p_node;
}

Node3D* TerrainGenerator::get_player_character() const {
	return player_character;
}

// Functions that will be used in GDSCRIPT
void TerrainGenerator::_bind_methods() {
	//ClassDB::bind_method(D_METHOD("add", "value"), &TerrainGenerator::add);
	//ClassDB::bind_method(D_METHOD("reset"), &TerrainGenerator::reset);
	//ClassDB::bind_method(D_METHOD("get_total"), &TerrainGenerator::get_total);

	ClassDB::bind_method(D_METHOD("set_player_character", "p_node"), &TerrainGenerator::set_player_character);
	ClassDB::bind_method(D_METHOD("get_player_character"), &TerrainGenerator::get_player_character);
	
	ClassDB::add_property("TerrainGenerator", PropertyInfo(Variant::OBJECT, "player_character", PROPERTY_HINT_NODE_TYPE, "Node3D"), "set_player_character", "get_player_character");
}


// constructor - sets up default values
TerrainGenerator::TerrainGenerator() {
	//count = 0;
	partition_distance = 4;

	// enable process
	set_process(true);
}