#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/3d/physics/character_body_3d.h"

#include "chunk.h"


// I think I do not need to include grid_location here since terrain_grid already uses grid_location as the key

class TerrainGenerator : public Node3D {
	GDCLASS(TerrainGenerator, Node3D);

	// export these values - to be defined in editor
	int seed_input;

	int render_distance = 8;
	int _delete_distance = render_distance + 2;
	int effective_render_distance = 0;
	Vector3i _old_player_chunk;

	bool _generating = true;
	bool _deleting = false;

	HashMap<Vector2i, int> _chunks;
	CharacterBody3D* player_character = nullptr;		// This is an OBJECT

protected:

	static void _bind_methods();

public:
	TerrainGenerator();
	//~TerrainGenerator();

	void _notification(int p_notification);
	//void init();	// probably do not need this, ready should take care of everything
	void ready();
	void process(double delta);

	void clean_up();
	void _delete_far_away_chunks(Vector3i player_chunk);

	void set_player_character(CharacterBody3D* p_node);
	CharacterBody3D* get_player_character() const;

};

#endif     //TERRAIN_GENERATOR_H