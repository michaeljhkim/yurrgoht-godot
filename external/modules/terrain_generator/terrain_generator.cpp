#include "terrain_generator.h"

void TerrainGenerator::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			process(get_process_delta_time());
			break;

		case NOTIFICATION_READY:
			ready();
			break;

		case NOTIFICATION_EXIT_TREE: { 		// Thread must be disposed (or "joined"), for portability.
			//clean_up();
		} break;
	}
}

// processes that only need to be done on intialization - generates the basic values needed going forward
void TerrainGenerator::ready() {
	//shared_mat.instantiate();
}

/*
CURRENT ISSUES:
- New mesh chunks generate significantly further away that expected
- stitching between chunks is non-existant, but can probably fix with adding 1 row/column of vertices to each side of each chunk
- then remove them after calculating normals
*/
void TerrainGenerator::process(double delta) {
	if (player_character == nullptr) return;

	//Vector3 player_location = player_character->get_global_position().snapped(Vector3(1.f, 1.f, 1.f) * 64.f) * Vector3(1.f, 0.f, 1.f);
	//Vector3i player_chunk = Vector3i((player_character->get_global_position() / Chunk::CHUNK_SIZE).round());
	Vector3i player_chunk = player_character->get_global_position().snapped(Vector3i(1, 1, 1) * Chunk::CHUNK_SIZE) * Vector3i(1, 0, 1);
	
	if (_deleting || (Vector2(player_chunk.x,player_chunk.z) != Vector2(_old_player_chunk.x,_old_player_chunk.z))) {
		_delete_far_away_chunks(player_chunk);
		_generating = true;
	}
	if (!_generating) return;

	// Try to generate chunks ahead of time based on where the player is moving.
	//player_chunk.y += round(CLAMP(player_character->get_velocity().y, -render_distance / 4, render_distance / 4));

	// Check existing chunks within range. If it doesn't exist, create it.
	for (int x = (player_chunk.x - effective_render_distance); x <= (player_chunk.x + effective_render_distance); x++) {
		for (int z = (player_chunk.z - effective_render_distance); z <= (player_chunk.z + effective_render_distance); z++) {
			Vector3i chunk_position = Vector3i(x, 0, z);
			Vector2i grid_position(x, z);

			if ((Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(grid_position)) > render_distance) || 
				_chunks.has(grid_position))
				continue;

			Chunk* chunk = memnew(Chunk);
			chunk->chunk_position = chunk_position;
			//chunk->set_material(shared_mat);
			add_child(chunk, true);
			_chunks[grid_position] = chunk->get_path();

			//print_line("chunk name: " + chunk->get_path());

			return;
		}
	}

	// We can move on to the next stage by increasing the effective distance.
	if (effective_render_distance < render_distance) {
		effective_render_distance += 1;
	}
	else {
		// Effective render distance is maxed out, done generating.
		_generating = false;
	}
}

void TerrainGenerator::clean_up() {
	/*
	for (KeyValue<Vector2i, int32_t> chunk : _chunks) {
		Ref<core_bind::Thread> thread = Object::cast_to<Chunk>(get_child(chunk.value))->get_thread();
		if (thread != nullptr)
			thread->wait_to_finish();

		thread.unref();
	}
	*/
	
	_chunks.clear();
	set_process(false);

	for (int c = 0; c < get_child_count(); c++) {
		//remove_child(child);
		get_child(c)->queue_free();
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
	// remove_child

	const Array key_list = _chunks.keys();
	
	for (const Vector2i chunk_key : key_list) {
		if (Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(chunk_key)) > _delete_distance) {
			Node* child = get_node(_chunks[chunk_key]);

			// No longer need this, but this is just in case
			if (child == nullptr) {
				NodePath name = _chunks[chunk_key];
				print_line("NULL CHILD: " + name);
				continue;
			} 

			child->queue_free();
			_chunks.erase(chunk_key);

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