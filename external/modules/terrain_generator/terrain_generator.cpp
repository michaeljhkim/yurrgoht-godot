#include "terrain_generator.h"

/*
CREDITs: 
- general structure borrowed from 'Voxel Game' demo https://github.com/godotengine/godot-demo-projects/tree/4.2-31d1c0c/3d/voxel

NOTES:
- remember that instantiating a node is usually safe on another thread, but ONLY if it is outside of the main scene tree
- basically, only touch nodes in the main scene tree ONLY IF on the main thread
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
		_thread_task_queue[StringName()] = Callable();	// empty (null) callable function
		_mutex_TTQ->unlock();

		_thread->wait_to_finish();	// Wait until it exits.
	}
	
	_thread.unref();
	_mutex_TTQ.unref();
	_mutex_ACQ.unref();

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
	_chunk_paths.clear();
	set_process(false);

	// delete child nodes
	for (Variant c : get_children()) {
		get_child(c)->queue_free();
		remove_child(get_child(c));
    }

	// delete nodes that are queued to be children - thread ended, so no mutexes required
	for (Chunk* new_task : _add_child_queue) {
		if (!new_task->is_queued_for_deletion()) {
			new_task->queue_free();
		}
	}
	_thread_task_queue.clear();
	_add_child_queue.clear();
}



// processes that only need to be done on intialization - mutexes, thread
void TerrainGenerator::ready() {
	_mutex_TTQ.instantiate();
	_mutex_ACQ.instantiate();
	_thread.instantiate();

	_thread->start(callable_mp(this, &TerrainGenerator::_thread_process), core_bind::Thread::PRIORITY_NORMAL);

	_old_player_chunk = (player_character->get_global_position() / Chunk::CHUNK_SIZE).round();
}

void TerrainGenerator::process(double delta) {
	if (player_character == nullptr) return;
	
	Vector<Chunk*> _new_chunks;

	// START adding newly instantiated chunks as child nodes (if exists)
	_mutex_ACQ->lock();
	if (!_add_child_queue.is_empty()) {
		_new_chunks = _add_child_queue;
		_add_child_queue.clear();
	}
	_mutex_ACQ->unlock();

	if (!_new_chunks.is_empty()) {
		for (Chunk* NC : _new_chunks) {
			add_child(NC);
			_chunk_paths[NC->_get_grid_position()] = NC->get_path();
		}
		_new_chunks.clear();
	}
	// FINISH adding child nodes

	Vector3 player_chunk = (player_character->get_global_position() / Chunk::CHUNK_SIZE).round();
	//player_character->get_global_rotation();
	
	if (_deleting || (Vector2(player_chunk.x,player_chunk.z) != Vector2(_old_player_chunk.x,_old_player_chunk.z))) {
		_delete_far_away_chunks(player_chunk);
		_generating = true;
	}
	if (!_generating) return;

	// Try to generate chunks ahead of time based on where the player is moving. - not sure what this does (study later, low priority)
	//player_chunk.y += round(CLAMP(player_character->get_velocity().y, -render_distance/4, render_distance/4));

	// Check existing chunks within range. If it doesn't exist, create it.
	for (int x = (player_chunk.x - effective_render_distance); x <= (player_chunk.x + effective_render_distance); x++) {
		for (int z = (player_chunk.z - effective_render_distance); z <= (player_chunk.z + effective_render_distance); z++) {
			Vector3 grid_position = Vector3(x, 0, z);
			//StringName task_name = String(grid_position) + "_TerrainGenerator::_instantiate_chunk";	
			StringName task_name = String(grid_position);	// probably do not need to add anything else

			_mutex_TTQ->lock();
			/*
			- these conditions need to be checked, otherwise extra nodes WILL be created and orphaned
			- check _thread_task_queue first, because the worker thread may need to use it right after
			*/
			if (
				_thread_task_queue.has(task_name) ||
				_chunk_paths.has(grid_position) ||
				(Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(x, z)) > render_distance)
			) {
				_mutex_TTQ->unlock();
				continue;
			}

			// direct callable instantiation
			_thread_task_queue[task_name] = callable_mp(this, &TerrainGenerator::_instantiate_chunk).bind(grid_position);
			_mutex_TTQ->unlock();

			/*
			// single thread version
			Chunk* chunk = memnew(Chunk);
			chunk->_set_grid_position(grid_position);
			chunk->_generate_chunk_mesh();

			add_child(chunk);
			_chunk_paths[grid_position] = chunk->get_path();
			*/

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
- should only be called inside _thread_process
*/
void TerrainGenerator::_instantiate_chunk(Vector3 grid_position) {
	Chunk* chunk = memnew(Chunk);
	chunk->_set_grid_position(grid_position);
	chunk->_generate_chunk_mesh();

	_mutex_ACQ->lock();
	_add_child_queue.push_back(chunk);
	_mutex_ACQ->unlock();

	chunk = nullptr;	// gaurentees reference removal
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

	for (const Vector3 chunk_key : _chunk_paths.keys()) {
		if (Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(chunk_key.x, chunk_key.z)) > _delete_distance) {
			// No longer need this, but this is just in case
			if (!has_node(_chunk_paths[chunk_key])) {
				NodePath name = _chunk_paths[chunk_key];
				print_line("NULL CHILD: " + name);
				continue;
			}

			get_node(_chunk_paths[chunk_key])->queue_free();
			remove_child(get_node(_chunk_paths[chunk_key]));
			_chunk_paths.erase(chunk_key);

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
