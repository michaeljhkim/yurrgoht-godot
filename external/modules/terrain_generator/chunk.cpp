#include "chunk.h"
#include "thirdparty/embree/kernels/bvh/bvh_statistics.h"

//typedef SurfaceTool::Vertex Vertex;

void Chunk::_notification(int p_what) {
	switch (p_what) {
		/*
		case NOTIFICATION_PROCESS:
			process(get_process_delta_time());
			break;
		*/

		case NOTIFICATION_READY:
			ready();
			break;

		/*
		case NOTIFICATION_EXIT_TREE: { 		// Thread must be disposed (or "joined"), for portability.
			if (_thread.is_valid()) _thread->wait_to_finish();	// Wait until it exits.

			_thread.unref();
		} break;
		*/
	}
}

void Chunk::ready() {
	//voxel_world = get_parent();

	// Should probably not use this since vertex calculations are on global scale - keeping for reference
	//set_position(Vector3(chunk_position * CHUNK_SIZE));

	_generate_chunk_mesh();
}


void Chunk::regenerate() {
	// Clear out all old nodes first.
	/*
	for (int c = 0; c < get_child_count(true); c++) {
		Node* child = get_child(c);
		remove_child(child);
		child->queue_free();
		//child.unref();
    }
	*/

	// Then generate new ones.
	//_generate_chunk_collider();
	_generate_chunk_mesh();
}


/*
- THIS IS SOMETHING I CAN DO MULTITHREADED SINCE IT IS 100% NOW JUST DONE BY ME

- Figure out a way to make it so that I do not need to create a new temporary array to extract vertices/normals
*/

/*
- MESH = {vertices, normals, uvs, tangents, indices}
 
- This function is used to generate the mesh of the chunk
- had to calculate MESH manually
- then added to an ArrayMesh for drawing 

- Why not use PlaneMesh with SurfaceTool?
- becauase by default, only the normals within that mesh is calculated
- this causes major issues with seaming, where there are texture and lighting inconsistencies

- The way to solve this issue is to generate the MESH as if the chunk had one extra row/column of vertices on all 4 sides
- then remove those extra MESH values.
- However, SurfaceTool provides no internal function that would do anyting remotely similar to this

- That is why all calculations had to be done manually - but significant portions of it was copied from PlaneMesh and SurfaceTool
- mostly copied: 
	PlaneMesh::_create_mesh_array(), 
	SurfaceTool::index(), 
	SurfaceTool::deindex(), 
	SurfaceTool::generate_normals(),
	SurfaceTool::generate_tangents()
- a few structs also had to be copied.
*/
void Chunk::_generate_chunk_mesh() {
	Array p_arr;
	p_arr.resize(Mesh::ARRAY_MAX);

	Vector2 size(CHUNK_SIZE, CHUNK_SIZE);	
	int subdivide_w = CHUNK_SIZE+1;
	int subdivide_d = CHUNK_SIZE+1;
	bool p_flip = false;

	int i, j, prevrow, thisrow, point;
	float x, z;

	//Size2 start_pos = size * -0.5;
	Size2 start_pos = size * -0.5 - Vector2(chunk_position.x, chunk_position.z) * CHUNK_SIZE;
	point = 0;

	/* top + bottom */
	z = start_pos.y;
	thisrow = point;
	prevrow = 0;

	for (j = 0; j <= (subdivide_d + 1); j++) {
		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float v = j;
			u /= subdivide_d;
			v /= subdivide_w;

			// Point orientation Y
			SurfaceTool::Vertex vert;
			float height = noise.get_noise_2d(-x, -z) * AMPLITUDE;
			vert.vertex = Vector3(-x, height, -z) + center_offset;

			// UVs
			vert.uv = Vector2(1.0 - u, 1.0 - v); /* 1.0 - uv to match orientation with Quad */
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

			x += size.x / (subdivide_w - 1.0);
		}
		z += size.y / (subdivide_d - 1.0);

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
	* any vertice that has a x/z value of either 0 or 3, will be removed:
	*
	* (1,1) (1,2)
	* (2,1) (2,2)
	*/
	Vector3 start_vertex = vertex_array[0].vertex;
	Vector3 last_vertex = vertex_array[vertex_array.size()-1].vertex;

	/*
	  ---------- DE-INDEX START ----------
	*/
	LocalVector<SurfaceTool::Vertex> old_vertex_array = vertex_array;
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

	// Normal Calculations
	AHashMap<SmoothGroupVertex, Vector3, SmoothGroupVertexHasher> smooth_hash = vertex_array.size();

	for (uint32_t vi = 0; vi < vertex_array.size(); vi += 3) {
		SurfaceTool::Vertex *v = &vertex_array[vi];

		Vector3 normal;
		!p_flip ?
			normal = Plane(v[0].vertex, v[1].vertex, v[2].vertex).normal:
			normal = Plane(v[2].vertex, v[1].vertex, v[0].vertex).normal;

		// Add face normal to smooth vertex influence if vertex is member of a smoothing group
		for (int i = 0; i < 3; i++) {
			if (v[i].smooth_group != UINT32_MAX) {
				Vector3 *lv = smooth_hash.getptr(v[i]);
				if (!lv) {
					smooth_hash.insert_new(v[i], normal);
				} else {
					(*lv) += normal;
				}
			} else {
				v[i].normal = normal;
			}
		}
	}

	for (SurfaceTool::Vertex &vertex : vertex_array) {
		if (vertex.smooth_group != UINT32_MAX) {
			Vector3 *lv = smooth_hash.getptr(vertex);
			!lv ?
				vertex.normal = Vector3():
				vertex.normal = lv->normalized(); 
		}
	}

	/*
	GENERATE TANGENTS
	*/
	_generate_chunk_tangents();


	/*
	  ---------- RE-INDEX START ----------
	*/
	//AHashMap<SurfaceTool::Vertex &, int, VertexHasher> indices = vertex_array.size();
	AHashMap<SurfaceTool::Vertex &, int, VertexHasher> indices;

	uint32_t new_size = 0;
	for (SurfaceTool::Vertex &vertex : vertex_array) {
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

	// Tangents are a little special, they have a 4th value called handedness - determines visibility direction
	PackedFloat32Array sub_tangent_array;
	sub_tangent_array.resize(vertex_array.size() * 4);
	float *w = sub_tangent_array.ptrw();

	for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
		const SurfaceTool::Vertex &v = vertex_array[idx];
		sub_vertex_array.push_back(v.vertex);
		sub_normal_array.push_back(v.normal);
		sub_uv_array.push_back(v.uv);

		// Tangent handedness calculations
		w[idx * 4 + 0] = v.tangent.x;
		w[idx * 4 + 1] = v.tangent.y;
		w[idx * 4 + 2] = v.tangent.z;
		float d = v.binormal.dot(v.normal.cross(v.tangent));
		w[idx * 4 + 3] = d < 0 ? -1 : 1;
	}

	// Unpacking indices into an array format that add_surface_from_arrays() accepts
	Vector<int> sub_index_array;
	for (int sub_index : index_array) {
		sub_index_array.push_back(sub_index);
	}

	p_arr[RS::ARRAY_VERTEX] = sub_vertex_array;
	p_arr[RS::ARRAY_NORMAL] = sub_normal_array;
	p_arr[RS::ARRAY_TANGENT] = sub_tangent_array;
	p_arr[RS::ARRAY_TEX_UV] = sub_uv_array;
	p_arr[RS::ARRAY_INDEX] = sub_index_array;
	

	// regular mesh does not have add_surface_from_arrays, but ArrayMesh is a child of Mesh, so it can be passed with set_mesh
	Ref<ArrayMesh> arr_mesh;
	arr_mesh.instantiate();
	arr_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, p_arr);

	set_mesh(arr_mesh);
}


void Chunk::_generate_chunk_normals() {
}


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
	for (SurfaceTool::Vertex &vertex : vertex_array) {
		vertex.binormal = Vector3();
		vertex.tangent = Vector3();
	}
	triangle_data.indices = &index_array;
	msc.m_pUserData = &triangle_data;

	bool res = genTangSpaceDefault(&msc);
	ERR_FAIL_COND(!res);
}




int Chunk::mikktGetNumFaces(const SMikkTSpaceContext *pContext) {
	TangentGenerationContextUserData &triangle_data = *reinterpret_cast<TangentGenerationContextUserData *>(pContext->m_pUserData);

	if (triangle_data.indices->size() > 0) {
		return triangle_data.indices->size() / 3;
	} else {
		return triangle_data.vertices->size() / 3;
	}
}

int Chunk::mikktGetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace) {
	return 3; //always 3
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
	SurfaceTool::Vertex *vtx = nullptr;
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


/*
Old code for generate_chunk_mesh() - keeping here for reference
*/
/*
    set_position(Vector3(chunk_position * CHUNK_SIZE));

	noise.set_noise_type(FastNoiseLite::TYPE_SIMPLEX);
	noise.set_frequency(0.008);
	noise.set_fractal_type(FastNoiseLite::FRACTAL_PING_PONG);
	noise.set_fractal_octaves(10);
	noise.set_fractal_gain(0.27);
	noise.set_fractal_ping_pong_strength(0.86);
	noise.set_cellular_distance_function(FastNoiseLite::DISTANCE_EUCLIDEAN_SQUARED);

	Ref<PlaneMesh> plane_mesh;
	plane_mesh.instantiate();
	plane_mesh->set_size(Vector2(CHUNK_SIZE, CHUNK_SIZE));
	plane_mesh->set_subdivide_width(CHUNK_SIZE);
	plane_mesh->set_subdivide_depth(CHUNK_SIZE);
	set_mesh(plane_mesh);

	//noise.set_offset(get_global_position());
	noise.set_offset(get_position());

	Ref<SurfaceTool> surface_tool;
	surface_tool.instantiate();
	surface_tool->clear();
	surface_tool->create_from(get_mesh(), 0);

	Ref<ArrayMesh> array_mesh;
	array_mesh.instantiate();
	array_mesh = surface_tool->commit();

	Ref<MeshDataTool> data_tool;
	data_tool.instantiate();
	data_tool->clear();
	data_tool->create_from_surface(array_mesh, 0);

	float vertex_count = data_tool->get_vertex_count();

	// due to how godots arrays work, the modified vertex must be applied in this manner
	for (int i = 0; i < vertex_count; i++) {
		Vector3 vertex = data_tool->get_vertex(i);
		//vertex.y = noise.get_noise_3d(vertex.x, vertex.y, vertex.z) * AMPLITUDE;
		float value = noise.get_noise_3d(vertex.x, vertex.y, vertex.z);
		vertex.y =  value * AMPLITUDE;
		data_tool->set_vertex(i, vertex);
	}
	array_mesh->clear_surfaces();
	data_tool->commit_to_surface(array_mesh);

	surface_tool->clear();
	surface_tool->begin(Mesh::PRIMITIVE_TRIANGLES);
	surface_tool->create_from(array_mesh, 0);
	surface_tool->generate_normals();

	// Set the material to the mesh
	set_mesh(surface_tool->commit());
	
	//print_line("success");

	plane_mesh.unref();
	surface_tool.unref();
	data_tool.unref();
	array_mesh.unref();

	return;
*/