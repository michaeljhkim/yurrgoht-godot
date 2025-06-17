#ifndef CHUNK_H
#define CHUNK_H

#include <godot_cpp/classes/ref_counted.hpp>
//#include "core/variant/variant_utility.h"
//#include "core/variant/typed_dictionary.h"

#include <godot_cpp/classes/fast_noise_lite.hpp> // this class inherits from the noise class in noise.h

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/primitive_mesh.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/thread.hpp>


namespace godot {

class Chunk : public StaticBody3D {
	GDCLASS(Chunk, StaticBody3D);

private:
	Ref<Thread> _thread;
    FastNoiseLite noise;

protected:
	//void init();	// probably do not need this, ready should take care of everything

    void regenerate();
    void _generate_chunk_collider();
    void _generate_chunk_mesh();
    //void _draw_chunk_mesh(Ref<SurfaceTool> surface_tool, Vector3 vertex);

    //static Vector<Vector2> calculate_chunk_uvs(int chunk_id);
    //static Vector<Vector3> calculate_chunk_verts(Vector3i chunk_position);

	static void _bind_methods();

public:
    static constexpr int CHUNK_SIZE = 16; // Keep in sync with TerrainGenerator.
    static constexpr int TEXTURE_SHEET_WIDTH = 8;

    static constexpr int CHUNK_LAST_INDEX = CHUNK_SIZE - 1;
    static constexpr float TEXTURE_TILE_SIZE = 1.0 / TEXTURE_SHEET_WIDTH;

    static constexpr float AMPLITUDE = 16.0f;

    //HashMap<Vector3i, int> data;
    PackedVector3Array vertices;
    Vector3i chunk_position;

    //@onready var voxel_world = get_parent()
    Node* voxel_world;

    Ref<Thread> get_thread() { return _thread; }

    //Chunk();
    //~Chunk();

	void _ready() override;
	//void _process(double delta) override;
};

}

#endif      //CHUNK_H