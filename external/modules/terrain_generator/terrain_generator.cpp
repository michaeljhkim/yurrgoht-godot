#include "terrain_generator.h"

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
	callable_queue.set_capacity(64);
}

// destructor - make sure to clean up anything that the main scene tree might not
TerrainGenerator::~TerrainGenerator() {
	// worker thread will usually be active up until destruction
	if (_thread.is_valid()) {
		THREAD_RUNNING.store(false, std::memory_order_acquire);
		_thread->wait_to_finish();	// Wait until it exits.
	}
	
	_thread.unref();
	_mutex.unref();

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

	//_thread_task_queue.clear();
	callable_queue.clear();
}



// processes that only need to be done on intialization - mutexes, thread
void TerrainGenerator::_ready() {
	world_scenario = get_world_3d()->get_scenario();	// thread has not started yet, so this is safe

	_mutex.instantiate();
	_thread.instantiate();

	_thread->start(callable_mp(this, &TerrainGenerator::_thread_process), CoreBind::Thread::PRIORITY_NORMAL);
	//_old_player_chunk = (player_character->get_global_position() / Chunk::CHUNK_SIZE).round();

	// generate all possible LODs into a lookup table
	// after generation, only the task thread will be looking at the table, so no mutexes needed
	for (int x = -render_distance; x <= render_distance; x++) {
		for (int z = -render_distance; z <= render_distance; z++) {
			Vector3 grid_position = Vector3(x, 0, z);
			float distance = Vector3(0, 0, 0).distance_to(grid_position);

			/*
			if (distance > render_distance) {
				continue;
			}
			*/
			_LOD_table.insert(String(grid_position), distance);
			//print_line("distance: ", distance);
		}
	}

	//debug
	start = std::chrono::high_resolution_clock::now();
}



void TerrainGenerator::_process(double delta) {
	if (player_character == nullptr) return;

	// main thread process tasks - setup by worker thread
	task_buffer_manager.main_thread_process();
	
	Vector3 player_chunk = (player_character->get_global_position() / Chunk::CHUNK_SIZE).round();
	player_chunk.y = 0.f;
	//player_character->get_global_rotation();	// reference for being able to render chunks according to the direction that the player is facing

	if (_deleting || (player_chunk != _old_player_chunk)) {
		_delete_far_away_chunks(player_chunk);
		_generating = true;
	}
	if (!_generating) return;

	//debug
	++frames;

	// Try to generate chunks ahead of time based on where the player is moving. - not sure what this does (study later, low priority)
	//player_chunk.y += round(CLAMP(player_character->get_velocity().y, -render_distance/4, render_distance/4));

	/*
	- for the most part, this nested for-loop generates chunks around the player from inner circle to outer circle
	- entire grid is scanned every iteration during generation in order to gaurentee no chunks were missed
	- could probably take advantage of this and use the player direction to determine where to start rendering first
	*/

	// Check existing chunks within range. If it doesn't exist, create it.
	for (int x = -effective_render_distance; x <= effective_render_distance; x++) {
		for (int z = -effective_render_distance; z <= effective_render_distance; z++) {
			Vector3 grid_position = Vector3(x, 0, z);
			Vector3 chunk_position = player_chunk + grid_position;
			++count;	// debug number of grid coordinates checked

			StringName task_name = String(chunk_position);
			
			float distance = player_chunk.distance_to(chunk_position);
			//float chunk_distance = _LOD_table[task_name];

			// we check _thread_task_queue first, since if true, we want to unlock the mutex asap
			_mutex->lock();
			bool task_exists = callable_queue.has(task_name);
			_mutex->unlock();

			// check if we should update the chunk if it exists
			if (_chunks.has(chunk_position) && 
				!task_exists &&
				!_chunks[chunk_position]->get_flag(Chunk::FLAG::DELETE) &&
				!_chunks[chunk_position]->get_flag(Chunk::FLAG::UPDATE) &&
				std::abs(distance - _chunks[chunk_position]->_get_chunk_LOD()) >= 1
			) {
				_chunks[chunk_position]->set_flag(Chunk::FLAG::UPDATE, true);
				//print_line("UPDATE chunk: ", chunk_position);
				_mutex->lock();
				callable_queue.insert(task_name, callable_mp(this, &TerrainGenerator::_update_chunk_mesh).bind(_chunks[chunk_position], distance));
				_mutex->unlock();
				continue;
			}

			// checks if chunk exists 
			if (task_exists ||
				_chunks.has(chunk_position) ||
				task_buffer_manager.task_exists(task_name) ||
				distance > render_distance
			) {
				continue;
			}
			//print_line("ADD chunk: ", chunk_position);

			// direct callable instantiation
			_mutex->lock();
			callable_queue.insert(
				task_name, 
				callable_mp(this, &TerrainGenerator::_instantiate_chunk).bind(chunk_position, MAX(abs(x), abs(z))) 
			);
			_mutex->unlock();

			// alert task thread that the task queue is no longer empty
			QUEUE_EMPTY.store(false, std::memory_order_acquire);

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

		/*
		DEBUG
		- Measure length of generation time
		- Measure number of chunk coordinates checked
		- Measure number of frames the generation takes place in
		*/
    	std::chrono::_V2::system_clock::time_point end = std::chrono::high_resolution_clock::now();
		long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

		print_line("Game start to generation calculations finished in milliseconds: ", duration);
		print_line("Cycle count: ", count);
		print_line("Generating frames count: ", frames);
		count = 0;
		frames = 0;
	}
}


/*
- worker thread function

- for the most part, this is an infinite loop, until the class itself is down
*/
void TerrainGenerator::_thread_process() {
	Callable task;

	while (THREAD_RUNNING) {
		if (QUEUE_EMPTY.load()) {
			continue;
		}
		_mutex->lock();
		task = callable_queue.front();			// tasks do exist, copy the front of the queue
		_mutex->unlock();
		
		task.call();	// all arguements are bound (with .bind) BEFORE being added to task queue, so no arguements needed here
			
		// only safe to remove AFTER calling, due to main thread checking _thread_task_queue
		_mutex->lock();
		callable_queue.pop_front();

		if (callable_queue.empty()) {
			QUEUE_EMPTY.store(true, std::memory_order_acquire);
		}
		_mutex->unlock();
	}
}


/*
- Computations that can safely be done outside of main thread - instantiating/generating the chunk
- should only be called inside _thread_process - purely because it can be slow
*/
void TerrainGenerator::_instantiate_chunk(Vector3 chunk_position, int chunk_lod) {
	Ref<Chunk> chunk;
	chunk.instantiate(world_scenario, chunk_position, chunk_lod);		// this is the only other time world_scenario is used, so it's thread safe
	chunk->_generate_chunk_mesh();

	task_buffer_manager.tasks_push_back(String(chunk_position), callable_mp(this, &TerrainGenerator::_add_chunk).bind(chunk_position, chunk));
}

void TerrainGenerator::_add_chunk(Vector3 chunk_position, Ref<Chunk> chunk) {
	_chunks[chunk_position] = chunk;
}

void TerrainGenerator::_update_chunk_mesh(Ref<Chunk> chunk, int chunk_lod) {
	chunk->_clear_mesh_data();		// reset chunk mesh before anything else

	chunk->_set_chunk_LOD(chunk_lod);
	chunk->_generate_chunk_mesh();

	//print_line("CHUNK: ", chunk->_get_chunk_position());
	chunk->set_flag(Chunk::FLAG::UPDATE, false);
}

// only called on main thread
void TerrainGenerator::_delete_chunk(Vector3 chunk_key) {
	_chunks.erase(chunk_key);
}

/*
- can probably delegate this to another thread
- TODO later
*/
void TerrainGenerator::_delete_far_away_chunks(Vector3 player_chunk) {
	_old_player_chunk = player_chunk;
	// If we need to delete chunks, give the new chunk system a chance to catch up.
	effective_render_distance = std::max(1, effective_render_distance - 1);

	int deleted_this_frame = 0;
	// We should delete old chunks more aggressively if moving fast.
	// An easy way to calculate this is by using the effective render distance.
	// The specific values in this formula are arbitrary and from experimentation.
	int max_deletions = CLAMP(2 * (render_distance - effective_render_distance), 2, 12);

	/*
	IDEA: 
	- add the chunks to delete to a SORTED list first, the furthest chunk being priority
	- consider making this sorted list apart of another thread
	*/

	// Also take the opportunity to delete far away chunks.
	for (KeyValue<Vector3, Ref<Chunk>> chunk : _chunks) {
		String task_name = String(chunk.key);

		// flag check - we do not want to tag the same chunk again
		if (player_chunk.distance_to(chunk.key) > _delete_distance && !chunk.value->get_flag(Chunk::FLAG::DELETE)) {
			// if an update chunk process is in the thread task queue, and is not at the front, we remove it
			// first process in deletion to attempt to 'catch up' to the update task quicker
			_mutex->lock();
			if (callable_queue.has(task_name) && !callable_queue.is_front(task_name)) {
				callable_queue.erase(task_name);
			}
			_mutex->unlock();

			// we do not delete right now because it is possible that there is an update task occuring
			// however, we can queue delete in the main thead task queue so that once update is finished, we can immediately delete 
			chunk.value->set_flag(Chunk::FLAG::DELETE, true);
			task_buffer_manager.tasks_push_back(String(chunk.key), callable_mp(this, &TerrainGenerator::_delete_chunk).bind(chunk.key));
			
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