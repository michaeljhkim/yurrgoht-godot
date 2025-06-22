#include "terrain_generator.h"

TerrainGenerator::~TerrainGenerator() {
	if (_thread.is_valid()) {
		_mutex_RUN->lock();
		_thread_run = false;
		_mutex_RUN->unlock();

		_thread->wait_to_finish();	// Wait until it exits.
	}
	
	_thread.unref();
	_mutex_TTQ.unref();
	_mutex_ACQ.unref();
	_mutex_RUN.unref();

	clean_up();

}

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

void TerrainGenerator::clean_up() {
	_chunks.clear();
	set_process(false);

	for (Variant c : get_children()) {
		get_child(c)->queue_free();
		remove_child(get_child(c));
    }

	// thread ended, so I do not need to use mutexes
	for (Chunk* new_task : _add_child_queue) {
		if (!new_task->is_queued_for_deletion()) {
			new_task->queue_free();
		}
	}
	_thread_task_queue.clear();
	_add_child_queue.clear();

	//print_line("CHILDS LEFT NUM: ", get_child_count());
}



// processes that only need to be done on intialization - mutexes, thread
void TerrainGenerator::ready() {
	_mutex_RUN.instantiate();
	_mutex_TTQ.instantiate();
	_mutex_ACQ.instantiate();
	_thread.instantiate();

	_thread->start(callable_mp(this, &TerrainGenerator::_thread_process), core_bind::Thread::PRIORITY_NORMAL);
}

void TerrainGenerator::process(double delta) {
	if (player_character == nullptr) return;
	
	Vector<Chunk*> _new_chunks;

	_mutex_ACQ->lock();
	if (!_add_child_queue.is_empty()) {
		_new_chunks = _add_child_queue;
		_add_child_queue.clear();
	}
	_mutex_ACQ->unlock();

	if (!_new_chunks.is_empty()) {
		for (Chunk* NC : _new_chunks) {
			add_child(NC);
			_chunks[NC->_get_grid_position()] = NC->get_path();
		}
		_new_chunks.clear();
	}

	//Vector3 player_location = player_character->get_global_position().snapped(Vector3(1.f, 1.f, 1.f) * 64.f) * Vector3(1.f, 0.f, 1.f);
	//Vector3 player_chunk = player_character->get_global_position().snapped(Vector3(1.f, 1.f, 1.f)  / Chunk::CHUNK_SIZE );
	Vector3 player_chunk = (player_character->get_global_position() / Chunk::CHUNK_SIZE).round();
	
	if (_deleting || (Vector2(player_chunk.x,player_chunk.z) != Vector2(_old_player_chunk.x,_old_player_chunk.z))) {
		_delete_far_away_chunks(player_chunk);
		_generating = true;
	}
	if (!_generating) return;

	// Try to generate chunks ahead of time based on where the player is moving. - not entirely sure what this does (study more later, low priority)
	player_chunk.y += round(CLAMP(player_character->get_velocity().y, -render_distance/4, render_distance/4));

	// Check existing chunks within range. If it doesn't exist, create it.
	for (int x = (player_chunk.x - effective_render_distance); x <= (player_chunk.x + effective_render_distance); x++) {
		for (int z = (player_chunk.z - effective_render_distance); z <= (player_chunk.z + effective_render_distance); z++) {
			Vector3 grid_position = Vector3(x, 0, z);
			Callable task_function = callable_mp(this, &TerrainGenerator::_instantiate_chunk).bind(grid_position);

			_mutex_TTQ->lock();
			// we NEED to check if these exist, otherwise extra nodes WILL be created and orphaned due to other existance checks
			if (
				_thread_task_queue.has(task_function) ||	// we check _thread_task_queue first, since if true, we want to unlock the mutex asap
				_chunks.has(grid_position) ||
				(Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(x, z)) > render_distance)
			) {
				_mutex_TTQ->unlock();

				task_function.~Callable();
				continue;
			}

			_thread_task_queue.push_back(task_function);
			_mutex_TTQ->unlock();

			/*
			Chunk* chunk = memnew(Chunk);
			chunk->_set_grid_position(grid_position);
			chunk->_generate_chunk_mesh();
			chunk->_draw_mesh();

			add_child(chunk);
			_chunks[grid_position] = chunk->get_path();
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
- for the most part, this is an infinite loop, until the class itself is down
- remember, mutexes ONLY lock threads if there is another attempt to lock a value
*/
void TerrainGenerator::_thread_process() {
	bool running = true;
	bool tasks_exists = true;
	Callable task;

	while (running) {
		_mutex_RUN->lock();
		running = _thread_run;
		_mutex_RUN->unlock();

		_mutex_TTQ->lock();
		tasks_exists = !_thread_task_queue.is_empty();
		if (tasks_exists) {
			task = _thread_task_queue[0];
		}
		_mutex_TTQ->unlock();

		// There are no tasks currently, continue
		if (!tasks_exists) {
			continue;
		}

		task.call();	// arguements should have been bound using .bind() before adding to task queue

		// its only save to remove the callable AFTER instantiating the above, because the main thread constantly checks the _thread_task_queue
		_mutex_TTQ->lock();
		_thread_task_queue.remove_at(0);
		_mutex_TTQ->unlock();
	}
}

/*
- realistically, should only be called inside thread
*/
void TerrainGenerator::_instantiate_chunk(Vector3 grid_position) {
	// These are the computations that I did not want to run on the main thread
	// instantiating and generating the chunk
	Chunk* chunk = memnew(Chunk);
	chunk->_set_grid_position(grid_position);
	chunk->_generate_chunk_mesh();

	// if 'running' flag was set to false right before here
	/*
	if (!running) {
		chunk->queue_free();
		return;
	}
	*/

	// it is possible for the _add_child_queue to push back a chunk, then the chunk to be cleared immediately. Must figure out how to prevent this 
	_mutex_ACQ->lock();
	_add_child_queue.push_back(chunk);
	_mutex_ACQ->unlock();

	chunk = nullptr;
}

/*
function

while thread.is_alive(), and this thread is not stopped,
	- pop one chunk from task queue
	- start(mesh calculations)
	- add finished chunks to finished queue
- draw all chunks in finished queue
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
	// remove_child

	for (const Vector3 chunk_key : _chunks.keys()) {
		if (Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(chunk_key.x, chunk_key.z)) > _delete_distance) {
			// No longer need this, but this is just in case
			if (!has_node(_chunks[chunk_key])) {
				NodePath name = _chunks[chunk_key];
				print_line("NULL CHILD: " + name);
				continue;
			}

			get_node(_chunks[chunk_key])->queue_free();
			remove_child(get_node(_chunks[chunk_key]));
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