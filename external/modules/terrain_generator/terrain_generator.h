#pragma once

#include "core/variant/typed_dictionary.h"
//#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/3d/physics/character_body_3d.h"
#include "chunk.h"
#include "custom_types/helper_types.h"

//#include <atomic>
#include <chrono>


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
	- e.g, ((4 + 2) * 2)^2 = 144 chunks possible (but is unlikely)
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

	// the master list that contains a reference to each chunk
	// allows for value deletion while iterating
	HashMap<Vector3, Ref<Chunk>> _chunks;

	// lookup table for all LODs for all chunks
	HashMap<String, float> _LOD_table;

protected:
	AHashMap<StringName, Callable> _thread_task_queue;	// NOT USED, ONLY FOR TESTING CURRENTLY

	// Using anything but AHashMap, makes it so that the chunks are loaded in order
	// have NO idea why
	LRUQueue<StringName, Callable> callable_queue;
	TaskBufferManager task_buffer_manager;
    std::atomic_bool QUEUE_EMPTY = true;

	// Only for the worker thread
	void _thread_process();
	void _instantiate_chunk(Vector3 chunk_position, int chunk_lod);
	void _add_chunk(Vector3 chunk_position, Ref<Chunk> chunk);

	void _update_chunk_mesh(Ref<Chunk> chunk, int chunk_lod);
	void _delete_chunk(Vector3 chunk_key);

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