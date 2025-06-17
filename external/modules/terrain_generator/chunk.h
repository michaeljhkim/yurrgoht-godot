#ifndef CHUNK_H
#define CHUNK_H

#include "core/object/ref_counted.h"
#include "core/variant/variant_utility.h"
//#include "scene/resources/image_texture.h"

#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/surface_tool.h"

#include "core/core_bind.h"

class Chunk : public MeshInstance3D {
	GDCLASS(Chunk, MeshInstance3D);

private:
	Ref<core_bind::Thread> _thread;
    FastNoiseLite noise;
	Vector3 center_offset;

protected:
	void _notification(int p_notification);
	//void init();	// probably do not need this, ready should take care of everything
	void ready();
	void process(float delta);

    void regenerate();
    void _generate_chunk_collider();
    void _generate_chunk_mesh();

	static void _bind_methods();

public:
    static constexpr int CHUNK_SIZE = 16; // Keep in sync with TerrainGenerator.
    static constexpr int TEXTURE_SHEET_WIDTH = 8;

    static constexpr int CHUNK_LAST_INDEX = CHUNK_SIZE - 1;
    static constexpr float TEXTURE_TILE_SIZE = 1.0 / TEXTURE_SHEET_WIDTH;

    static constexpr float AMPLITUDE = 16.0f;

    PackedVector3Array vertices;
    Vector3i chunk_position;

    Node* voxel_world;

    Ref<core_bind::Thread> get_thread() { return _thread; }

    /*
    Chunk();
    ~Chunk();
    */
};



#endif      //CHUNK_H