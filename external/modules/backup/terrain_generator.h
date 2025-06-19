#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "core/object/ref_counted.h"
#include "core/variant/typed_dictionary.h"
#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/resources/surface_tool.h" 
#include "scene/3d/physics/static_body_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/character_body_3d.h"
#include "external/godot/scene/3d/physics/collision_shape_3d.h"

#include "core/core_bind.h"

//#include "chunk.h"


// I think I do not need to include grid_location here since terrain_grid already uses grid_location as the key

class TerrainGenerator : public Node3D {
	GDCLASS(TerrainGenerator, Node3D);

	// Signals
	//SignalNode chunk_spawned(chunkIndex, chunkNode);
	//SignalNode chunk_removed(chunkIndex);

	/*
		export values
	*/

	// Generator
	NodePath generatorNode;
	bool dynamicGeneration = true;
	NodePath playerNode; 
	
	// Chunk System
	int chunkLoadRadius = 0;
	//export var chunkSize: Vector2 = Vector2(50,50)
	float mapUpdateTime = 0.1;
	
	// Grid size (for 1 chunk)
	Vector2 gridSize = Vector2(51, 51);
	
	// Enabling/Disabling marching squares
	bool marchingSquares = true;
	
	// Physics
	bool addCollision = true;
	
	// Material
	TypedArray<Material> materials;
	enum Filter {ALL, WHITELIST, BLACKLIST};
	TypedArray<int> materialFilters;
	TypedArray<String> materialValues;
	
	// Tilesheet
	Vector2 tilesheetSize = Vector2(2, 2);
	Vector2 tileMargin = Vector2(0.01, 0.01);
	
	// Additional
	Vector3 offset = Vector3(-0.5, 0, -0.5);

	// Generator
	Node* generator;
	bool generatorHasValueFunc = false;
	bool generatorHasHeightFunc = false;
	Node3D* player;

	// Chunk loading
	struct Chunk {
		Vector<Ref<ArrayMesh>> surfaces;
		CollisionShape3D* collisionShape;
		Vector2 origin2d;
		Vector2 chunkIndex;
	};
	Vector<Chunk> chunkLoadQueue;
	Vector<Chunk> chunkRemoveQueue;
	Vector<MeshInstance3D*> loadedChunks;
	PackedVector2Array loadedChunkPositions;

	// Second thread
	Ref<core_bind::Thread> thread;
	float threadTime = 0.0;
	bool threadActive = true;
	Ref<Mutex> mutex;
	Vector3 playerPosition = Vector3(0.f, 0.f, 0.f);

	// Quad corners
	PackedVector2Array cornerVectors = {Vector2(0,0), Vector2(1,0), Vector2(0,1), Vector2(1,1)};
		
	// Trigs for each cell
	// |0  1|
	// |2  3|
	PackedFloat64Array cell = {
		// Quad 0
		// 0
		0,   0,
		0.5, 0,
		0,   0.5,
		
		// 1
		0,   0.5,
		0.5, 0,
		0.5, 0.5,
		
		// Quad 1
		// 2
		0.5, 0.5,
		0.5, 0,
		1,   0.5,
		// 3
		1,   0.5,
		0.5, 0,
		1,   0,
		
		// Quad 2
		// 4
		0,   1,
		0,   0.5,
		0.5, 1,
		
		// 5
		0.5, 1,
		0,   0.5,
		0.5, 0.5,
		
		// Quad 3
		// 6
		0.5, 0.5,
		1,   0.5,
		0.5, 1,
		
		// 7
		0.5, 1,
		1,   0.5,
		1,   1
	};
	PackedInt64Array get_trigs_marching(PackedInt64Array corners);
	void clean();
	void generate_chunk(Vector2 chunkIndex);
	void remove_chunk(int chunkIndex);
	Callable map_update();


	/*
	- Here is why the _chunks map uses TypedDictionary<Vector2i, NodePath> specifically
	- Vector2i represents where the chunk is in the grid surrounding the player
	- NodePath is the most accurate way to access a specific child node publicly
	- indices were constantly changing within the parent node, so managing with indices was impossible
	*/
	TypedDictionary<Vector2i, NodePath> _chunks;
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