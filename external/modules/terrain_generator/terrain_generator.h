#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "core/object/ref_counted.h"
#include "core/variant/typed_dictionary.h"
#include "core/templates/fixed_vector.h"
//#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/3d/physics/character_body_3d.h"
#include "chunk.h"

#include <atomic>

/*
struct grid_algorithm {
	// generates a list of grid coordinates in a spiral pattern starting from the center
	// solution borrowed from: https://stackoverflow.com/questions/398299/looping-in-a-spiral
	PackedVector2Array generator_coordaintes(int max_distance) {
		PackedVector2Array grid;
		Vector2 xz_vector(0, 0);

		int layer_radius = 1; 	// each increment represents 2 sides of the 'square'
		max_distance *= 2;		// multiply by 2, so we can remove the second nested while loop, and only require 1 for both x and z
		bool is_z = 0; 			// determines if the current value to evaluate is 0 (x) or 1 (z)

		while (index < max_distance) {
			while (
				(index < max_distance) &&
				(2 * xz_vector[is_z] * direction < layer_radius)
			) {
				grid.push_back(xz_vector);
				xz_vector[is_z] += direction;
				++index;
			}

			is_z ^= 1;	// bitwise XOR - 0 (x) for even, 1 (z) for odd
			
			// is_z was computed for the next iteration, so we are actually checking if the next cycle is_z value is x or z
			// if is_z is x, that means the next iteration is x, and the current iteration is actually z
			// we only want to modify direction and layer_radius if the current iteration is z
			if (is_z == 0)
				continue;

			direction = -direction;
			++layer_radius;
		}

		return grid;
	}	
};
*/

class TerrainGenerator : public Node3D {
	GDCLASS(TerrainGenerator, Node3D);

	// export these values - to be defined in editor
	int seed_input;
	
	RID world_scenario;
	std::atomic_bool PROCESSING_TASKS[2] = {false, false};
	std::atomic_bool TASKS_READY[2] = {false, false};
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

	Ref<CoreBind::Mutex> _mutex_TTQ;	// mutex for _thread_task_queue
	Ref<CoreBind::Thread> _thread;		// worker thread - only need one worker thread currently

	// godot has no real queues, so I had to make do
	Vector<Callable> _process_tasks_buffer[2];

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
	AHashMap<Vector3, Ref<Chunk>> _chunks;		// the master list that contains a reference to each chunk

protected:
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

#endif