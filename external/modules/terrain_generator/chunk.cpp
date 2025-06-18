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


// FORGOT TO ADD A MATERIAL - THATS WHY I CANT SEE ANYTHING

/*
- THIS IS SOMETHING I CAN DO MULTITHREADED SINCE IT IS 100% NOW JUST DONE BY ME
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
	int subdivide_w = CHUNK_SIZE-1;
	int subdivide_d = CHUNK_SIZE-1;
	bool p_flip = false;

	int i, j, prevrow, thisrow, point;
	float x, z;

	// Plane mesh can use default UV2 calculation as implemented in Primitive Mesh
	Size2 start_pos = size * -0.5;

	//Vector3 normal = Vector3(0.0, 1.0, 0.0);

	LocalVector<SurfaceTool::Vertex> vertex_array;
	//Vector<Vector3> normal_array;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	LocalVector<int> index_array;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	/* top + bottom */
	z = start_pos.y;
	thisrow = point;
	prevrow = 0;

	for (j = 0; j <= (subdivide_d + 1); j++) {
		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float v = j;
			u /= (subdivide_w + 1.0);
			v /= (subdivide_d + 1.0);

			// Point orientation Y
			SurfaceTool::Vertex vert;
			vert.vertex = Vector3(-x, noise.get_noise_2d(get_global_position().x-x, get_global_position().z-z)*AMPLITUDE, -z) + center_offset;
			vertex_array.push_back(vert);

			// Normal
			//normal_array.push_back(normal);

			// Tangent
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0);

			// UVs
			uvs.push_back(Vector2(1.0 - u, 1.0 - v)); /* 1.0 - uv to match orientation with Quad */
			point++;

			if (i > 0 && j > 0) {
				index_array.push_back(prevrow + i - 1);
				index_array.push_back(prevrow + i);
				index_array.push_back(thisrow + i - 1);
				index_array.push_back(prevrow + i);
				index_array.push_back(thisrow + i);
				index_array.push_back(thisrow + i - 1);
			}

			x += size.x / (subdivide_w + 1.0);
		}

		z += size.y / (subdivide_d + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}

	// adding normal_array
	// ----- deindex start -----
	LocalVector<SurfaceTool::Vertex> old_vertex_array = vertex_array;
	vertex_array.clear();
	for (const int &index : index_array) {
		ERR_FAIL_COND(uint32_t(index) >= old_vertex_array.size());
		vertex_array.push_back(old_vertex_array[index]);
	}
	index_array.clear();
	// ----- deindex end -----

	AHashMap<SmoothGroupVertex, Vector3, SmoothGroupVertexHasher> smooth_hash = vertex_array.size();

	for (uint32_t vi = 0; vi < vertex_array.size(); vi += 3) {
		SurfaceTool::Vertex *v = &vertex_array[vi];

		Vector3 normal;
		if (!p_flip) {
			normal = Plane(v[0].vertex, v[1].vertex, v[2].vertex).normal;
		} else {
			normal = Plane(v[2].vertex, v[1].vertex, v[0].vertex).normal;
		}

		for (int i = 0; i < 3; i++) {
			// Add face normal to smooth vertex influence if vertex is member of a smoothing group
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
			if (!lv) {
				vertex.normal = Vector3();
			} else {
				vertex.normal = lv->normalized();
			}
		}
	}

	print_line("before index");
	print_line(vertex_array.size());

	// ----- reindex start -----
	AHashMap<SurfaceTool::Vertex &, int, VertexHasher> indices = vertex_array.size();

	uint32_t new_size = 0;
	for (SurfaceTool::Vertex &vertex : vertex_array) {
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
	// ----- reindex end -----

	print_line("after index");
	print_line(vertex_array.size());

	PackedVector3Array point_array;
	PackedVector3Array normal_array;
	for (SurfaceTool::Vertex p : vertex_array) {
		point_array.push_back(p.vertex);
		normal_array.push_back(p.normal);
	}


	Vector<int> new_index_array;
	for (int in : index_array) {
		new_index_array.push_back(in);
	}

	/*
	print_line(point_array.size());
	print_line(normal_array.size());
	print_line(index_array.size());
	*/

	p_arr[RS::ARRAY_VERTEX] = point_array;
	p_arr[RS::ARRAY_NORMAL] = normal_array;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	p_arr[RS::ARRAY_INDEX] = new_index_array;

	// regular mesh does not have add_surface_from_arrays, but ArrayMesh is a child of Mesh, so it can be passed with set_mesh
	Ref<ArrayMesh> arr_mesh;
	arr_mesh.instantiate();
	arr_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, p_arr);

	set_mesh(arr_mesh);
}


void Chunk::_bind_methods() {
	//ClassDB::bind_method(D_METHOD("add", "value"), &TerrainGenerator::add);
	//ClassDB::bind_method(D_METHOD("reset"), &TerrainGenerator::reset);
	//ClassDB::bind_method(D_METHOD("get_total"), &TerrainGenerator::get_total);

	//ClassDB::bind_method(D_METHOD("_generate_chunk_mesh"), &Chunk::_generate_chunk_mesh);

	//ClassDB::bind_method(D_METHOD("set_player_character", "p_node"), &TerrainGenerator::set_player_character);
	//ClassDB::bind_method(D_METHOD("get_player_character"), &TerrainGenerator::get_player_character);
	
	//ClassDB::add_property("TerrainGenerator", PropertyInfo(Variant::OBJECT, "player_character", PROPERTY_HINT_NODE_TYPE, "Node3D"), "set_player_character", "get_player_character");
}
