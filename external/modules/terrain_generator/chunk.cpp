#include "chunk.h"
#include "thirdparty/embree/kernels/bvh/bvh_statistics.h"

// NOTE: LocalVector push_back() does check if there is enough capacity, and only resizes if full
// Vector push_back() does not do this, and resizes regardless, so need to use set if you manually resize

Chunk::Chunk(RID scenario, Vector3 new_c_position, int new_lod) {
	RS_instance_rid = RS::get_singleton()->instance_create();

	RenderingServer::get_singleton()->instance_set_scenario(RS_instance_rid, scenario);
	chunk_position = new_c_position;
	chunk_LOD = MAX(new_lod, 1.0);

	p_arr.resize(Mesh::ARRAY_MAX);
	noise.instantiate();
	mesh_rid = RS::get_singleton()->mesh_create();

	/*
	- instantiate and create a new material for the mesh -> imitates default material
	- we imitate the default material is because it allows the geometry of a mesh to be very visible

	- real godot default material cannot be used due to less testing options
	*/
	material = memnew(StandardMaterial3D);
	material->set_albedo(Color(0.7, 0.7, 0.7));   // slightly darker gray
	material->set_metallic(0.f);
	material->set_roughness(1.f);
	//material->set_depth_test();

	RS::get_singleton()->instance_set_base(RS_instance_rid, mesh_rid);
}

Chunk::~Chunk() {
	_clear_chunk_data();	// check if the clear is valid
	RS::get_singleton()->mesh_clear(mesh_rid);
	
	if (mesh_rid.is_valid())
		RS::get_singleton()->free(mesh_rid);

	if (RS_instance_rid.is_valid())
		RS::get_singleton()->free(RS_instance_rid);

	noise.unref();
	material.unref();
}

/*
clears bare minimum for re-generation
*/
void Chunk::_clear_chunk_data() {
	p_arr.clear();
	p_arr.resize(Mesh::ARRAY_MAX);

	vertex_array.reset();
	index_array.reset();

	set_flag(FLAG::DELETE, false);
	set_flag(FLAG::UPDATE, false);
}

/*
complete data reset
*/
void Chunk::_reset_chunk_data() {
	_clear_chunk_data();
	_set_chunk_position(Vector3(0, 0, 0));
	
	//RS::get_singleton()->call_on_render_thread(callable_mp(RS::get_singleton(), &RS::mesh_clear).bind(mesh_rid));
	RS::get_singleton()->mesh_clear(mesh_rid);
}






/*
MESH = {vertices, normals, uvs, tangents, indices}
- Used to generate the mesh of a chunk
- MESH was calculated manually and pushed directly to the RenderingServer

Why not use PlaneMesh with SurfaceTool?
    - Only calculates internal normals by default
	- Causes seaming issues even between chunks with the same LOD (texture/lighting inconsistencies)

Solution:
    Generate MESH with an extra row/column on all 4 sides
	- e.g. for 4x4 chunk → generate 6x6 → then remove outer rows/columns
    - fixes seams between chunks

SurfaceTool doesn’t support this method internally
	- All calculations done manually
	- Reused most of:
		-> PlaneMesh::_create_mesh_array()
		-> SurfaceTool::index(), deindex(), generate_normals(), generate_tangents()
    - a few structs

Why not use ArrayMesh + MeshInstance3D?
    -> Adds too much overhead
    -> No multi-threading once in the scene tree -> must run on main thread

- These issues made it too unoptimized -> had to scrap that path
- Pushing directly to RenderingServer worked well
- Can be done off the main thread -> more optimization possible
*/

/*
- the lower the chunk detail (higher lod), the higher the octaves
- creates the illusion that vertex count remains continuous
*/
void Chunk::_generate_chunk_mesh() {
	Vector2 size(CHUNK_SIZE, CHUNK_SIZE);	// size should most likely be established elsewhere, but do not need to currently

	//float lod = MAX(pow(2, chunk_LOD-1), 1.f);
	float lod = pow(2, chunk_LOD);
	//float octave_total = 2.0 + (1.0 - (1.0 / lod));		// Partial Sum Formula (Geometric Series)

	noise->set_frequency(0.01 / 4.0);
	noise->set_fractal_octaves(2.0);	// high octaves not reccomended, makes LOD seams much more noticeable

	// number of vertices (subdivide_w * subdivide_d)
	int subdivide_w = ((CHUNK_SIZE * CHUNK_RESOLUTION) / lod) + 1.0;
	int subdivide_d = ((CHUNK_SIZE * CHUNK_RESOLUTION) / lod) + 1.0;

	// distance between vertices
	int step_size_x = size.x / (subdivide_w - 1.0);
	int step_size_z = size.y / (subdivide_d - 1.0);

	// exact amount of vertices and indices is known
	vertex_array.reserve((subdivide_d + 2) * (subdivide_w + 2) * 6);
	index_array.reserve((subdivide_d + 1) * (subdivide_w + 1) * 6);

	int i, j, prevrow, thisrow, point;
	float x, z;

	// subtract step size since we are calculating a chunk of size 1 larger on each side
	Size2 start_pos = size * -0.5 - Vector2(chunk_position.x, chunk_position.z) * CHUNK_SIZE - Vector2(step_size_x, step_size_z); 
	
	/* top + bottom */
	z = start_pos.y;	
	point = 0;
	thisrow = 0;
	prevrow = 0;

	for (j = 0; j <= (subdivide_d + 1); j++) {
		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			// Point orientation Y
			Vertex vert;
			//vert.vertex = Vector3(-x, (noise->get_noise_2d(-x, -z)*AMPLITUDE), -z);
			vert.vertex = Vector3(-x, 0, -z);

			// UVs -> 	'1.0 - uv' to match orientation with Quad
			vert.uv = Vector2(
				1.0 - (i / (subdivide_w - 1.0)),
				1.0 - (j / (subdivide_d - 1.0))
			);

			// done adding required values, add to array
			vertex_array.push_back(vert);

			point++;
			if (i > 0 && j > 0) {
				index_array.push_back(prevrow + i - 1);
				index_array.push_back(prevrow + i);
				index_array.push_back(thisrow + i - 1);

				index_array.push_back(prevrow + i);
				index_array.push_back(thisrow + i);
				index_array.push_back(thisrow + i - 1);
			}

			x += step_size_x;
		}
		z += step_size_z;

		prevrow = thisrow;
		thisrow = point;
	}
	/*
	* These are use to cut off the edges later.
	* Logic:
	*
	* (0,0) (0,1) (0,2) (0,3)
	* (1,0) (1,1) (1,2) (1,3)
	* (2,0) (2,1) (2,2) (2,3)
	* (3,0) (3,1) (3,2) (3,3)
	*
	* start_vertex = (0,0)
	* last_vertex = (3,3)
	* any vertice that has a x/z value of either 0 or 3, will be removed, resulting in:
	*
	*
	* 		(1,1) (1,2)
	* 		(2,1) (2,2)
	*
	*/
	//Vector3 start_vertex = (*vertex_array.begin()).vertex;
	//Vector3 last_vertex = (*vertex_array.end()).vertex;		// more elegant, but end does not actually return end
	Vector3 start_vertex = vertex_array[0].vertex;
	Vector3 last_vertex = vertex_array[vertex_array.size()-1].vertex;
	
	/*
	GENERATE NORMALS
	*/
	_generate_chunk_normals();

	/*
	GENERATE TANGENTS
	*/
	_generate_chunk_tangents();
	

	/*
	  ---------- RE-INDEX START ----------
	*/
	// use '=' here resizes the capacity -> avoids future (expensive) resize operations
	AHashMap<Vertex &, int, VertexHasher> indices = vertex_array.size();

	uint32_t new_size = 0;
	for (Vertex &vertex : vertex_array) {
		// removes vertices as explained from the logic above
		if ((vertex.vertex.x == start_vertex.x) ||
			(vertex.vertex.z == start_vertex.z) ||
			(vertex.vertex.x == last_vertex.x) ||
			(vertex.vertex.z == last_vertex.z)) {
			continue;
		}

		int *idxptr = indices.getptr(vertex);
		int idx;
		if (!idxptr) {
			idx = indices.size();
			vertex_array[new_size] = vertex;
			indices.insert_new(vertex_array[new_size], idx);
			new_size++;
		} else {
			idx = *idxptr;
		}

		index_array.push_back(idx);
	}
	vertex_array.resize(new_size);
	/*
	  ---------- RE-INDEX END ----------
	*/

	// Unpacking vertices, normals, uvs, and tangents into an array format that add_surface_from_arrays() accepts
	PackedVector3Array sub_vertex_array;
	PackedVector3Array sub_normal_array;
	PackedVector2Array sub_uv_array;
	PackedFloat32Array sub_tangent_array;
	Vector<int> sub_index_array;

	// Resize Vectors -> size for each Vector is known
	sub_vertex_array.resize(vertex_array.size());
	sub_normal_array.resize(vertex_array.size());
	sub_uv_array.resize(vertex_array.size());
	sub_tangent_array.resize(vertex_array.size() * 4);
	sub_index_array.resize(index_array.size());

	// Tangents are a little special, they have a 4th value called handedness -> determines visibility direction
	float *w = sub_tangent_array.ptrw();

	// Unpack -> vertices, normals, uvs, tangents
	for (int idx = 0; idx < vertex_array.size(); idx++) {
		const Vertex &v = vertex_array[idx];
		sub_vertex_array.set(idx, v.vertex);
		sub_normal_array.set(idx, v.normal);
		sub_uv_array.set(idx, v.uv);

		// Tangent handedness calculations
		w[idx * 4 + 0] = v.tangent.x;
		w[idx * 4 + 1] = v.tangent.y;
		w[idx * 4 + 2] = v.tangent.z;
		float d = v.binormal.dot(v.normal.cross(v.tangent));
		w[idx * 4 + 3] = d < 0 ? -1 : 1;
	}

	// Unpack -> indices 
	for (int idx = 0; idx < index_array.size(); idx++) {
		sub_index_array.set(idx, index_array[idx]);
	}

	p_arr[RS::ARRAY_VERTEX] = sub_vertex_array;
	p_arr[RS::ARRAY_NORMAL] = sub_normal_array;
	p_arr[RS::ARRAY_TANGENT] = sub_tangent_array;
	p_arr[RS::ARRAY_TEX_UV] = sub_uv_array;
	p_arr[RS::ARRAY_INDEX] = sub_index_array;

	// Draw mesh
	_draw_mesh();
}

// seperated for future flexibility
void Chunk::_draw_mesh() {
	
	// surface_data -> class member
	Error err = RS::get_singleton()->mesh_create_surface_data_from_arrays(
		&surface_data, (RS::PrimitiveType)Mesh::PRIMITIVE_TRIANGLES, p_arr, 	// created data
		TypedArray<Array>(), Dictionary(), 0	// empty data
	);
	ERR_FAIL_COND(err != OK);	// makes sure the surface is valid

	RS::get_singleton()->mesh_clear(mesh_rid);
	RS::get_singleton()->mesh_add_surface(mesh_rid, surface_data);
	print_line("TIME TEST", chunk_LOD);

	// for some reason, issues arise when used in constructor, use here
	// i think its caused by mesh_clear()
	//RS::get_singleton()->instance_set_surface_override_material(RS_instance_rid, 0, material->get_rid());
}









/*
GENERATE CHUNK NORMALS
*/
void Chunk::_generate_chunk_normals(bool p_flip) {
	/*
	  ---------- DE-INDEX START ----------
	*/

	// attempt was made to de-index on creation, but not possible through greedy algorithms
	LocalVector<Vertex> old_vertex_array = vertex_array;
	vertex_array.clear();
	// There are 6 indices per vertex
	for (const int &index : index_array) {
		ERR_FAIL_COND(uint32_t(index) >= old_vertex_array.size());
		vertex_array.push_back(old_vertex_array[index]);
	}
	index_array.clear();

	/*
	  ---------- DE-INDEX END ----------
	*/

	// Normal Calculations -> Highly unlikely that a mesh will reach UINT32_MAX
	AHashMap<SmoothGroupVertex, Vector3, SmoothGroupVertexHasher> smooth_hash = vertex_array.size();

	for (uint32_t vi = 0; vi < vertex_array.size(); vi += 3) {
		Vertex *v = &vertex_array[vi];

		Vector3 normal = !p_flip ?
			Plane(v[0].vertex, v[1].vertex, v[2].vertex).normal :
			Plane(v[2].vertex, v[1].vertex, v[0].vertex).normal;

		// Add face normal to smooth vertex influence if vertex is member of a smoothing group
		for (int i = 0; i < 3; i++) {
			if (v[i].smooth_group != UINT32_MAX) {
				Vector3 *lv = smooth_hash.getptr(v[i]);
				if (!lv)
					smooth_hash.insert_new(v[i], normal);
				else
					(*lv) += normal;
			} 
			else 
				v[i].normal = normal;
		}
	}

	for (Vertex &vertex : vertex_array) {
		if (vertex.smooth_group != UINT32_MAX) {
			Vector3 *lv = smooth_hash.getptr(vertex);
			vertex.normal = lv ? 
				lv->normalized() : 
				Vector3();
		}
	}
}



/*
GENERATE CHUNK TANGENTS
*/
void Chunk::_generate_chunk_tangents() {
	SMikkTSpaceInterface mkif;
	mkif.m_getNormal = mikktGetNormal;
	mkif.m_getNumFaces = mikktGetNumFaces;
	mkif.m_getNumVerticesOfFace = mikktGetNumVerticesOfFace;
	mkif.m_getPosition = mikktGetPosition;
	mkif.m_getTexCoord = mikktGetTexCoord;
	mkif.m_setTSpace = mikktSetTSpaceDefault;
	mkif.m_setTSpaceBasic = nullptr;

	SMikkTSpaceContext msc;
	msc.m_pInterface = &mkif;

	TangentGenerationContextUserData triangle_data;
	triangle_data.vertices = &vertex_array;
	triangle_data.indices = &index_array;
	msc.m_pUserData = &triangle_data;

	bool res = genTangSpaceDefault(&msc);
	ERR_FAIL_COND(!res);
}




int Chunk::mikktGetNumFaces(const SMikkTSpaceContext *pContext) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);

	return (triangle_data.indices->size() > 0) ?
		triangle_data.indices->size() / 3 :
		triangle_data.vertices->size() / 3;
}

int Chunk::mikktGetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace) {
	return 3; 	// always 3
}

void Chunk::mikktGetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);
	Vector3 v;
	if (triangle_data.indices->size() > 0) {
		uint32_t index = triangle_data.indices->operator[](iFace * 3 + iVert);
		if (index < triangle_data.vertices->size()) {
			v = triangle_data.vertices->operator[](index).vertex;
		}
	} else {
		v = triangle_data.vertices->operator[](iFace * 3 + iVert).vertex;
	}

	fvPosOut[0] = v.x;
	fvPosOut[1] = v.y;
	fvPosOut[2] = v.z;
}

void Chunk::mikktGetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);
	Vector3 v;
	if (triangle_data.indices->size() > 0) {
		uint32_t index = triangle_data.indices->operator[](iFace * 3 + iVert);
		if (index < triangle_data.vertices->size()) {
			v = triangle_data.vertices->operator[](index).normal;
		}
	} else {
		v = triangle_data.vertices->operator[](iFace * 3 + iVert).normal;
	}

	fvNormOut[0] = v.x;
	fvNormOut[1] = v.y;
	fvNormOut[2] = v.z;
}

void Chunk::mikktGetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);
	Vector2 v;
	if (triangle_data.indices->size() > 0) {
		uint32_t index = triangle_data.indices->operator[](iFace * 3 + iVert);
		if (index < triangle_data.vertices->size()) {
			v = triangle_data.vertices->operator[](index).uv;
		}
	} else {
		v = triangle_data.vertices->operator[](iFace * 3 + iVert).uv;
	}

	fvTexcOut[0] = v.x;
	fvTexcOut[1] = v.y;
}

void Chunk::mikktSetTSpaceDefault(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT,
		const tbool bIsOrientationPreserving, const int iFace, const int iVert) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);
	Vertex *vtx = nullptr;
	if (triangle_data.indices->size() > 0) {
		uint32_t index = triangle_data.indices->operator[](iFace * 3 + iVert);
		if (index < triangle_data.vertices->size()) {
			vtx = &triangle_data.vertices->operator[](index);
		}
	} else {
		vtx = &triangle_data.vertices->operator[](iFace * 3 + iVert);
	}

	if (vtx != nullptr) {
		vtx->tangent = Vector3(fvTangent[0], fvTangent[1], fvTangent[2]);
		vtx->binormal = Vector3(-fvBiTangent[0], -fvBiTangent[1], -fvBiTangent[2]); // for some reason these are reversed, something with the coordinate system in Godot
	}
}


void Chunk::_bind_methods() {
}