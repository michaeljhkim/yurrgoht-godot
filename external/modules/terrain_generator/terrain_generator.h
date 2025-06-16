#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "core/object/ref_counted.h"
#include "core/templates/a_hash_map.h"
//#include "core/typedefs.h"

#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h
//#include "scene/resources/gradient_texture.h"

//#include "scene/3d/node_3d.h"
//#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/character_body_3d.h"
//#include "scene/resources/3d/primitive_meshes.h"
//#include "scene/resources/surface_tool.h"
#include <algorithm>

#include "chunk.h"


// I think I do not need to include grid_location here since terrain_grid already uses grid_location as the key

class TerrainGenerator : public Node3D {
	GDCLASS(TerrainGenerator, Node3D);
	
	int count = 0;
	int render_distance = 8;
	int _delete_distance = render_distance + 2;
	int effective_render_distance = 0;
	Vector3i _old_player_chunk;

	bool _generating = true;
	bool _deleting = false;

	AHashMap<Vector2i, Chunk*> _chunks;
	CharacterBody3D* player_character = nullptr;		// This is an OBJECT

protected:
	void _notification(int p_notification);
	//void init();	// probably do not need this, ready should take care of everything
	void ready();
	void process(float delta);

	static void _bind_methods();

	// export these values - to be defined in editor
	int seed_input;
	

	//HashMap<Vector2i, Partition*> terrain_grid;

public:
	void clean_up();
	void _delete_far_away_chunks(Vector3i player_chunk);

	void set_player_character(CharacterBody3D* p_node);
	CharacterBody3D* get_player_character() const;

	TerrainGenerator();
};

#endif     //TERRAIN_GENERATOR_H