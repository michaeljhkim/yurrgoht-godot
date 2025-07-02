#include "terrain_generator.h"

#define ENABLE 1
#define TERRAIN_DEBUG(m_msg, debug_value) 	\
	if (ENABLE) {							\
		print_line(m_msg, debug_value); 	\
	}										\


/*
CREDITs: 
- general structure borrowed from 'Voxel Game' demo https://github.com/godotengine/godot-demo-projects/tree/4.2-31d1c0c/3d/voxel

NOTES:
- remember that instantiating a node is usually safe on another thread, but ONLY if it is outside of the main scene tree
- basically, only touch nodes in the main scene tree ONLY IF on the main thread

- notes above may not be relavant since mesh generation system no longer uses nodes
*/


// constructor - sets up default values
TerrainGenerator::TerrainGenerator() {
	set_process(true);

	// reserve now instead of later
	_reuse_pool.resize(6);	// 2**6 = 64

	/*
	- maximum possible number of chunks instantiated is ((_render_distance * 2) + 1) ** 2
	- e.g, ((5 * 2) + 1) ** 2 = 121 chunks possible (but unlikely)
	*/
	MAX_CHUNKS_NUM = pow((_render_distance * 2) + 1, 2);
}

// destructor - cleans up anything that the main scene tree might not
TerrainGenerator::~TerrainGenerator() {
	_reuse_mutex.unref();
	clean_up();
}

/*
- allows C++ classes to use the same ready/process signals as in gdscript
- GDExtension has this built in
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
		// Thread must be disposed (or "joined"), for portability.
		case NOTIFICATION_EXIT_TREE: { } 
		break;
		*/
	}
}


// called only in destructor
void TerrainGenerator::clean_up() {
	_chunks.clear();
	set_process(false);
}



// processes that only need to be done on intialization - mutexes, thread
void TerrainGenerator::_ready() {
	_world_scenario = get_world_3d()->get_scenario();	// thread has not started yet, so this is safe
	_reuse_mutex.instantiate();

	// debug
	start = std::chrono::high_resolution_clock::now();
}


/*
- need to implement a system to make updates/deletes aggressively based on player viewing direction 
*/
void TerrainGenerator::_process(double delta) {
	if (player_character == nullptr) return;

	// main thread process tasks - setup by worker thread
	_main_thread_manager.main_thread_process();
	// finished tasks, continue
	
	Vector3 player_chunk = (player_character->get_global_position() / Chunk::CHUNK_SIZE).round();
	player_chunk.y = 0.f;
	
	// goes from PI to -PI
	//player_character->get_global_rotation();	// reference for being able to render chunks according to the direction that the player is facing
	//print_line("PLAYER VIEW DIRECTION: ", player_character->get_global_rotation());

	if (_deleting || (player_chunk != _old_player_chunk)) {
		_delete_far_away_chunks(player_chunk);
		_generating = true;
	}
	if (!_generating) return;

	//debug
	++frames;

	// Try to generate chunks ahead of time based on where the player is moving. - not sure what this does (study later, low priority)
	//player_chunk.y += round(CLAMP(player_character->get_velocity().y, -_render_distance/4, _render_distance/4));

	/*
	- for the most part, this nested for-loop generates chunks around the player from inner circle to outer circle
	- entire grid is scanned every iteration during generation in order to gaurentee no chunks were missed
	- could probably take advantage of this and use the player direction to determine where to start rendering first
	*/

	// Check existing chunks within range. If it doesn't exist, create it.
	for (int x = -_effective_render_distance; x <= _effective_render_distance; x++) {
		for (int z = -_effective_render_distance; z <= _effective_render_distance; z++) {
			Vector3 grid_pos = Vector3(x, 0, z);
			Vector3 chunk_pos = player_chunk + grid_pos;
			++count;	// debug number of grid coordinates checked

			StringName task_name = String(chunk_pos);
			float distance = player_chunk.distance_to(chunk_pos);

			// we check _thread_task_queue first, since if true, we want to unlock the mutex asap
			bool task_exists = _task_thread_manager.has_task(task_name);

			// UPDATE EXISTING CHUNK
			if ( distance <= _render_distance &&
				_chunks.has(chunk_pos) &&
				!_chunks[chunk_pos]->get_flag(Chunk::FLAG::DELETE) &&
				!_chunks[chunk_pos]->get_flag(Chunk::FLAG::UPDATE) &&
				!task_exists &&
				!_main_thread_manager.task_exists(task_name) &&
				_chunks[chunk_pos]->get_grid_position().distance_to(grid_pos) > 2
			) {
				_chunks[chunk_pos]->set_flag(Chunk::FLAG::UPDATE, true);
				TERRAIN_DEBUG("UPDATE CHUNK: ", task_name);

				_task_thread_manager.insert_task(
					task_name, callable_mp(this, &TerrainGenerator::_update_chunk_mesh).bind(_chunks[chunk_pos], distance, grid_pos)
				);

				continue;
			}

			// CREATE NEW CHUNK -> checks if chunk exists anywhere first
			else if (
				task_exists ||
				_chunks.has(chunk_pos) ||
				_main_thread_manager.task_exists(task_name) ||
				distance > _render_distance
			) {
				continue;
			}

			// direct callable instantiation
			_task_thread_manager.insert_task(
				task_name, callable_mp(this, &TerrainGenerator::_instantiate_chunk).bind(chunk_pos, MAX(abs(x), abs(z)), grid_pos)
			);

			return;
		}
	}

	// We can move on to the next stage by increasing the effective distance.
	if (_effective_render_distance < _render_distance) {
		_effective_render_distance += 1;
	}
	else {
		// Effective render distance is maxed out, done generating.
		_generating = false;

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
		//start = end;

		print_line("Duration (milliseconds) (unreliable), For-loop iterations, Generation Frames count: ", duration, count, frames);
		TERRAIN_DEBUG("NUMBER OF CHUNKS ", chunk_count);
		count = 0;
		frames = 0;
	}
}


/*
* Computations that can safely be done outside of main thread -> instantiating/generating the chunk
* should only be called inside _thread_process -> purely because it can be slow
*/
void TerrainGenerator::_instantiate_chunk(Vector3 chunk_pos, int lod_factor, Vector3 grid_pos) {
	Ref<Chunk> chunk;
	_reuse_mutex->lock();
	bool data_check = _reuse_pool.data_left() > 0;
	_reuse_mutex->unlock();

	// check if the reuse pool has chunks we can recycle
	if (data_check) {
		TERRAIN_DEBUG("REUSE INSTANTIATED CHUNK: ", chunk_pos);

		_reuse_mutex->lock();
		chunk = _reuse_pool.read();
		_reuse_mutex->unlock();

		// reset data again -> in case it didnt take the first time
		chunk->reset_data();
		chunk->set_position(chunk_pos);
		chunk->set_LOD_factor(lod_factor);
	}
	else if (chunk_count < MAX_CHUNKS_NUM) {
		TERRAIN_DEBUG("INSTANTIATE NEW CHUNK: ", chunk_pos);
		++chunk_count;
		chunk.instantiate(_world_scenario, chunk_pos, lod_factor);		// this is the only other time _world_scenario is used, so it's thread safe
	}
	else return;		// neither generation conditions activated -> exit function (does not normally activate, but added for safety gaurentees)
		
	chunk->set_grid_position(grid_pos);
	chunk->_generate_mesh();
	_main_thread_manager.tasks_push_back(String(chunk_pos), callable_mp(this, &TerrainGenerator::_add_chunk).bind(chunk_pos, chunk));

	chunk.unref(); 		// unreference for safety
}

/*
* add chunk reference to master chunk list
* done in main thread to avoid worker thread accessing master chunk list
*/
void TerrainGenerator::_add_chunk(Vector3 chunk_pos, Ref<Chunk> chunk) {
	_chunks[chunk_pos] = chunk;

	chunk.unref(); 		// unreference for safety
}

/*
* add chunk reference to master chunk list
*/
void TerrainGenerator::_update_chunk_mesh(Ref<Chunk> chunk, int lod_factor, Vector3 grid_pos) {
	// reset chunk mesh before anything else
	chunk->clear_data();

	chunk->set_LOD_factor(lod_factor);
	chunk->set_grid_position(grid_pos);
	chunk->_generate_mesh();
	chunk->set_flag(Chunk::FLAG::UPDATE, false);

	//TERRAIN_DEBUG("UPDATE FINISHED: ", chunk->_get_chunk_pos());
	chunk.unref(); 		// unreference for safety
}

/*
* if there is space left in _reuse_pool, add chunk for reuse, and unref the chunk in the master list
* reset chunk data to remove rendered instance
* erase Ref from master list before data reset -> reset_data() also resets flags, some of which may still be checked
*/
void TerrainGenerator::_delete_chunk(Vector3 chunk_key) {
	TERRAIN_DEBUG("DELETE CHUNK: ", chunk_key);

	_reuse_mutex->lock();
	bool space_check = _reuse_pool.space_left() > 0;
	_reuse_mutex->unlock();

	// delete if theres no space left in reuse pool, or too many chunks spawned
	if (!space_check || (chunk_count >= MAX_CHUNKS_NUM)) {
		TERRAIN_DEBUG("TRUE DELETE -> DESTRUCTOR: ", chunk_key);
		_chunks.erase(chunk_key);
		--chunk_count;
		return;
	}
	TERRAIN_DEBUG("WE CAN REUSE CHUNK: ", chunk_key);
	Ref<Chunk> reuse_chunk(_chunks[chunk_key]);

	_chunks[chunk_key].unref();		// erase calls destructor -> unref now to avoid true memory delete
	_chunks.erase(chunk_key);
	reuse_chunk->reset_data();

	_reuse_mutex->lock();
	_reuse_pool.write(reuse_chunk);
	_reuse_mutex->unlock();

	TERRAIN_DEBUG("CHUNK REF COUNT: ", reuse_chunk->get_reference_count());
	reuse_chunk.unref();		// remove reference
}

/*
- can probably delegate this to another thread
- TODO later
*/

void TerrainGenerator::_delete_far_away_chunks(Vector3 player_chunk) {
	_old_player_chunk = player_chunk;
	// If we need to delete chunks, give the new chunk system a chance to catch up.
	_effective_render_distance = std::max(1, _effective_render_distance-1);

	int deleted_this_frame = 0;
	// We should delete old chunks more aggressively if moving fast.
	// An easy way to calculate this is by using the effective render distance.
	// The specific values in this formula are arbitrary and from experimentation.

	/*
	- if not adjusted correctly, some pieces may never get deleted, or just take way too long
	- consider reversing list every time we reach max deletes per frame?
	*/
	//int max_deletions = CLAMP(2 * (_render_distance - _effective_render_distance), 2, 64);
	int max_deletions = 1000;

	// Also take the opportunity to delete far away chunks.
	for (KeyValue<Vector3, Ref<Chunk>> chunk : _chunks) {
		StringName task_name = String(chunk.key);

		// flag check -> we do not want to tag the same chunk again
		if (!chunk.value->get_flag(Chunk::FLAG::DELETE) &&
			player_chunk.distance_to(chunk.key) >= _delete_distance &&
			!chunk.value->get_flag(Chunk::FLAG::UPDATE)
		) {
			chunk.value->set_flag(Chunk::FLAG::DELETE, true);
			_main_thread_manager.tasks_push_back(String(chunk.key), callable_mp(this, &TerrainGenerator::_delete_chunk).bind(chunk.key));
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