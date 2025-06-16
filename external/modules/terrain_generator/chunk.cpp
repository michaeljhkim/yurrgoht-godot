#include "chunk.h"
#include "thirdparty/embree/kernels/bvh/bvh_statistics.h"

void Chunk::_notification(int p_what) {
	switch (p_what) {
		/*
		case NOTIFICATION_PROCESS:
			process(get_process_delta_time());
			break;
		*/

		case NOTIFICATION_READY & NOTIFICATION_SCENE_INSTANTIATED:
			//ready();
			break;
	}
}

void Chunk::ready() {
    // IDK why the demo didn't do this in ready as well
	voxel_world = get_parent();

    set_position(Vector3(chunk_position * CHUNK_SIZE));
    set_name(stringify_variants(chunk_position));

	for (int x = 0; x <= CHUNK_SIZE; x++) {
		for (int z = 0; z <= CHUNK_SIZE; z++) {
			vertices.push_back(Vector3(x, 0, z));
		}
	}

	// We can only add colliders in the main thread due to physics limitations.
	//_generate_chunk_collider();
	
    // However, we can use a thread for mesh generation.
	_thread.instantiate();
	_thread->start(callable_mp(this, &Chunk::_generate_chunk_mesh), core_bind::Thread::PRIORITY_NORMAL);
}


void Chunk::regenerate() {
	// Clear out all old nodes first.
	for (int c = 0; c < get_child_count(true); c++) {
		Node* child = get_child(c);
		remove_child(child);
		child->queue_free();
		//child.unref();
    }

	// Then generate new ones.
	//_generate_chunk_collider();
	_generate_chunk_mesh();
}

/*
void _generate_chunk_collider() {
	if vertices.is_empty():
		# Avoid errors caused by StaticBody3D not having colliders.
		_create_block_collider(Vector3.ZERO)
		collision_layer = 0
		collision_mask = 0
		return

	# For each block, generate a collider. Ensure collision layers are enabled.
	collision_layer = 0xFFFFF
	collision_mask = 0xFFFFF
	for block_position in vertices.keys():
		var chunk_id = vertices[block_position]
		if chunk_id != 27 and chunk_id != 28:
			_create_block_collider(block_position)
}
*/


void Chunk::_generate_chunk_mesh() {
	if (vertices.is_empty())
		return;

	Ref<SurfaceTool> surface_tool;
	surface_tool.instantiate();
	surface_tool->begin(Mesh::PRIMITIVE_TRIANGLES);

	// For each block, add vertices to the SurfaceTool and generate a collider.

	// Due to godots custom Vector type, single vertex values must be set in this manner
	for (int i = 0; i < vertices.size(); i++) {
		Vector3 vertex = vertices.get(i);
		vertex.y = noise.get_noise_2d(get_global_position().x + vertices[i].x, get_global_position().z + vertices[i].z) * AMPLITUDE;
		vertices.set(i, vertex);

		surface_tool->set_uv(Vector2(vertex.x/CHUNK_SIZE, vertex.y/CHUNK_SIZE )); 
		surface_tool->add_vertex(vertex);
	}

	// Create the chunk's mesh from the SurfaceTool vertices.
	surface_tool->generate_normals();
	surface_tool->generate_tangents();
	surface_tool->index();

	Ref<ArrayMesh> array_mesh = surface_tool->commit();
	
	MeshInstance3D* mi = memnew(MeshInstance3D());
	mi->set_mesh(array_mesh);
	//mi.set_material_override(preload("res://world/textures/material.tres"));
	
	call_deferred("add_child", mi);
}

// THIS IS THE NEXT THING TO WORK ON!!!!
/*
void Chunk::_draw_chunk_mesh(Ref<SurfaceTool> surface_tool, Vector3 vertex) {
}

static Vector<Vector2> calculate_chunk_uvs(int chunk_id) {
}

static Vector<Vector3> calculate_chunk_verts(Vector3i chunk_position) {
}
*/



void Chunk::_bind_methods() {
	//ClassDB::bind_method(D_METHOD("add", "value"), &TerrainGenerator::add);
	//ClassDB::bind_method(D_METHOD("reset"), &TerrainGenerator::reset);
	//ClassDB::bind_method(D_METHOD("get_total"), &TerrainGenerator::get_total);

	//ClassDB::bind_method(D_METHOD("_generate_chunk_mesh"), &Chunk::_generate_chunk_mesh);

	//ClassDB::bind_method(D_METHOD("set_player_character", "p_node"), &TerrainGenerator::set_player_character);
	//ClassDB::bind_method(D_METHOD("get_player_character"), &TerrainGenerator::get_player_character);
	
	//ClassDB::add_property("TerrainGenerator", PropertyInfo(Variant::OBJECT, "player_character", PROPERTY_HINT_NODE_TYPE, "Node3D"), "set_player_character", "get_player_character");
}
