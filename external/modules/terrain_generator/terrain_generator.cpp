#include "terrain_generator.h"

/*
CREDITs: 
-> general structure borrowed from 'Voxel Game' demo https://github.com/godotengine/godot-demo-projects/tree/4.2-31d1c0c/3d/voxel
*/


TerrainGenerator::TerrainGenerator() {
	set_process(true);
	reuse_pool.resize(6);	// 2**6 = 64

	// maximum possible number of chunks instantiated
	MAX_CHUNKS_NUM = pow((render_distance * 2) + 1, 2);
}

// cleans up anything that the main scene tree might not
TerrainGenerator::~TerrainGenerator() {
	chunks.clear();
	set_process(false);
}

/*
allows C++ classes to use the same ready/process signals as in gdscript
*/
void TerrainGenerator::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			_process(get_process_delta_time());
			break;
		
		case NOTIFICATION_READY:
			_ready();
			break;

		/*
		case NOTIFICATION_EXIT_TREE: {
			chunks.clear();
			set_process(false);
		} 
		break;
		*/
	}
}

void TerrainGenerator::_ready() {
	world_scenario = get_world_3d()->get_scenario();

	// debug
	start = std::chrono::high_resolution_clock::now();
}

void TerrainGenerator::_process(double delta) {
	if (player_character == nullptr) return;

	main_thread_manager.process_tasks();		// setup by worker thread
	
	Vector3 player_chunk = (player_character->get_global_position() / Chunk::CHUNK_SIZE).round();
	player_chunk.y = 0.f;
	
	if (_deleting || player_chunk != old_player_chunk) {
		delete_far_away_chunks(player_chunk);
		_generating = true;
	}
	if (!_generating && !_final_update) return;

	//debug
	++frames;

	// Try to generate chunks ahead of time based on where the player is moving.
	//player_chunk.y += round(CLAMP(player_character->get_velocity().y, -render_distance/4, render_distance/4));

	// priority based chunk spawning -> spawn chunks according to player view direction
	// for now, only considers 4 directions (diagonals) -> considering 8 directions in the future
	float yaw = player_character->get_global_rotation_degrees().y;
	bool x_flip = yaw < 0.f && yaw >= -180.f;
	bool z_flip = yaw > 90.f || yaw <= -90.f;
	range_flip x_range(-effective_render_distance, effective_render_distance, x_flip);
	range_flip z_range(-effective_render_distance, effective_render_distance, z_flip);

	print_line("PLAYER VIEW DIRECTION: ", player_character->get_global_rotation_degrees());
	print_line("SPAWN PARAMETERS: ", x_flip, z_flip);

	// Check existing chunks within range. If it doesn't exist, create it.
	for (int x : x_range) {
		for (int z : z_range) {
			Vector3 grid_pos = Vector3(x, 0, z);
			Vector3 chunk_pos = player_chunk + grid_pos;
			++count;	// debug

			StringName task_name = String(chunk_pos);
			bool task_exists = task_thread_manager.has_task(task_name);
			float distance = player_chunk.distance_to(chunk_pos);

			// UPDATE EXISTING CHUNK
			if (distance <= render_distance &&
				chunks.has(chunk_pos) &&
				!chunks[chunk_pos]->get_flag(Chunk::FLAG::DELETE) &&
				!chunks[chunk_pos]->get_flag(Chunk::FLAG::UPDATE) &&
				!task_exists &&
				!main_thread_manager.task_exists(task_name) &&
				chunks[chunk_pos]->grid_distance(grid_pos) > 2
			) {
				print_line("UPDATE CHUNK: ", task_name);
				
				chunks[chunk_pos]->set_flag(Chunk::FLAG::UPDATE, true);
				task_thread_manager.insert_task(
					task_name, callable_mp(this, &TerrainGenerator::update_chunk_mesh).bind(chunks[chunk_pos], distance, grid_pos)
				);
				continue;
			}

			// CREATE NEW CHUNK
			if (task_exists ||
				chunks.has(chunk_pos) ||
				main_thread_manager.task_exists(task_name) ||
				distance > render_distance ||
				_final_update
			) {
				continue;
			}

			task_thread_manager.insert_task(
				task_name, callable_mp(this, &TerrainGenerator::instantiate_chunk).bind(chunk_pos, MAX(abs(x), abs(z)), grid_pos)
			);
			return;
		}
	}

	// We can move on to the next stage by increasing the effective distance.
	if (effective_render_distance < render_distance) {
		effective_render_distance += 1;
	}
	else if (!_final_update) {	// check for updates one more time -> allows chunks to catch up to player
		_final_update = true;
		_generating = false;	// Effective render distance is maxed out, done generating.
	}
	else {
		_final_update = false;
		effective_render_distance = 0;	// reset effective render_distance

		/*
		DEBUG
		- Measure length of generation time
			- unreliable since it counts non-generation times as well
			- just check times while player is walking for rough estimates, should be accurate enough
		- Measure number of chunk coordinates checked
		- Measure number of frames the generation takes place in
		*/
    	std::chrono::_V2::system_clock::time_point end = std::chrono::high_resolution_clock::now();
		long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

		print_line("Duration (milliseconds) (unreliable), For-loop iterations, Generation Frames count: ", duration, count, frames);
		print_line("NUMBER OF CHUNKS ", chunk_count);
		count = 0;
		frames = 0;
	}
}


/*
* Computations that can safely be done outside of main thread -> instantiating/generating the chunk
* should only be called inside _thread_process -> purely because it can be slow
*/
void TerrainGenerator::instantiate_chunk(Vector3 chunk_pos, int lod_factor, Vector3 grid_pos) {
	bool data_check;
	{
		MutexLock mutex_lock(reuse_mutex);
		data_check = reuse_pool.data_left() > 0;
	}
	Ref<Chunk> chunk;
	if (data_check) {
		print_line("REUSE CHUNK: ", chunk_pos);
        {
			MutexLock mutex_lock(reuse_mutex);
			chunk = reuse_pool.read();
		}
		chunk->setup_reuse(lod_factor, chunk_pos, grid_pos);
	}
	else if (chunk_count < MAX_CHUNKS_NUM) {
		print_line("NEW CHUNK: ", chunk_pos);
		++chunk_count;
		chunk.instantiate(world_scenario, lod_factor, chunk_pos, grid_pos);
	}
	else return;		// exit (does not normally activate, but added for safety gaurentees)
	
	main_thread_manager.tasks_push_back(String(chunk_pos), callable_mp(this, &TerrainGenerator::add_chunk).bind(chunk_pos, chunk));
	chunk.unref();
}

/*
* add chunk reference to master chunk list
* done in main thread to avoid worker threads accessing master chunk list
*/
void TerrainGenerator::add_chunk(Vector3 chunk_pos, Ref<Chunk> chunk) {
	chunks[chunk_pos] = chunk;
	chunk.unref();
}

/*
* update chunk mesh according to new LOD factor
*/
void TerrainGenerator::update_chunk_mesh(Ref<Chunk> chunk, int lod_factor, Vector3 grid_pos) {
	chunk->setup_update(lod_factor, grid_pos);
	chunk.unref();
}

/*
* if there is space left in reuse_pool, add chunk for reuse, and remove from master list
* reset chunk data to remove rendered gpu instance
* erase Ref from master list before data reset -> reset_data() also resets flags, some of which may still be checked

- can probably delegate this to another thread
- TODO later
*/
void TerrainGenerator::delete_far_away_chunks(Vector3 player_chunk) {
	old_player_chunk = player_chunk;
	// If we need to delete chunks, give the new chunk system a chance to catch up.
	effective_render_distance = std::max(1, effective_render_distance-1);

	int deleted_this_frame = 0;
	// We should delete old chunks more aggressively if moving fast.
	// An easy way to calculate this is by using the effective render distance.
	// The specific values in this formula are arbitrary and from experimentation.
	//int max_deletions = CLAMP(2 * (render_distance - effective_render_distance), 2, 64);
	int max_deletions = 1000;

	// Also take the opportunity to delete far away chunks.
	for (KeyValue<Vector3, Ref<Chunk>> chunk : chunks) {
		if (player_chunk.distance_to(chunk.key) >= delete_distance &&
			!chunk.value->get_flag(Chunk::FLAG::DELETE) &&
			!chunk.value->get_flag(Chunk::FLAG::UPDATE)
		) {
			chunk.value->set_flag(Chunk::FLAG::DELETE, true);
			bool space_check;
			{
				MutexLock mutex_lock(reuse_mutex);
				space_check = reuse_pool.space_left() > 0;
			}

			// delete if theres no space left in reuse pool, or too many chunks spawned
			if (!space_check || chunk_count >= MAX_CHUNKS_NUM) {
				print_line("DELETE CHUNK: ", chunk.key);
				chunks.erase(chunk.key);
				--chunk_count;
				return;
			}
			print_line("REUSE CHUNK: ", chunk.key);
			Ref<Chunk> chunk_ref(chunk.value);
			chunk.value.unref();		// erase calls destructor -> unref now to avoid true memory delete
			chunks.erase(chunk.key);
			chunk_ref->reset_data();	// clears render instance
			{
				MutexLock mutex_lock(reuse_mutex);
				reuse_pool.write(chunk_ref);
			}
			chunk_ref.unref();			// IMPORTANT -> memory safety
			deleted_this_frame++;

			// Limit the amount of deletions per frame to avoid lag spikes -> Continue deleting next frame.
			if (deleted_this_frame > max_deletions) {
				_deleting = true;
				return;
			}
		}
	}
	
	// We're done deleting.
	_deleting = false;
}


/*
- standard editor only exports 
- we only use the player character for position (for now)
*/
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