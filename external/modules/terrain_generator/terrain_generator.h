#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "core/object/ref_counted.h"
#include "core/variant/variant_utility.h"
#include "core/variant/typed_dictionary.h"
//#include "scene/resources/image_texture.h"

#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

//#include "modules/noise/noise_texture_2d.h"
//#include "scene/gui/texture_rect.h"
#include "scene/resources/gradient_texture.h"

#include "scene/3d/node_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/surface_tool.h"
#include <algorithm>


// I think I do not need to include grid_location here since terrain_grid already uses grid_location as the key
class Partition : public MeshInstance3D {
	GDCLASS(Partition, MeshInstance3D);

	Vector2i parititon_location;
	int8_t lod_level;

	Ref<PlaneMesh> plane_mesh;
	SurfaceTool surface_tool;

	float length;
	float mesh_quality = 2.0f;

	int x;
	int z;

	// need less jank solution later
	// I feel like I would not need to store these values for each parition
	Vector2 partition_size = Vector2(64.f, 64.f);
	float amplitude = 16.f;

	FastNoiseLite noise;
	
	//Partition();
public:
	/*
	Partition(Vector2i in_location, int8_t in_lod_level)
		: parititon_location(in_location), lod_level(in_lod_level) { 
	}
	*/
	// Need to optimize mesh generation in the future. For now, this will suffice
	Partition(Vector2i in_location, int x, int z) : x(x), z(z) {
		set_position(Vector3(float(x), 0.f, float(z)) * partition_size.x);
		
		// calculate the lod of this square according to how far it is from player
		// then set subdivides
		int lod = std::max(abs(x), abs(z));
		int subdivision_length = pow(2, lod);
		float subdivides = std::max(partition_size.x * mesh_quality / subdivision_length - 1.f, 0.f);

		// create a PlaneMesh as a base
		plane_mesh.instantiate();
		plane_mesh->set_size(partition_size);
		plane_mesh->set_subdivide_width(subdivides);
		plane_mesh->set_subdivide_depth(subdivides);

		generate_mesh();
	}

	void update_xz(Vector3 player_location) {
		//x += player_location.x;
		//z += player_location.z;
	}

	void generate_mesh() {
		//set_position(Vector3(float(x), 0.f, float(z)) * partition_size.x);

		// create surface tool
		surface_tool.create_from(plane_mesh, 0);
		Array data = surface_tool.commit_to_arrays();
		PackedVector3Array vertices = data[ArrayMesh::ARRAY_VERTEX];

		// define heights for each vertice using noise 
		// Due to godots custom Vector type, single vertex values must be set in this manner
		for (int i = 0; i < vertices.size(); i++) {
			Vector3 vertex = vertices.get(i);
			vertex.y = noise.get_noise_2d(get_global_position().x + vertices[i].x, get_global_position().z + vertices[i].z) * amplitude;

			vertices.set(i, vertex);
		}
		data[ArrayMesh::ARRAY_VERTEX] = vertices;

		Ref<ArrayMesh> array_mesh;
		array_mesh.instantiate();
		array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, data);

		surface_tool.create_from(array_mesh, 0);
		surface_tool.generate_normals();

		set_mesh(surface_tool.commit());
		// $CollisionShape.shape = array_mesh.create_trimesh_shape()

		array_mesh.unref();
		surface_tool.clear();
	}
};


class TerrainGenerator : public Node3D {
	GDCLASS(TerrainGenerator, Node3D);
	
	int count = 0;

protected:
	void _notification(int p_notification);
	//void init();	// probably do not need this, ready should take care of everything
	void ready();
	void process(float delta);

	static void _bind_methods();

	// export these values - to be defined in editor
	int seed_input;
	int partition_distance;	// number of chunks extending from player position - essentially the radius
	Node3D* player_character = nullptr;	// This is an OBJECT
	int _partition_size_x = 16;
	int _partition_size_y = 16;
	

	enum SeamType {
		SEAM_LEFT = 1,
		SEAM_RIGHT = 2,
		SEAM_BOTTOM = 4,
		SEAM_TOP = 8
		//,SEAM_CONFIG_COUNT = 16
	};

	// [seams_mask][lod]
	//Mesh _mesh_cache[SeamType::SEAM_CONFIG_COUNT];


	// just copied from the unreal example
	int8_t max_lod = 7;

	/*
	int chunk_view_distance = 8;
	float vertex_spacing = 300.0f;
	int quads_per_chunk = 128;
	float frequency = 10000.0f;
	float amplitude = 5000.0f;
	float offset = 0.1f;
	int32_t num_octaves = 3;
	float lacunarity = 2.0f;
	float persistence = 0.5f;
	*/

	HashMap<Vector2i, Partition*> terrain_grid;

public:
	//Ref<Image> prepare_image();		// Can most likely just be done in gdscript, but created just in case it becomes cleaner to move the process here
	//void generate_noise(Ref<Image> heightmap); 		// Generate noise - apply the noise from the gradient to the heightmap image
	//Vector2i player_grid_location(Vector3 player_position);

	/*
	void remove_detached_chunks(Vector2i player_location);
	void add_new_chunks(Vector2i player_location);
	void add_chunk(Vector2i grid_location, int8_t lod_level);
	void update_chunk(Vector2i grid_location, int8_t lod_level);

	void generate_mesh(Vector2i grid_location, int8_t lod_level);

	int8_t get_lod_level(Vector2i grid_location, Vector2i player_location);
	int floor_div(int a, int b);
	*/

	void set_player_character(Node3D *p_node);
	Node3D* get_player_character() const;

	TerrainGenerator();
};

#endif     //TERRAIN_GENERATOR_H