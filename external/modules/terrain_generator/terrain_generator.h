#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "core/object/ref_counted.h"
#include "core/variant/typed_dictionary.h"
//#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/3d/physics/character_body_3d.h"
#include "chunk.h"

#include <queue>


// I think I do not need to include grid_location here since terrain_grid already uses grid_location as the key

class TerrainGenerator : public Node3D {
	GDCLASS(TerrainGenerator, Node3D);

	// export these values - to be defined in editor
	int seed_input;
	
	RID world_scenario;

	/*
	- maximum possible number of chunks being rendered is (render_distance + 2)^2
	- e.g, (4+2)^2 = 36 chunks possible (but is unlikely)
	*/
	int render_distance = 4;
	int _delete_distance = render_distance + 2;
	int effective_render_distance = 0;
	Vector3 _old_player_chunk;

	bool _generating = true;
	bool _deleting = false;

	// the master list that contains a reference to each chunk
	AHashMap<Vector3, Ref<Chunk>> _chunks;
	CharacterBody3D* player_character = nullptr;		// This is an OBJECT

protected:
	Ref<CoreBind::Mutex> _mutex_TTQ;	// mutex for _thread_task_queue
	Ref<CoreBind::Mutex> _mutex_PTQ;	// mutex _process_task_queue _process_task_queue_add_child_queue
	Ref<CoreBind::Thread> _thread;		// worker thread - only need one worker thread currently

	// godot has no real queues, so I had to make do
	Vector<Callable> _process_task_queue;
	Vector<Callable> _new_tasks;	// for copying the _process_task_queue - main thread only

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
	*/
	AHashMap<StringName, Callable> _thread_task_queue;	

	// Only for the worker thread
	void _thread_process();
	void _instantiate_chunk(Vector3 grid_position, Vector3 chunk_position);
	void _add_chunk(Vector3 grid_position, Ref<Chunk> chunk);

	void _update_chunk_mesh(Vector3 grid_position, Vector3 chunk_position);

	static void _bind_methods();

public:
	TerrainGenerator();
	~TerrainGenerator();
	void clean_up();

	void _notification(int p_notification);
	//void init();	// probably do not need this, ready should take care of everything
	void ready();
	void process(double delta);

	void _delete_far_away_chunks(Vector3 player_chunk);

	void set_player_character(CharacterBody3D* p_node);
	CharacterBody3D* get_player_character() const;

};

#endif     //TERRAIN_GENERATOR_H