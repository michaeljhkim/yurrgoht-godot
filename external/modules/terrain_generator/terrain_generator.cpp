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
}

// destructor - make sure to clean up anything that the main scene tree might not
TerrainGenerator::~TerrainGenerator() {
	// worker thread will usually be active up until destruction
	if (_thread.is_valid()) {
		_mutex_TTQ->lock();
		// empty (null) callable function
		//_thread_task_queue[StringName()] = Callable();	
		_thread_task_queue["STOP_THREAD"] = Callable();
		_mutex_TTQ->unlock();

		_thread->wait_to_finish();	// Wait until it exits.
	}
	
	_thread.unref();
	_mutex_TTQ.unref();
	_mutex_PTQ.unref();

	clean_up();

}

/*
- allows C++ classes to use the same ready/process signals as in gdscript
- GDExtension has this built in
*/
void TerrainGenerator::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			process(get_process_delta_time());
			break;
		
		case NOTIFICATION_READY:
			ready();
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

	_thread_task_queue.clear();
	_process_task_queue.clear();
}



// processes that only need to be done on intialization - mutexes, thread
void TerrainGenerator::ready() {
	world_scenario = get_world_3d()->get_scenario();	// thread has not started yet, so this is safe

	_mutex_TTQ.instantiate();
	_mutex_PTQ.instantiate();
	_thread.instantiate();

	_thread->start(callable_mp(this, &TerrainGenerator::_thread_process), CoreBind::Thread::PRIORITY_NORMAL);

	/*
	while (player_character == nullptr) {
		continue;
	}
	*/

	//_old_player_chunk = (player_character->get_global_position() / Chunk::CHUNK_SIZE).round();
}

void TerrainGenerator::process(double delta) {
	if (player_character == nullptr) return;
	
	Vector3 player_chunk = (player_character->get_global_position() / Chunk::CHUNK_SIZE).round();
	player_chunk.y = 0;
	//player_character->get_global_rotation();	// reference for being able to render chunks according to the direction that the player is facing

	if (_deleting || (Vector2(player_chunk.x,player_chunk.z) != Vector2(_old_player_chunk.x,_old_player_chunk.z))) {
		_delete_far_away_chunks(player_chunk);
		_generating = true;
	}
	if (!_generating) return;


	// START running main thread process tasks - setup by worker thread

	_mutex_PTQ->lock();
	if (!_process_task_queue.is_empty()) {
		_new_tasks = _process_task_queue;
		_process_task_queue.clear();
	}
	_mutex_PTQ->unlock();

	if (!_new_tasks.is_empty()) {
		for (const Callable &NT : _new_tasks) {
			NT.call();
		}
		_new_tasks.clear();
	}
	// FINISH running main thread process tasks


	// Try to generate chunks ahead of time based on where the player is moving. - not sure what this does (study later, low priority)
	//player_chunk.y += round(CLAMP(player_character->get_velocity().y, -render_distance/4, render_distance/4));

	/*
	- for the most part, this nested for-loop generates chunks around the player from inner circle to outer circle
	- could probably take advantage of this and use the player direction to determine where to start rendering first
	- perhaps, pre-compute all possible values into a list, and determine which block player is facing during pre-computation
	- add to list according to generation priority? 
	*/

	// Check existing chunks within range. If it doesn't exist, create it.
	for (int x = -effective_render_distance; x <= effective_render_distance; x++) {
		for (int z = -effective_render_distance; z <= effective_render_distance; z++) {
			Vector3 grid_position = Vector3(x, 0, z);
			Vector3 chunk_position = player_chunk + grid_position;

			//StringName task_name = String(chunk_position) + "_TerrainGenerator::_update_chunk_mesh";
			StringName task_name = String(chunk_position);

			_mutex_TTQ->lock();

			/*
			if(_thread_task_queue.has(task_name))
				print_line("_thread_task_queue.has(task_name) = TRUE");
			if(_chunks.has(chunk_position))
				print_line("_chunks.has(grid_position) = TRUE");
			if(Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(chunk_position.x, chunk_position.z)) > render_distance)
				print_line("(Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(chunk_position.x, chunk_position.z)) = TRUE");
			*/

			// direct callable instantiation
			if (
				_thread_task_queue.has(task_name) ||	// we check _thread_task_queue first, since if true, we want to unlock the mutex asap
				_chunks.has(chunk_position) || 
				(player_chunk.distance_to(chunk_position) > render_distance)
			) {
				_mutex_TTQ->unlock();
				continue;
			}

			_thread_task_queue[task_name] = callable_mp(this, &TerrainGenerator::_instantiate_chunk).bind(grid_position, chunk_position);
			_mutex_TTQ->unlock();

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


/*
- worker thread function

- for the most part, this is an infinite loop, until the class itself is down
- remember, mutexes ONLY lock threads if there is another attempt to lock a value
*/
void TerrainGenerator::_thread_process() {
	bool running = true;
	Callable task;
	StringName task_name;

	while (running) {
		_mutex_TTQ->lock();
		if (_thread_task_queue.is_empty()) {	// there are no tasks currently, continue
			_mutex_TTQ->unlock();
			continue;
		}
		task = _thread_task_queue.get_by_index(0).value;			// tasks do exist, copy the front of the queue (Vector)
		_mutex_TTQ->unlock();
		
		if (task.is_null())		// destructor adds empty 'Callable' to _thread_task_queue - signals shutdown to _thread_process
			running = false;
		else 
			task.call();		// all arguements are bound (with .bind) BEFORE being added to task queue, so no arguements need to be provided here
			

		// task call complete, remove from the front of queue
		// only safe to remove AFTER calling, due to main thread checking _thread_task_queue
		_mutex_TTQ->lock();
		_thread_task_queue.erase_by_index(0);
		_mutex_TTQ->unlock();
	}
}


/*
- Computations that can safely be done outside of main thread - instantiating/generating the chunk
- should only be called inside _thread_process - purely because it can be slow
*/
void TerrainGenerator::_instantiate_chunk(Vector3 grid_position, Vector3 chunk_position) {
	Ref<Chunk> chunk;
	chunk.instantiate();
	chunk->_set_world_scenario(world_scenario);
	chunk->_set_chunk_position(chunk_position);
	chunk->_generate_chunk_mesh();

	_mutex_PTQ->lock();
	_process_task_queue.push_back(callable_mp(this, &TerrainGenerator::_add_chunk).bind(chunk_position, chunk));
	_mutex_PTQ->unlock();
}

void TerrainGenerator::_add_chunk(Vector3 grid_position, Ref<Chunk> chunk) {
	_chunks[grid_position] = chunk;
}


// done on worker thread
void TerrainGenerator::_update_chunk_mesh(Vector3 grid_position, Vector3 chunk_position) {
	Ref<Chunk> chunk = _chunks[grid_position];
	chunk->_set_chunk_position(chunk_position);
	chunk->_generate_chunk_mesh();
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
	int max_deletions = CLAMP(2 * (render_distance - effective_render_distance), 2, 8);

	// Also take the opportunity to delete far away chunks.
	for (int idx = 0; idx < _chunks.size(); idx++) {
		KeyValue<Vector3, Ref<Chunk>> chunk = _chunks.get_by_index(idx);
		Vector2 chunk_pos = Vector2(chunk.value->_get_chunk_position().x, chunk.value->_get_chunk_position().z);

		if (player_chunk.distance_to(chunk.key) > _delete_distance) {
			// add the task of freeing the mesh to the thread
			chunk.value->_clear_mesh_data();
			//chunk.value->~Chunk();
			_chunks.erase_by_index(idx);	// apparently, this calls the destructor now

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
