#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "core/object/ref_counted.h"

// this class inherits from the noise class in noise.h
#include "fastnoise_lite.h"
#include "noise_texture_2d.h"
#include "texture_rect.h"
#include "gradient_texture.h"

//#include "core/variant/variant_utility.h"
//#include "scene/resources/image_texture.h"
#include "core/variant/typed_dictionary.h"

struct ChunkInfo{
	Vector2i grid_location;
	int8_t lod_level;

	ChunkInfo(Vector2i in_grid_location, int8_t in_lod_level): grid_location(in_grid_location), lod_level(in_lod_level) { 
	}
};


class TerrainGenerator : public RefCounted {
	GDCLASS(TerrainGenerator, RefCounted);

	int count;

protected:
	static void _bind_methods();

	int dimension = 512;
	int seed_input;
	int po2_dimensions;

	TextureRect heightmap_rect;
	TextureRect island_rect;
	FastNoiseLite noise;
	Gradient gradient;
	GradientTexture1D gradient_tex;


	//const HTerrainData = preload("./hterrain_data.gd")

	enum SeamType {
		SEAM_LEFT = 1,
		SEAM_RIGHT = 2,
		SEAM_BOTTOM = 4,
		SEAM_TOP = 8
		//,SEAM_CONFIG_COUNT = 16
	};

	// [seams_mask][lod]
	//Mesh _mesh_cache[SeamType::SEAM_CONFIG_COUNT];
	int _chunk_size_x = 16;
	int _chunk_size_y = 16;

	// Where the player is in the x and y axis
	Vector2i player_location;
	int8_t max_lod = 7;

	// just copied from the unreal example
	int chunk_view_distance = 8;
	float vertex_spacing = 300.0f;
	int quads_per_chunk = 128;
	float frequency = 10000.0f;
	float amplitude = 5000.0f;
	float offset = 0.1f;
	int32_t num_octaves = 3;
	float lacunarity = 2.0f;
	float persistence = 0.5f;

	HashMap<Vector2i, ChunkInfo> terrain_grid;

public:
	void add(int p_value);
	void reset();
	int get_total() const;

	Ref<Image> prepare_image();		// Can most likely just be done in gdscript, but created just in case it becomes cleaner to move the process here
	void generate_noise(Ref<Image> heightmap); 		// Generate noise - apply the noise from the gradient to the heightmap image
	Vector2i player_grid_location(Vector3 player_position);
	void process(Vector3 player_position);

	void remove_detached_chunks(Vector2i player_location);
	void add_new_chunks(Vector2i player_location);
	void add_chunk(Vector2i grid_location, int8_t lod_level);
	void update_chunk(Vector2i grid_location, int8_t lod_level);
	void generate_mesh(Vector2i grid_location, int8_t lod_level);

	int8_t get_lod_level(Vector2i grid_location, Vector2i player_location);
	int floor_div(int a, int b);

	TerrainGenerator();
};

#endif     //TERRAIN_GENERATOR_H