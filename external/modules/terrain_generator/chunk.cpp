#include "chunk.h"
#include "thirdparty/embree/kernels/bvh/bvh_statistics.h"

// NOTE: LocalVector push_back() does check if there is enough capacity, and only resizes if full
// Vector push_back() does not do this, and resizes regardless, so need to use set if you manually resize

Chunk::Chunk(RID scenario, int new_lod, Vector3 new_c_position, Vector3 new_grid_pos, Ref<Material> p_material) :
	material(p_material), position(new_c_position), grid_position(new_grid_pos), LOD_factor(new_lod)
{
	// render instance
	render_instance_rid = RS::get_singleton()->instance_create();
	RenderingServer::get_singleton()->instance_set_scenario(render_instance_rid, scenario);

	//float octave_total = 2.0 + (1.0 - (1.0 / lod));		// Partial Sum Formula (Geometric Series)
	noise.instantiate();
	noise->set_frequency(0.01 / 4.0);
	noise->set_fractal_octaves(2.0);	// high octaves not reccomended, makes LOD seams much more noticeable

	/*
	* instantiate and create a new material for the mesh -> imitates default material
	* we imitate the default material is because it allows the geometry of a mesh to stand out
	* real godot default material cannot be used due to less testing options
	*/
	if (material.is_null()) {
		material.instantiate();
	}
	/*
	material.instantiate();
	material->set_albedo(Color(0.7, 0.7, 0.7));   // slightly darker gray
	material->set_metallic(0.f);
	material->set_roughness(1.f);
	material->set_shading_mode(BaseMaterial3D::ShadingMode::SHADING_MODE_PER_PIXEL);
	material->set_diffuse_mode(BaseMaterial3D::DiffuseMode::DIFFUSE_BURLEY);
	material->set_specular_mode(BaseMaterial3D::SpecularMode::SPECULAR_DISABLED);
	*/

	// mesh instance
	mesh_rid = RS::get_singleton()->mesh_create();
	RS::get_singleton()->instance_set_base(render_instance_rid, mesh_rid);

	generate_mesh();
}

Chunk::~Chunk() {
	reset_data();

	if (mesh_rid.is_valid())
		RS::get_singleton()->free(mesh_rid);

	if (render_instance_rid.is_valid())
		RS::get_singleton()->free(render_instance_rid);

	noise.unref();
	material.unref();
}

/*
* clears bare minimum for re-generation
* only used by updates
* clears does not de-allocate, less power needed, but more memory stored
* reset does de-allocate, requiring more power, but less memory 	-> maybe allow users to choose between
*/
void Chunk::clear_data() {
	flat_vertex_array.reset();
	vertex_array.reset();
	index_array.reset();
	/*
	flat_vertex_array.clear();
	vertex_array.clear();
	index_array.clear();
	*/
}

/*
* complete data reset
*/
void Chunk::reset_data() {
	clear_data();
	/*
	flat_vertex_array.reset();
	vertex_array.reset();
	index_array.reset();
	*/

	RS::get_singleton()->mesh_clear(mesh_rid);
	lod_surface_cache.clear();
	
	set_flag(Chunk::FLAG::UPDATE, false);
	set_flag(Chunk::FLAG::DELETE, false);
}



/*
MESH = {vertices, normals, uvs, tangents, indices}
- Used to generate the mesh of a chunk
- MESH was calculated manually and pushed directly to the RenderingServer

Why not use PlaneMesh with SurfaceTool?
    - Only calculates internal normals by default
	- Causes seaming issues, even between chunks with the same LOD (texture/lighting inconsistencies)

Why not use ArrayMesh + MeshInstance3D?
    - Adds too much overhead
    - No multi-threading once in the scene tree -> must run on main thread

Solution:
    Generate MESH with an extra row/column on all 4 sides
	- e.g. for 4x4 chunk -> generate 6x6 -> then remove outer rows/columns
    - fixes seams between chunks (with the same LOD)
	- can be done multi-threaded
*/
int nearest_multiple_2(int num) {
    return 2 * ((num + 1) / 2);
}

void Chunk::generate_mesh() {
	// draw_mesh if SurfaceData for current LOD_Factor exists
	if (lod_surface_cache.has(LOD_factor)) {
		draw_mesh();
		return;
	}
	// create new SurfaceData instance if it does not exist
	lod_surface_cache.insert(LOD_factor, RS::SurfaceData());
	float lod = pow(2, CLAMP(LOD_factor, 0, LOD_LIMIT));	// lod range -> 2**0 to 2**5

	// number of vertices (subdivide_w * subdivide_d)
	int subdivide_w = (CHUNK_SIZE / (CHUNK_RESOLUTION * lod)) + 1.0;
	int subdivide_d = (CHUNK_SIZE / (CHUNK_RESOLUTION * lod)) + 1.0;

	// distance between vertices
	int step_size_x = size.x / (subdivide_w - 1.0);
	int step_size_z = size.y / (subdivide_d - 1.0);

	// reserve known amount of vertices and indices
	vertex_array.reserve(pow(subdivide_d + 2, 2));
	index_array.reserve(pow(subdivide_d + 1, 2) * 6);
	flat_vertex_array.reserve(index_array.size());

	// start position shifted by half chunk size and one step for padding
	Size2 start_pos = size * -0.5 - Vector2(position.x, position.z) * CHUNK_SIZE - Vector2(step_size_x, step_size_z);
	float z = start_pos.y;
	
	int point = 0;
	int prevrow = 0;
	int thisrow = 0;

	for (int j = 0; j <= (subdivide_d + 1); j++) {
		float x = start_pos.x;
		prevrow = thisrow;
		thisrow = point;

		for (int i = 0; i <= (subdivide_w + 1); i++) {
			Vertex vert;
			vert.vertex = Vector3(-x, noise->get_noise_2d(-x,-z) * AMPLITUDE, -z);

			vert.uv = Vector2(
				1.0 - (i / (subdivide_w - 1.0)),
				1.0 - (j / (subdivide_d - 1.0))
			);

			vertex_array.push_back(vert);
			point++;
			
			if (i > 0 && j > 0) {	// de-index/flatten vertices -> avoid de-indexing later
				// triangle 1
				index_array.push_back(prevrow + i - 1);
				index_array.push_back(prevrow + i);
				index_array.push_back(thisrow + i - 1);

				// triangle 2
				index_array.push_back(prevrow + i);
				index_array.push_back(thisrow + i);
				index_array.push_back(thisrow + i - 1);
			}
			x += step_size_x;
		}
		z += step_size_z;
	}
	
	// DRAW MESH
	draw_mesh();
}

void Chunk::draw_mesh() {
	RS::get_singleton()->mesh_clear(mesh_rid);
	RS::get_singleton()->mesh_add_surface(mesh_rid, lod_surface_cache[LOD_factor]);

	// for some reason, issues arise when used in constructor, use here
	// i think its caused by mesh_clear()
	RS::get_singleton()->instance_set_surface_override_material(render_instance_rid, 0, material->get_rid());

	// free memory space to allow more chunks to be allocated
	clear_data();
}

void Chunk::_bind_methods() {
}

