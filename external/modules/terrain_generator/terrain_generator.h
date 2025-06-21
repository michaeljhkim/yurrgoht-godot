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

	int render_distance = 4;
	int _delete_distance = render_distance + 2;
	int effective_render_distance = 0;
	Vector3 _old_player_chunk;

	bool _generating = true;
	bool _deleting = false;

	/*
	- Here is why the _chunks map uses TypedDictionary<Vector2i, NodePath> specifically
	- Vector2i represents where the chunk is in the grid surrounding the player
	- NodePath is the most accurate way to access a specific child node publicly
	- indices were constantly changing within the parent node, so managing with indices was impossible
	*/
	TypedDictionary<Vector3, NodePath> _chunks;
	CharacterBody3D* player_character = nullptr;		// This is an OBJECT

protected:
	Ref<core_bind::Mutex> _run_mutex;
	Ref<core_bind::Mutex> _mutex;
	Ref<core_bind::Mutex> _mutex_2;
	Ref<core_bind::Thread> _thread;
	bool _thread_run = true;

	// godot has no real queues, so I had to make do
	Vector<Vector3> _thread_task_queue;
	AHashMap<Vector3, Chunk*> _new_chunks_queue;

	void _thread_process();

	static void _bind_methods();

public:
	TerrainGenerator();
	//~TerrainGenerator();

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