#pragma once

#include "core/variant/typed_dictionary.h"
#include "core/templates/fixed_vector.h"
//#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/3d/physics/character_body_3d.h"
#include "chunk.h"
#include "custom_types/helper_types.h"

//#include <atomic>
#include <chrono>


/*
- ROUGHLY TAKES 
- Time spent:  837273
- Time spent:  886614
559338
886614


TOMORROW IMPLEMENT THE NEW SYSTEM I WORKED ON TODAY
- INSIDE THE helper_types.h file

*/

class TerrainGenerator : public Node3D {
	GDCLASS(TerrainGenerator, Node3D);
	
	int count = 0;
	int frames = 0;

	// export these values - to be defined in editor
	int seed_input;
	std::chrono::_V2::system_clock::time_point start;
	
	RID world_scenario;
	std::atomic_bool THREAD_RUNNING{true};

	/*
	- maximum possible number of chunks being rendered is ((render_distance + 2) * 2)^2
	- e.g, ((render_distance + 2) * 2)^2 = 144 chunks possible (but is unlikely)
	*/
	static const int render_distance = 4;
	static constexpr int _delete_distance = render_distance + 2;
	int effective_render_distance = 0;
	
	Vector3 _old_player_chunk;
	CharacterBody3D* player_character = nullptr;		// This is an OBJECT

	bool _generating = true;
	bool _deleting = false;

	Ref<CoreBind::Mutex> _mutex;	// mutex for adding to task queue
	Ref<CoreBind::Thread> _thread;		// worker thread - only need one worker thread currently

	/*
	- used as a vector for the most part, the reason why it is not a vector is because Callables with specified bindings are not uniquely identified
	- on the brightside, this allows Callables to be created if and only if it does not exist

	REASONS FOR USING AHashMap AND NOT:
	- RBMap: 
		- do not need to iterate over elements (except for deletion at the very end)
	- HashMap:
		- do not need to keep an iterator or const pointer to Key, and intend to add/remove elements in the meantime
		- do not need to preserve insertion order when using erase (just need to get element at index 0)
		- `KeyValue` size is not very large
	
	- ONLY HERE NOW BECAUSE NOT SURE ABOUT PERFORMANCE
	*/

	AHashMap<StringName, Callable> _thread_task_queue;
	AHashMap<Vector3, Ref<Chunk>> _chunks;		// the master list that contains a reference to each chunk

protected:
	TaskBufferManager task_buffer_manager;
	LRUQueue<StringName, Callable> callable_queue;
    std::atomic_bool queue_empty = true;

	void _main_thread_tasks(u_int b_num);

	// Only for the worker thread
	void _thread_process();
	void _instantiate_chunk(Vector3 grid_position, Vector3 chunk_position);
	void _add_chunk(Vector3 grid_position, Ref<Chunk> chunk);

	// not actually used right now
	void _update_chunk_mesh(Vector3 grid_position, Vector3 chunk_position);

	static void _bind_methods();

public:
	TerrainGenerator();
	~TerrainGenerator();
	void clean_up();

	void _notification(int p_notification);
	//void _init();	// probably do not need this, ready should take care of everything
	void _ready();
	void _process(double delta);

	void _delete_far_away_chunks(Vector3 player_chunk);

	void set_player_character(CharacterBody3D* p_node);
	CharacterBody3D* get_player_character() const;

};