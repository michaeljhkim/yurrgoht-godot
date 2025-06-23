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

	/*
	- Here is why the _chunks map uses TypedDictionary<Vector3, NodePath> specifically
	- Vector3 represents where the chunk is in the grid (not a physical grid, just chunks surrounding player)
	- NodePath is the most direct/safe way to access a specific child node publicly
	- indices were constantly changing within the parent node, so using indices was impossible
	*/
	AHashMap<Vector3, Ref<Chunk>> _chunks;
	CharacterBody3D* player_character = nullptr;		// This is an OBJECT

protected:
	Ref<core_bind::Mutex> _mutex_TTQ;	// mutex for _thread_task_queue
	Ref<core_bind::Mutex> _mutex_ACQ;	// mutex for _add_child_queue
	Ref<core_bind::Thread> _thread;		// worker thread - only need one worker thread currently

	// godot has no real queues, so I had to make do

	// for copying the _add_child_queue - main thread only
	Vector<Callable> _process_task_queue;

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
	void _update_chunk_mesh(Vector3 grid_position, Vector3 chunk_position);

	static void _bind_methods();

public:
	TerrainGenerator();
	~TerrainGenerator();

	void _notification(int p_notification);
	//void init();	// probably do not need this, ready should take care of everything
	void ready();
	void process(double delta);

	void clean_up();
	void _delete_far_away_chunks(Vector3 player_chunk);

	void set_player_character(CharacterBody3D* p_node);
	CharacterBody3D* get_player_character() const;

};

#endif     //TERRAIN_GENERATOR_H