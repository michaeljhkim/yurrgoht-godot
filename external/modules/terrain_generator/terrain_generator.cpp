#include "terrain_generator.h"

void TerrainGenerator::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			process(get_process_delta_time());
			break;
		
		case NOTIFICATION_READY:
			ready();
			break;

		// put this into destructor maybe as well
		//case NOTIFICATION_UNPARENTED:
		case NOTIFICATION_EXIT_TREE: { 		// Thread must be disposed (or "joined"), for portability.
			clean_up();

			//if (_thread.is_valid() && _thread->is_alive()) {
			if (_thread.is_valid()) {
				_run_mutex->lock();
				_thread_run = false;
				_run_mutex->unlock();

				_thread->wait_to_finish();	// Wait until it exits.
			}

			_thread.unref();
			_mutex.unref();
			_mutex_2.unref();
			_run_mutex.unref();

			// thread ended, so I do not need to use mutexes
			_thread_task_queue.clear();
			_new_chunks_queue.clear();

			//clean_up();
		} break;
	}
}

// processes that only need to be done on intialization - generates the basic values needed going forward
void TerrainGenerator::ready() {
	_run_mutex.instantiate();
	_mutex.instantiate();
	_mutex_2.instantiate();
	_thread.instantiate();

	_thread->start(callable_mp(this, &TerrainGenerator::_thread_process), core_bind::Thread::PRIORITY_NORMAL);
}

void TerrainGenerator::process(double delta) {
	if (player_character == nullptr) return;
	
	AHashMap<Vector3, Chunk*> _new_chunks;

	_mutex_2->lock();
	if (!_new_chunks_queue.is_empty()) {
		_new_chunks = _new_chunks_queue;
		_new_chunks_queue.reset();
	}
	_mutex_2->unlock();

	if (!_new_chunks.is_empty()) {
		for (KeyValue<Vector3, Chunk*> cn : _new_chunks) {
			// THIS PART SPECIFICALLY IS CAUSING THE MEMORY LEAKS
			//cn.value->_generate_chunk_mesh();	
			cn.value->_draw_mesh();

			add_child(cn.value);
			_chunks[cn.key] = cn.value->get_path();
		}
		_new_chunks.reset();
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
			Vector3 chunk_position = Vector3(x, 0, z);

			if ((Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(x, z)) > render_distance) || 
				_chunks.has(chunk_position)
			) {
				continue;
			}

			/*
			Chunk* chunk = memnew(Chunk);
			chunk->_set_chunk_position(chunk_position);
			chunk->_generate_chunk_mesh();
			chunk->_draw_mesh();

			add_child(chunk);
			_chunks[chunk_position] = chunk->get_path();
			*/

			// this mutex lock is for _thread_task_queue
			_mutex->lock();
			_thread_task_queue.push_back(chunk_position);
			_mutex->unlock();

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
	_chunks.clear();
	set_process(false);

	for (Variant c : get_children()) {
		get_child(c)->queue_free();
		remove_child(get_child(c));
    }

	print_line("CHILDS LEFT NUM: ", get_child_count());
}

/*
- for the most part, this is an infinite loop, until the class itself is down
- remember, mutexes ONLY lock threads if there is another attempt to lock a value
*/
void TerrainGenerator::_thread_process() {
	bool running = true;
	bool tasks_exists = true;
	Vector3 chunk_position;

	while (running) {
		_run_mutex->lock();
		running = _thread_run;
		_run_mutex->unlock();

		_mutex->lock();
		tasks_exists = !_thread_task_queue.is_empty();
		if (tasks_exists) {
			chunk_position = _thread_task_queue[0];

			// we can remove the from here since we now have a copy
			_thread_task_queue.remove_at(0);
		}
		_mutex->unlock();

		// There are no tasks currently, continue
		// check 'running' flag here, just in case
		if (!tasks_exists || !running) {
			continue;
		}

		// These are the computations that I did not want to run on the main thread
		// instantiating and generating the chunk
		Chunk* chunk = memnew(Chunk);
		chunk->_set_chunk_position(chunk_position);
		chunk->_generate_chunk_mesh(_mutex_2);
		//chunk->_draw_mesh();

		//call_deferred("_add_child_chunk", chunk_position, chunk);

		_mutex_2->lock();
		//chunk->_generate_chunk_mesh();

		_new_chunks_queue[chunk_position] = chunk;
		_mutex_2->unlock();

		chunk = nullptr;
	}
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

	const Array key_list = _chunks.keys();
	
	for (const Vector3 chunk_key : key_list) {
		if (Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(chunk_key.x, chunk_key.z)) > _delete_distance) {
			//Node* child = ;

			// No longer need this, but this is just in case
			if (!has_node(_chunks[chunk_key])) {
				NodePath name = _chunks[chunk_key];
				print_line("NULL CHILD: " + name);
				continue;
			} 

			get_node(_chunks[chunk_key])->queue_free();
			remove_child(get_node(_chunks[chunk_key]));
			//call_deferred("remove_child", get_node(_chunks[chunk_key]));
			//call_deferred("_chunks.erase", chunk_key);
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