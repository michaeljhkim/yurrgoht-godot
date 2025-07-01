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
	//float octave_total = 2.0 + (1.0 - (1.0 / lod));		// Partial Sum Formula (Geometric Series)
	float lod = pow(2, chunk_LOD);

	// number of vertices (subdivide_w * subdivide_d)
	int subdivide_w = (CHUNK_SIZE * CHUNK_RESOLUTION / lod) + 1.0;
	int subdivide_d = (CHUNK_SIZE * CHUNK_RESOLUTION / lod) + 1.0;

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
	thisrow = point;
	prevrow = 0;

	for (j = 0; j <= (subdivide_d + 1); j++) {
		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			// Point orientation Y
			Vertex vert;
			//vert.vertex = Vector3(-x, 0, -z);		// test if noise calculations was the bottleneck
			vert.vertex = Vector3(-x, (noise->get_noise_2d(-x, -z)*AMPLITUDE), -z);

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
	_generate_chunk_normals();

	/*
	GENERATE TANGENTS + 
	RE-INDEX
	*/
	_generate_chunk_tangents(first_vertex, last_vertex);

	// Unpacking vertices, normals, uvs, and tangents into an array format that add_surface_from_arrays() accepts
	// Prepare sub arrays
	PackedVector3Array sub_vertex_array;
	PackedVector3Array sub_normal_array;
	PackedVector2Array sub_uv_array;
	PackedFloat64Array sub_tangent_array;
	Vector<int> sub_index_array;

	const int array_len = vertex_array.size();
	const int index_array_len = index_array.size();

	sub_vertex_array.resize(array_len);
	sub_normal_array.resize(array_len);
	sub_uv_array.resize(array_len);
	sub_tangent_array.resize(array_len * 4);
	sub_index_array.resize(index_array_len);

	double *tangent_write = sub_tangent_array.ptrw();
	int *index_write = sub_index_array.ptrw();

	// Unpack vertex and index data
	for (int i = 0; i < array_len; ++i) {
		const Vertex &v = vertex_array[i];

		sub_vertex_array.set(i, v.vertex);
		sub_normal_array.set(i, v.normal);
		sub_uv_array.set(i, v.uv);

		tangent_write[i * 4 + 0] = v.tangent.x;
		tangent_write[i * 4 + 1] = v.tangent.y;
		tangent_write[i * 4 + 2] = v.tangent.z;
		tangent_write[i * 4 + 3] = v.binormal.dot(v.normal.cross(v.tangent)) < 0 ? -1.0 : 1.0;

		// Each vertex has 6 indices
		for (int j = 0; j < 6; ++j) {
			index_write[i * 6 + j] = index_array[i * 6 + j];
		}
	}

	// Surface format setup
	uint64_t format = 0;
	format |= (1ULL << RS::ARRAY_VERTEX) |
			(1ULL << RS::ARRAY_INDEX) |
			(1ULL << RS::ARRAY_TEX_UV) |
			(1ULL << RS::ARRAY_NORMAL) |
			(1ULL << RS::ARRAY_TANGENT);

	format &= ~(RS::ARRAY_FLAG_FORMAT_VERSION_MASK << RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT);
	format |= RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION & (RS::ARRAY_FLAG_FORMAT_VERSION_MASK << RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT);

	uint32_t offsets[RS::ARRAY_MAX];
	uint32_t vertex_element_size, normal_element_size, attrib_element_size, skin_element_size;

	RS::get_singleton()->mesh_surface_make_offsets_from_format(
		format, array_len, index_array_len, offsets,
		vertex_element_size, normal_element_size, attrib_element_size, skin_element_size
	);

	// Index stride
	const bool use_16bit = array_len > 0 && array_len <= (1 << 16);
	const int index_stride = use_16bit ? 2 : 4;

	// Allocate surface arrays
	Vector<uint8_t> surface_vertex_array;
	surface_vertex_array.resize((vertex_element_size + normal_element_size) * array_len);

	Vector<uint8_t> surface_attrib_array;
	surface_attrib_array.resize(attrib_element_size * array_len);

	Vector<uint8_t> surface_index_array;
	surface_index_array.resize(index_stride * index_array_len);

	// Raw data pointers
	uint8_t *vw = surface_vertex_array.ptrw();
	uint8_t *aw = surface_attrib_array.ptrw();
	uint8_t *iw = surface_index_array.ptrw();

	const Vector3 *vertex_src = sub_vertex_array.ptr();
	const Vector3 *normal_src = sub_normal_array.ptr();
	const Vector2 *uv_src = sub_uv_array.ptr();
	const double *tangent_src = sub_tangent_array.ptr();
	const int *index_src = sub_index_array.ptr();

	// AABB calculation
	AABB aabb;
	{
		Vector3 min = vertex_src[0];
		Vector3 max = vertex_src[0];
		for (int i = 1; i < array_len; ++i) {
			min = min.min(vertex_src[i]);
			max = max.max(vertex_src[i]);
		}
		aabb = AABB(min, max - min);
		aabb.size = aabb.size.max(SMALL_VEC3);
	}

	// Write vertex and attribute data
	for (int i = 0; i < array_len; ++i) {
		memcpy(&vw[offsets[RS::ARRAY_VERTEX] + i * vertex_element_size], &vertex_src[i], sizeof(Vector3));

		Vector2 normal_res = normal_src[i].octahedron_encode();
		uint16_t packed_normal[2] = {
			(uint16_t)CLAMP(normal_res.x * 65535.0, 0, 65535),
			(uint16_t)CLAMP(normal_res.y * 65535.0, 0, 65535)
		};
		memcpy(&vw[offsets[RS::ARRAY_NORMAL] + i * normal_element_size], packed_normal, sizeof(packed_normal));

		Vector3 tangent_vec(
			tangent_src[i * 4 + 0],
			tangent_src[i * 4 + 1],
			tangent_src[i * 4 + 2]
		);
		Vector2 tangent_res = tangent_vec.octahedron_tangent_encode(tangent_src[i * 4 + 3]);
		uint16_t packed_tangent[2] = {
			(uint16_t)CLAMP(tangent_res.x * 65535.0, 0, 65535),
			(uint16_t)CLAMP(tangent_res.y * 65535.0, 0, 65535)
		};
		if (packed_tangent[0] == 0 && packed_tangent[1] == 65535)
			packed_tangent[0] = 65535;
		memcpy(&vw[offsets[RS::ARRAY_TANGENT] + i * normal_element_size], packed_tangent, sizeof(packed_tangent));

		memcpy(&aw[offsets[RS::ARRAY_TEX_UV] + i * attrib_element_size], &uv_src[i], sizeof(Vector2));
	}

	// Write index data
	for (int i = 0; i < index_array_len; ++i) {
		if (use_16bit) {
			uint16_t idx = (uint16_t)index_src[i];
			memcpy(&iw[i * index_stride], &idx, sizeof(idx));
		} else {
			uint32_t idx = (uint32_t)index_src[i];
			memcpy(&iw[i * index_stride], &idx, sizeof(idx));
		}
	}

	// Final surface data setup
	surface_data.format = format;
	surface_data.primitive = (RS::PrimitiveType)Mesh::PRIMITIVE_TRIANGLES;
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



	RS::get_singleton()->mesh_clear(mesh_rid);
	RS::get_singleton()->mesh_add_surface(mesh_rid, surface_data);

	// for some reason, issues arise when used in constructor, use here
	// i think its caused by mesh_clear()
	RS::get_singleton()->instance_set_surface_override_material(RS_instance_rid, 0, material->get_rid());
}


// seperated for future flexibility
/*
void Chunk::_draw_mesh() {
	// surface_data -> Chunk class member 
	Error err = RS::get_singleton()->mesh_create_surface_data_from_arrays(
		&surface_data, (RS::PrimitiveType)Mesh::PRIMITIVE_TRIANGLES, p_arr, 	// created data
		TypedArray<Array>(), Dictionary(), 0									// empty data
	);
	ERR_FAIL_COND(err != OK);

	RS::get_singleton()->mesh_clear(mesh_rid);
	RS::get_singleton()->mesh_add_surface(mesh_rid, surface_data);

	// for some reason, issues arise when used in constructor, use here
	// i think its caused by mesh_clear()
	RS::get_singleton()->instance_set_surface_override_material(RS_instance_rid, 0, material->get_rid());
}
*/






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
void Chunk::_generate_chunk_tangents(Vector3 &first_vertex, Vector3 &last_vertex) {
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




