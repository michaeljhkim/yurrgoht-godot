#ifndef CHUNK_H
#define CHUNK_H

#include "core/object/ref_counted.h"
#include "core/variant/variant_utility.h"
//#include "scene/resources/image_texture.h"

#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/3d/mesh_instance_3d.h"
//#include "scene/3d/physics/static_body_3d.h"
//#include "scene/resources/3d/primitive_meshes.h"
#include "core/templates/a_hash_map.h"
#include "scene/resources/surface_tool.h" // only need this for Vertex struct
#include "thirdparty/misc/mikktspace.h"

#include "core/core_bind.h"

class Chunk : public MeshInstance3D {
	GDCLASS(Chunk, MeshInstance3D);

private:
	Ref<core_bind::Thread> _thread;
    FastNoiseLite noise;
	Vector3 center_offset;

	LocalVector<SurfaceTool::Vertex> vertex_array;
	LocalVector<int> index_array;

protected:
    struct VertexHasher {
		static _FORCE_INLINE_ uint32_t hash(const SurfaceTool::Vertex &p_vtx) {
            uint32_t h = hash_djb2_buffer((const uint8_t *)p_vtx.bones.ptr(), p_vtx.bones.size() * sizeof(int));
            h = hash_djb2_buffer((const uint8_t *)p_vtx.weights.ptr(), p_vtx.weights.size() * sizeof(float), h);

            const int64_t length = (int64_t)&p_vtx.vertex - (int64_t)&p_vtx.smooth_group + sizeof(p_vtx.vertex);
            const void *key = &p_vtx.smooth_group;
            h = hash_murmur3_buffer(key, length, h);
            return h;
        }
	};

    struct SmoothGroupVertex {
		Vector3 vertex;
		uint32_t smooth_group = 0;
		bool operator==(const SmoothGroupVertex &p_vertex) const {
            if (vertex != p_vertex.vertex) 
                return false;
            if (smooth_group != p_vertex.smooth_group) 
                return false;
                
            return true;
        }

		SmoothGroupVertex(const SurfaceTool::Vertex &p_vertex) {
			vertex = p_vertex.vertex;
			smooth_group = p_vertex.smooth_group;
		}
	};

    struct SmoothGroupVertexHasher {
		static _FORCE_INLINE_ uint32_t hash(const SmoothGroupVertex &p_vtx) {
            uint32_t h = HashMapHasherDefault::hash(p_vtx.vertex);
            h = hash_murmur3_one_32(p_vtx.smooth_group, h);
            h = hash_fmix32(h);
            return h;
        }
    };

    //mikktspace callbacks
    struct TangentGenerationContextUserData {
        LocalVector<SurfaceTool::Vertex> *vertices;
        LocalVector<int> *indices;
    };

	void _notification(int p_notification);
	//void init();	// probably do not need this, ready should take care of everything
	void ready();
	void process(float delta);

    void regenerate();
    void _generate_chunk_collider();
    void _generate_chunk_mesh();

    //probably do not need this
    void _generate_chunk_normals();
    void _generate_chunk_tangents();

	//mikktspace callbacks
	static int mikktGetNumFaces(const SMikkTSpaceContext *pContext);
	static int mikktGetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace);
	static void mikktGetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert);
	static void mikktGetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert);
	static void mikktGetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert);
	static void mikktSetTSpaceDefault(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT,
			const tbool bIsOrientationPreserving, const int iFace, const int iVert);

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

    //void set_material(const Ref<Material> &new_material) { set_material_override(new_material); }


    /*
    Chunk();
    ~Chunk();
    */
};



#endif      //CHUNK_H