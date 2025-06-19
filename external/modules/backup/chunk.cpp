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

    set_position(Vector3(chunk_position * CHUNK_SIZE));
    set_name(VariantUtilityFunctions::var_to_str(chunk_position)); 

	// TEMPORARY!!!
	set_ignore_occlusion_culling(true);

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

- Done with figuring out normals, I just need to figure out tangents now

- also after this, figure out a way to make it so that I do not need to create a new temporary array to extract vertices/normals
*/
void Chunk::_generate_chunk_mesh() {
	/*
	Ref<PlaneMesh> plane_mesh;
	plane_mesh.instantiate();
	plane_mesh->set_size(Vector2(CHUNK_SIZE, CHUNK_SIZE));
	plane_mesh->set_subdivide_width(CHUNK_SIZE-1);
	plane_mesh->set_subdivide_depth(CHUNK_SIZE-1);

	Ref<SurfaceTool> surface_tool;
	surface_tool.instantiate();
	surface_tool->create_from(plane_mesh, 0);
	Array data = surface_tool->commit_to_arrays();
	PackedVector3Array vertices = data[ArrayMesh::ARRAY_VERTEX];

	// due to how godots arrays work, the modified vertex must be applied in this manner
	for (int i = 0; i < vertices.size(); i++) {
		Vector3 vertex = vertices[i];
		vertex.y = noise.get_noise_2d(get_global_position().x + vertex.x, get_global_position().z + vertex.z) * AMPLITUDE;
		vertices.set(i, vertex);
	}
	data[ArrayMesh::ARRAY_VERTEX] = vertices;

	Ref<ArrayMesh> array_mesh;
	array_mesh.instantiate();
	array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, data);
	
	surface_tool->create_from(array_mesh, 0);
	surface_tool->generate_normals();

	// Set the material to the mesh
	set_mesh(surface_tool->commit());
	
	//print_line("success");

	plane_mesh.unref();
	surface_tool.unref();
	array_mesh.unref();
	return;
	*/



	Array p_arr;
	p_arr.resize(Mesh::ARRAY_MAX);


	Vector2 size(CHUNK_SIZE, CHUNK_SIZE);	
	int subdivide_w = CHUNK_SIZE+1;
	int subdivide_d = CHUNK_SIZE+1;
	bool p_flip = false;

	int i, j, prevrow, thisrow, point;
	float x, z;


	/*
	int int_subdivide_d = subdivide_d + 1;
	int int_subdivide_w = subdivide_w + 1;
	float float_subdivide_d = subdivide_d + 1.0f;
	float float_subdivide_w = subdivide_w + 1.0f;
	*/
	
	/*
	int int_subdivide_d = subdivide_d + 1 + 1;
	int int_subdivide_w = subdivide_w + 1 + 1;
	float float_subdivide_d = subdivide_d + 1.0f;
	float float_subdivide_w = subdivide_w + 1.0f;
	*/

	float step_x = size.x / (subdivide_w + 1.0);
	float step_z = size.y / (subdivide_d + 1.0);

	// Plane mesh can use default UV2 calculation as implemented in Primitive Mesh
	Size2 start_pos = size * -0.5 - Vector2(step_x, step_z);
	//Size2 start_pos = size * -0.5;
	point = 0;

	/*
	Vector<float> tangents;
#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);
	*/

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
			float height = noise.get_noise_2d(get_global_position().x-x, get_global_position().z-z) * AMPLITUDE;
			//float height = 0.0;
			vert.vertex = Vector3(-x, height, -z) + center_offset;

			// Tangent
			//ADD_TANGENT(1.0, 0.0, 0.0, 1.0);

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


				/*
				index_array.push_back(i + j     * (subdivide_w + 1 + 2));
				index_array.push_back(i + (j+1) * (subdivide_w + 1 + 2));
				index_array.push_back(i + j     * (subdivide_w + 1 + 2) + 1);

				index_array.push_back(i + (j+1) * (subdivide_w + 1 + 2));
				index_array.push_back(i + (j+1) * (subdivide_w + 1 + 2) + 1);
				index_array.push_back(i + j     * (subdivide_w + 1 + 2) + 1);
				*/
			}

			x += size.x / (subdivide_w + 1.0);
		}

		z += size.y / (subdivide_d + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}
	Vector3 last_vertex = vertex_array[vertex_array.size()-1].vertex;

	// ----- deindex start -----
	LocalVector<SurfaceTool::Vertex> old_vertex_array = vertex_array;
	vertex_array.clear();
	// There are 6 indices per vertex
	for (const int &index : index_array) {
		//print_line("index size: ", uint32_t(index));
		//print_line("old_vertex_array size: ", old_vertex_array.size());
		ERR_FAIL_COND(uint32_t(index) >= old_vertex_array.size());
		vertex_array.push_back(old_vertex_array[index]);
	}
	index_array.clear();
	// ----- deindex end -----

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

	//print_line("before index");
	//print_line(vertex_array.size());

	// ----- reindex start -----
	// explain the numbers later
	//uint32_t first_index = vertex_array.size() - (subdivide_w + (subdivide_w - 1) + ((subdivide_d - 1) * 2));
	//AHashMap<SurfaceTool::Vertex &, int, VertexHasher> indices = first_index;
	AHashMap<SurfaceTool::Vertex &, int, VertexHasher> indices = vertex_array.size();
	//print_line("removed extra rows/volumns: ", first_index);

	uint32_t new_size = 0;
	for (SurfaceTool::Vertex &vertex : vertex_array) {
		// remove 1 row/column from all 4 sides, since those were extras generated purely for removing seaming between chunks
		/*
		if ((vertex.vertex.x == start_pos.x) ||
			(vertex.vertex.z == start_pos.y) ||
			(vertex.vertex.x == last_vertex.x) ||
			(vertex.vertex.z == last_vertex.z)) {
			
			continue;
		}
		*/

		int *idxptr = indices.getptr(vertex);
		int idx;
		if (!idxptr) {
			idx = indices.size();
			vertex_array[new_size] = vertex;
			indices.insert_new(vertex_array[new_size], idx);
			new_size++;

			//ADD_TANGENT(1.0, 0.0, 0.0, 1.0);

		} else {
			idx = *idxptr;
		}

		index_array.push_back(idx);
	}

	vertex_array.resize(new_size);
	//print_line("resize after index: ", vertex_array.size());

	// ----- reindex end -----
	// This is also the end of normal calculations and refactoring - time for tangents

	//print_line("after index");
	//print_line(vertex_array.size());

	PackedVector3Array point_array;
	PackedVector3Array normal_array;
	PackedVector2Array uv_array;

	PackedFloat32Array tangent_array;
	tangent_array.resize(vertex_array.size() * 4);
	float *w = tangent_array.ptrw();

	for (uint32_t idx = 0; idx < vertex_array.size(); idx++) {
		const SurfaceTool::Vertex &v = vertex_array[idx];
		point_array.push_back(v.vertex);
		normal_array.push_back(v.normal);
		uv_array.push_back(v.uv);

		//Tangent calculations
		w[idx * 4 + 0] = v.tangent.x;
		w[idx * 4 + 1] = v.tangent.y;
		w[idx * 4 + 2] = v.tangent.z;
		//float d = v.tangent.dot(v.binormal,v.normal);
		float d = v.binormal.dot(v.normal.cross(v.tangent));
		w[idx * 4 + 3] = d < 0 ? -1 : 1;

		/*
		Vector3 n = v.normal.normalized();
		Vector3 t = v.tangent.normalized();
		Vector3 b = v.binormal.normalized();

		float w_sign = b.dot(n.cross(t)) < 0.0f ? -1.0f : 1.0f;

		w[idx * 4 + 0] = t.x;
		w[idx * 4 + 1] = t.y;
		w[idx * 4 + 2] = t.z;
		w[idx * 4 + 3] = w_sign;
		*/
	}

	Vector<int> new_index_array;
	for (int in : index_array) {
		new_index_array.push_back(in);
	}

	p_arr[RS::ARRAY_VERTEX] = point_array;
	p_arr[RS::ARRAY_NORMAL] = normal_array;
	p_arr[RS::ARRAY_TANGENT] = tangent_array;
	//p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uv_array;
	p_arr[RS::ARRAY_INDEX] = new_index_array;

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
		//vtx->binormal = Vector3(fvBiTangent[0], fvBiTangent[1], fvBiTangent[2]);
	}
}


void Chunk::_bind_methods() {
}
