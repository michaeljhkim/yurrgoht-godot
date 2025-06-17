#include "chunk.h"
#include "thirdparty/embree/kernels/bvh/bvh_statistics.h"

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
- Why did I not think of this before?
- Copy PlaneMesh mesh creation code instead of attempting to try and figure it out myself
- copy normal generation code from surface tool as well
- Surface tool will also no longer be needed since this needs to be done with arraymesh
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
	*/
	Array p_arr;
	p_arr.resize(Mesh::ARRAY_MAX);
	

	Vector2 size(CHUNK_SIZE, CHUNK_SIZE);	
	int subdivide_w = CHUNK_SIZE-1;
	int subdivide_d = CHUNK_SIZE-1;

	int i, j, prevrow, thisrow, point;
	float x, z;

	// Plane mesh can use default UV2 calculation as implemented in Primitive Mesh
	Size2 start_pos = size * -0.5;

	Vector3 normal = Vector3(0.0, 1.0, 0.0);

	Vector<Vector3> points;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	Vector<int> indices;
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

			// orientation Y
			points.push_back(Vector3(-x, 0.0, -z) + center_offset);
			normals.push_back(normal);
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0);

			uvs.push_back(Vector2(1.0 - u, 1.0 - v)); /* 1.0 - uv to match orientation with Quad */
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			}

			x += size.x / (subdivide_w + 1.0);
		}

		z += size.y / (subdivide_d + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	p_arr[RS::ARRAY_INDEX] = indices;

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
