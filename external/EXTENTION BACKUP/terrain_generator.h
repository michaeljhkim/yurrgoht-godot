#ifndef TERRAINGENERATOR_H
#define TERRAINGENERATOR_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include "chunk.h"

namespace godot {

class TerrainGenerator : public Node3D {
	GDCLASS(TerrainGenerator, Node3D)
	
	int seed_input;

	//int count = 0;
	int render_distance = 8;
	int _delete_distance = render_distance + 2;
	int effective_render_distance = 0;
	Vector3i _old_player_chunk;

	bool _generating = true;
	bool _deleting = false;

	HashMap<Vector2i, int32_t> _chunks;
	CharacterBody3D* player_character = nullptr;

private:
	double time_passed;

protected:
	static void _bind_methods();

public:
	TerrainGenerator();
	~TerrainGenerator();

	//void _ready() override;
	void _process(double delta) override;

	void clean_up();
	void _delete_far_away_chunks(Vector3i player_chunk);

	void set_player_character(CharacterBody3D* p_node);
	CharacterBody3D* get_player_character() const;
};

}

#endif