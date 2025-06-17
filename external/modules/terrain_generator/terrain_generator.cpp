#include "terrain_generator.h"

void TerrainGenerator::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			process(get_process_delta_time());
			break;

		/*
		case NOTIFICATION_READY:
			ready();
			break;
		*/
	}
}

// processes that only need to be done on intialization - generates the basic values needed going forward
/*
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
*/

void TerrainGenerator::process(double delta) {
	if (player_character == nullptr) return;

	//Vector3 player_location = player_character->get_global_position().snapped(Vector3(1.f, 1.f, 1.f) * 64.f) * Vector3(1.f, 0.f, 1.f);
	Vector3i player_chunk = Vector3i((player_character->get_global_position() / Chunk::CHUNK_SIZE).round());
	
	if (_deleting || (Vector2(player_chunk.x,player_chunk.z) != Vector2(_old_player_chunk.x,_old_player_chunk.z))) {
		_delete_far_away_chunks(player_chunk);
		_generating = true;
	}
	if (!_generating) return;

	// Try to generate chunks ahead of time based on where the player is moving.
	player_chunk.y += round(CLAMP(player_character->get_velocity().y, -render_distance / 4, render_distance / 4));

	// Check existing chunks within range. If it doesn't exist, create it.
	for (int x = (player_chunk.x - effective_render_distance); x <= (player_chunk.x + effective_render_distance); x++) {
		for (int z = (player_chunk.z - effective_render_distance); z <= (player_chunk.z + effective_render_distance); z++) {
			Vector3i chunk_position = Vector3i(x, 0, z);
			Vector2 grid_position(x, z);

			if (Vector2(player_chunk.x, player_chunk.z).distance_to(grid_position) > render_distance)
				continue;
			
			if (_chunks.has(grid_position))
				continue;

			Chunk* chunk = memnew(Chunk);
			chunk->chunk_position = chunk_position;
			add_child(chunk);

			print_line("ADDING TO HASHMAP");
			_chunks[grid_position] = chunk->get_index();
			print_line("FINISHED ADDING TO HASHMAP");

			return;
		}
	}

	if (effective_render_distance < render_distance) {
		// We can move on to the next stage by increasing the effective distance.
		effective_render_distance += 1;
	}
	else {
		// Effective render distance is maxed out, done generating.
		_generating = false;
	}
}

void TerrainGenerator::clean_up() {
	for (KeyValue<Vector2i, int32_t> chunk : _chunks) {
		Ref<core_bind::Thread> thread = Object::cast_to<Chunk>(get_child(chunk.value))->get_thread();
		if (thread != nullptr)
			thread->wait_to_finish();
	}
	
	_chunks.clear();
	set_process(false);

	/*
	- I am hoping this code here is able to be a good enough replacement to the gdscript version:
	
	for c in get_children():
		c.free()

	- .free() is a gdscript exclusive function
	*/
	for (int c = 0; c < get_child_count(); c++) {
		Node* child = get_child(c);
		remove_child(child);
		child->queue_free();
		//child.unref();
    }
}


void TerrainGenerator::_delete_far_away_chunks(Vector3i player_chunk) {
	_old_player_chunk = player_chunk;
	// If we need to delete chunks, give the new chunk system a chance to catch up.
	effective_render_distance = std::max(1, effective_render_distance - 1);

	int deleted_this_frame = 0;
	// We should delete old chunks more aggressively if moving fast.
	// An easy way to calculate this is by using the effective render distance.
	// The specific values in this formula are arbitrary and from experimentation.
	int max_deletions = CLAMP(2 * (render_distance - effective_render_distance), 2, 8);

	// Also take the opportunity to delete far away chunks.
	for (KeyValue<Vector2i, int32_t> chunk : _chunks) {
		if (Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(chunk.key)) > _delete_distance) {
			Chunk* chunk_value = Object::cast_to<Chunk>(get_child(chunk.value));
			
			Ref<core_bind::Thread> thread = chunk_value->get_thread();
			if (thread != nullptr) {
				thread->wait_to_finish();
			}
			
			chunk_value->queue_free();
			_chunks.erase(chunk.key);
			deleted_this_frame += 1;

			// Limit the amount of deletions per frame to avoid lag spikes.
			if (deleted_this_frame > max_deletions) {
				_deleting = true;	// Continue deleting next frame.
				return;
			}
		}
	}

	// We're done deleting.
	_deleting = false;
}

void TerrainGenerator::set_player_character(CharacterBody3D* p_node) {
	if (p_node == nullptr) return;

	player_character = p_node;
}

CharacterBody3D* TerrainGenerator::get_player_character() const {
	return player_character;
}

void TerrainGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_player_character", "p_node"), &TerrainGenerator::set_player_character);
	ClassDB::bind_method(D_METHOD("get_player_character"), &TerrainGenerator::get_player_character);
	
	ClassDB::add_property("TerrainGenerator", PropertyInfo(Variant::OBJECT, "player_character", PROPERTY_HINT_NODE_TYPE, "CharacterBody3D"), "set_player_character", "get_player_character");
}


// constructor - sets up default values
TerrainGenerator::TerrainGenerator() {
	//count = 0;

	// enable process
	set_process(true);
}