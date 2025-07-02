#include "chunk.h"
#include "thirdparty/embree/kernels/bvh/bvh_statistics.h"

// NOTE: LocalVector push_back() does check if there is enough capacity, and only resizes if full
// Vector push_back() does not do this, and resizes regardless, so need to use set if you manually resize

Chunk::Chunk(RID scenario, Vector3 new_c_position, int new_lod) {
	render_instance_rid = RS::get_singleton()->instance_create();

	RenderingServer::get_singleton()->instance_set_scenario(render_instance_rid, scenario);
	position = new_c_position;
	LOD_factor = new_lod;

	noise.instantiate();
	noise->set_frequency(0.01 / 4.0);
	noise->set_fractal_octaves(2.0);	// high octaves not reccomended, makes LOD seams much more noticeable

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

	RS::get_singleton()->instance_set_base(render_instance_rid, mesh_rid);
}

Chunk::~Chunk() {
	clear_data();	// check if the clear is valid
	RS::get_singleton()->mesh_clear(mesh_rid);
	
	if (mesh_rid.is_valid())
		RS::get_singleton()->free(mesh_rid);

	if (render_instance_rid.is_valid())
		RS::get_singleton()->free(render_instance_rid);

	noise.unref();
	material.unref();
}

/*
* clears bare minimum for re-generation
*/
void Chunk::clear_data() {
	vertex_array.reset();
	index_array.reset();
}

/*
* complete data reset
*/
void Chunk::reset_data() {
	clear_data();

	// ADD A ADD SURFACE CANCELLATION FLAG IF DELETING 
	//RS::get_singleton()->call_on_render_thread(callable_mp(RS::get_singleton(), &RS::mesh_clear).bind(mesh_rid));
	RS::get_singleton()->mesh_clear(mesh_rid);
	
	set_flag(Chunk::FLAG::UPDATE, false);
	set_flag(Chunk::FLAG::DELETE, false);
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
	- e.g. for 4x4 chunk -> generate 6x6 -> then remove outer rows/columns
    - fixes seams between chunks (with the same LOD)

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

void Chunk::generate_mesh() {
	Vector2 size(CHUNK_SIZE, CHUNK_SIZE);	// size should most likely be established elsewhere, but do not need to currently

	//float lod = MAX(pow(2, LOD_factor-1), 1.f);
	//float octave_total = 2.0 + (1.0 - (1.0 / lod));		// Partial Sum Formula (Geometric Series)
	float lod = CLAMP(pow(2, LOD_factor), 1, 128);

	// number of vertices (subdivide_w * subdivide_d)
	int subdivide_w = (CHUNK_SIZE * CHUNK_RESOLUTION / lod) + 1.0;
	int subdivide_d = (CHUNK_SIZE * CHUNK_RESOLUTION / lod) + 1.0;

	// distance between vertices
	int step_size_x = size.x / (subdivide_w - 1.0);
	int step_size_z = size.y / (subdivide_d - 1.0);

	// reserve known amount of vertices and indices
	vertex_array.reserve((subdivide_d + 2) * (subdivide_w + 2) * 6);
	index_array.reserve((subdivide_d + 1) * (subdivide_w + 1) * 6);

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
	}
	/*
	* These are use to cut off the edges later
	* Logic:
	*
	* (0,0) (0,1) (0,2) (0,3)
	* (1,0) (1,1) (1,2) (1,3)
	* (2,0) (2,1) (2,2) (2,3)
	* (3,0) (3,1) (3,2) (3,3)
	*
	* first_vertex = (0,0)
	* last_vertex = (3,3)
	* any vertice that has a x/z value of either 0 or 3, will be removed, resulting in:
	*
	*
	* 		(1,1) (1,2)
	* 		(2,1) (2,2)
	*
	*/
	Vector3 first_vertex = vertex_array.begin()->vertex;
	Vector3 last_vertex = (--vertex_array.end())->vertex;	// .end() returns out of bounds -> fixed with --
	
	/*
	DE-INDEX + 
	GENERATE NORMALS
	*/
	generate_normals();

	/*
	GENERATE TANGENTS + 
	RE-INDEX
	*/
	generate_tangents(first_vertex, last_vertex);

	
	/*
	GENERATE SURFACE DATA
	*/
	const int array_len = vertex_array.size();
	const int index_array_len = index_array.size();

	// Build format
	uint64_t format = 
		(1ULL << RS::ARRAY_VERTEX) |
		(1ULL << RS::ARRAY_INDEX)  |
		(1ULL << RS::ARRAY_TEX_UV) |
		(1ULL << RS::ARRAY_NORMAL) |
		(1ULL << RS::ARRAY_TANGENT);

	format &= ~(RS::ARRAY_FLAG_FORMAT_VERSION_MASK << RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT);
	format |= RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION & (RS::ARRAY_FLAG_FORMAT_VERSION_MASK << RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT);

	// Offsets and sizes
	uint32_t offsets[RS::ARRAY_MAX];
	uint32_t vertex_element_size, normal_element_size, attrib_element_size, skin_element_size;

	RS::get_singleton()->mesh_surface_make_offsets_from_format(
		format, array_len, index_array_len, offsets,
		vertex_element_size, normal_element_size, attrib_element_size, skin_element_size
	);

	// Index config
	const bool use_16bit = array_len > 0 && array_len <= (1 << 16);
	const int index_stride = use_16bit ? 2 : 4;

	// Allocate buffers
	Vector<uint8_t> surface_vertex_array;
	surface_vertex_array.resize((vertex_element_size + normal_element_size) * array_len);

	Vector<uint8_t> surface_attrib_array;
	surface_attrib_array.resize(attrib_element_size * array_len);

	Vector<uint8_t> surface_index_array;
	surface_index_array.resize(index_stride * index_array_len);

	// Pointers
	uint8_t *vw_base = surface_vertex_array.ptrw();
	uint8_t *aw_base = surface_attrib_array.ptrw();
	uint8_t *iw = surface_index_array.ptrw();

	// Precompute base offset pointers
	uint8_t *vtx_ptr   = vw_base + offsets[RS::ARRAY_VERTEX];
	uint8_t *norm_ptr  = vw_base + offsets[RS::ARRAY_NORMAL];
	uint8_t *tang_ptr  = vw_base + offsets[RS::ARRAY_TANGENT];
	uint8_t *uv_ptr    = aw_base + offsets[RS::ARRAY_TEX_UV];

	// AABB min/max
	Vector3 min = vertex_array[0].vertex;
	Vector3 max = vertex_array[0].vertex;

	// Pack all vertex attributes in one loop
	for (int i = 0; i < array_len; ++i) {
		const Vertex &v = vertex_array[i];

		// AABB
		min = min.min(v.vertex);
		max = max.max(v.vertex);

		// Position
		memcpy(&vtx_ptr[i * vertex_element_size], &v.vertex, sizeof(Vector3));

		// Normal (oct16 packed)
		Vector2 norm_enc = v.normal.octahedron_encode();
		uint16_t norm_packed[2] = {
			(uint16_t)CLAMP(norm_enc.x * 65535.0, 0, 65535),
			(uint16_t)CLAMP(norm_enc.y * 65535.0, 0, 65535)
		};
		memcpy(&norm_ptr[i * normal_element_size], norm_packed, sizeof(norm_packed));

		// Tangent (oct16 + handedness)
		const double h = v.binormal.dot(v.normal.cross(v.tangent)) < 0.0 ? -1.0 : 1.0;
		Vector2 tang_enc = v.tangent.octahedron_tangent_encode(h);
		uint16_t tang_packed[2] = {
			(uint16_t)CLAMP(tang_enc.x * 65535.0, 0, 65535),
			(uint16_t)CLAMP(tang_enc.y * 65535.0, 0, 65535)
		};
		if (tang_packed[0] == 0 && tang_packed[1] == 65535)
			tang_packed[0] = 65535;
		memcpy(&tang_ptr[i * normal_element_size], tang_packed, sizeof(tang_packed));

		// UV
		memcpy(&uv_ptr[i * attrib_element_size], &v.uv, sizeof(Vector2));
	}

	// Final AABB
	AABB aabb(min, max - min);
	aabb.size = aabb.size.max(
		Vector3(CMP_EPSILON, CMP_EPSILON, CMP_EPSILON)
	);

	// Index packing loop -> conditional checks are done once
	if (use_16bit) {
		uint16_t *iw16 = reinterpret_cast<uint16_t *>(iw);
		for (int i = 0; i < index_array_len; ++i)
			iw16[i] = (uint16_t)index_array[i];
	} else {
		uint32_t *iw32 = reinterpret_cast<uint32_t *>(iw);
		for (int i = 0; i < index_array_len; ++i)
			iw32[i] = (uint32_t)index_array[i];
	}

	// Populate surface_data
	surface_data.format = format;
	surface_data.primitive = RS::PRIMITIVE_TRIANGLES;
	surface_data.aabb = aabb;
	surface_data.vertex_data = surface_vertex_array;
	surface_data.attribute_data = surface_attrib_array;
	surface_data.vertex_count = array_len;
	surface_data.index_data = surface_index_array;
	surface_data.index_count = index_array_len;
	surface_data.blend_shape_data = PackedByteArray();
	surface_data.bone_aabbs = Vector<AABB>();
	surface_data.lods = Vector<RenderingServer::SurfaceData::LOD>();
	surface_data.uv_scale = Vector4(0, 0, 0, 0);

	// DRAW MESH
	draw_mesh();
}

void Chunk::draw_mesh() {
	RS::get_singleton()->mesh_clear(mesh_rid);
	RS::get_singleton()->mesh_add_surface(mesh_rid, surface_data);

	// for some reason, issues arise when used in constructor, use here
	// i think its caused by mesh_clear()
	RS::get_singleton()->instance_set_surface_override_material(render_instance_rid, 0, material->get_rid());

	// free memory space for more chunks
	//vertex_array.reset();
	//index_array.reset();
}


// seperated for future flexibility
/*
void Chunk::draw_mesh() {
	// surface_data -> Chunk class member 
	Error err = RS::get_singleton()->mesh_createsurface_data_from_arrays(
		&surface_data, (RS::PrimitiveType)Mesh::PRIMITIVE_TRIANGLES, p_arr, 	// created data
		TypedArray<Array>(), Dictionary(), 0									// empty data
	);
	ERR_FAIL_COND(err != OK);

	RS::get_singleton()->mesh_clear(mesh_rid);
	RS::get_singleton()->mesh_add_surface(mesh_rid, surface_data);

	// for some reason, issues arise when used in constructor, use here
	// i think its caused by mesh_clear()
	RS::get_singleton()->instance_set_surface_override_material(render_instance_rid, 0, material->get_rid());
}
*/






/*
GENERATE CHUNK NORMALS
*/
void Chunk::generate_normals(bool p_flip) {
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
		if (vertex.smooth_group == UINT32_MAX)
			continue;

		Vector3 *lv = smooth_hash.getptr(vertex);
		vertex.normal = lv ? 
			lv->normalized() : 
			Vector3();
	}
}



/*
GENERATE CHUNK TANGENTS
*/
void Chunk::generate_tangents(Vector3 &first_vertex, Vector3 &last_vertex) {
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


	/*
	  ---------- RE-INDEX START ----------
	*/

	// use '=' here resizes the capacity -> avoids future (expensive) resize operations
	AHashMap<Vertex &, int, VertexHasher> indices = vertex_array.size();

	uint32_t new_size = 0;
	for (Vertex &vertex : vertex_array) {
		// removes vertices as explained from the logic above
		if ((vertex.vertex.x == first_vertex.x) ||
			(vertex.vertex.z == first_vertex.z) ||
			(vertex.vertex.x == last_vertex.x)  ||
			(vertex.vertex.z == last_vertex.z)) {
			continue;
		}

		int *idxptr = indices.getptr(vertex);
		int idx;
		if (!idxptr) {
			idx = indices.size();
			vertex_array[new_size] = vertex;
			indices.insert_new(vertex_array[new_size], idx);
			++new_size;
		} else {
			idx = *idxptr;
		}

		index_array.push_back(idx);
	}
	vertex_array.resize(new_size);
	/*
	  ---------- RE-INDEX END ----------
	*/
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




