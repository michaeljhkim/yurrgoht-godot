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
void Chunk::_generate_chunk_mesh() {
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
	
	//call_deferred("add_child", mi);
	//print_line("success");

	plane_mesh.unref();
	surface_tool.unref();
	array_mesh.unref();
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
