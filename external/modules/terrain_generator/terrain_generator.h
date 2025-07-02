#pragma once

//#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/3d/physics/character_body_3d.h"
#include "chunk.h"
#include "custom_types/helper_types.h"

//#include <atomic>
#include <chrono>


class TerrainGenerator : public Node3D {
	GDCLASS(TerrainGenerator, Node3D);
	
	// DEBUG VALUES
	int count = 0;
	int frames = 0;

	// export these values - to be defined in editor
	int seed_input;
	std::chrono::_V2::system_clock::time_point start;
	
	RID world_scenario;

	// maximum number of chunks -> gaurentees that infinite chunks are not being instantiated
	int MAX_CHUNKS_NUM;
	int chunk_count = 0;

	static const int render_distance = 5;
	static constexpr int _delete_distance = render_distance + 1;
	int effective_render_distance = 0;
	
	Vector3 _old_player_chunk;
	CharacterBody3D* player_character = nullptr;

	bool _generating = true;
	bool _deleting = false;

	Ref<CoreBind::Mutex> _reuse_mutex;	// mutex for reuse buffer

	// the master list that contains a reference to each chunk
	// allows for value deletion while iterating
	HashMap<Vector3, Ref<Chunk>> _chunks;

protected:
	RingBuffer<Ref<Chunk>> _reuse_pool;
	//RingBuffer<Ref<Chunk>> delete_queue;
	MainThreadManager main_thread_manager;
	TaskThreadManager task_thread_manager;

	// Only for worker threads
	void _instantiate_chunk(Vector3 chunk_position, int chunk_lod, Vector3 grid_position);
	void _update_chunk_mesh(Ref<Chunk> chunk, int chunk_lod, Vector3 grid_position);
	
	// Only for main thread
	void _add_chunk(Vector3 chunk_position, Ref<Chunk> chunk);
	void _delete_chunk(Vector3 chunk_key);
	void _delete_far_away_chunks(Vector3 player_chunk);

	static void _bind_methods();

public:
	TerrainGenerator();
	~TerrainGenerator();
	void clean_up();

	void _notification(int p_notification);
	//void _init();	// probably do not need this, ready should take care of everything
	void _ready();
	void _process(double delta);

	void set_player_character(CharacterBody3D* p_node);
	CharacterBody3D* get_player_character() const;

};