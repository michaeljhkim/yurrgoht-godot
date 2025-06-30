#pragma once

// GODOT DARED ME TO OPTIMIZE! HAHAHHAHAH
// I AM WILLING TO GO AS FAR AS POSSIBLE!!!!
// https://docs.godotengine.org/en/stable/tutorials/performance/using_servers.html
#include "servers/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"

#include "core/object/ref_counted.h"
#include "core/variant/variant_utility.h"
#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h

#include "scene/resources/mesh.h"

// For multi-threading
#include "core/core_bind.h"
#include "thirdparty/misc/mikktspace.h"

#include <atomic>

class Chunk : public RefCounted {
	GDCLASS(Chunk, RefCounted);

private:
    // render server pointers
    RID RS_instance_rid;
    mutable RID mesh_rid;
    //RID world_scenario;

    // possibly need in the future
	//Vector<Surface> surfaces;
	//Mesh::BlendShapeMode blend_shape_mode = Mesh::BlendShapeMode::BLEND_SHAPE_MODE_RELATIVE;
	//Vector<StringName> blend_shapes;
    Ref<StandardMaterial3D> material;
    
	Array p_arr;
    Ref<FastNoiseLite> noise;

protected:

    // Copied from SurfaceTool
    struct Vertex {
		// Trivial data for which the hash is computed using hash_buffer.
		// ----------------------------------------------------------------
		uint32_t smooth_group = 0; // Must be first.

		Vector3 normal; // normal, binormal, tangent.
		Vector3 binormal = Vector3();
		Vector3 tangent = Vector3();
		Vector2 uv;
		//Vector2 uv2;

		Vector3 vertex; // Must be last.
		// ----------------------------------------------------------------

        // return only if none of the conditions are true
		bool operator==(const Vertex &p_vertex) const {
            return (
                (vertex == p_vertex.vertex) &&
                (uv == p_vertex.uv) &&
                //(uv2 == p_vertex.uv2) &&
                (normal == p_vertex.normal) &&
                (binormal == p_vertex.binormal) &&
                (tangent == p_vertex.tangent) &&
                (smooth_group == p_vertex.smooth_group)
            );
        }

		Vertex() {}
	};

    struct VertexHasher {
		static _FORCE_INLINE_ uint32_t hash(const Vertex &p_vtx) {
            const int64_t length = (int64_t)&p_vtx.vertex - (int64_t)&p_vtx.smooth_group + sizeof(p_vtx.vertex);
            const void *key = &p_vtx.smooth_group;
            uint32_t h = hash_murmur3_buffer(key, length);
            return h;
        }
	};

    struct SmoothGroupVertex {
		Vector3 vertex;
		uint32_t smooth_group = 0;
		bool operator==(const SmoothGroupVertex &p_vertex) const {
            return (vertex == p_vertex.vertex) && (smooth_group == p_vertex.smooth_group); 
        }

		SmoothGroupVertex(const Vertex &p_vertex) : vertex(p_vertex.vertex), smooth_group(p_vertex.smooth_group) {}
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
        LocalVector<Vertex> *vertices;
        LocalVector<int> *indices;
    };

	//mikktspace callbacks
	static int mikktGetNumFaces(const SMikkTSpaceContext *pContext);
	static int mikktGetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace);
	static void mikktGetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert);
	static void mikktGetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert);
	static void mikktGetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert);
	static void mikktSetTSpaceDefault(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT,
			const tbool bIsOrientationPreserving, const int iFace, const int iVert);


    /*
    ----- CHUNK SPECIFIC MEMBER VALUES -----
    */
    
    // FOR CACHE
    struct LODMeshData {
        LocalVector<Vertex> vertex_array;
        LocalVector<int> index_array;
        RS::SurfaceData surface_data;
    };

    /*
    TODO IDEA:
    - I know for a fact that a given chunk can be updated multiple times in rapid succession because the player might move back and forth
    - instead of removing old lod data, and then regenerating, we can just store the old data until the delete flag is raised
    - since the chunk will be completely removed, there is no point holding all that data, and only then are we sure we can delete it
    */


	static void _bind_methods();

	LocalVector<Vertex> vertex_array;
	LocalVector<int> index_array;
	RS::SurfaceData surface_data;

    Vector3 chunk_position = Vector3(0, 0, 0);
    int chunk_LOD = 1;
    
    // for storing neighboring lod chunks -> UNUSED
    enum ADJACENT {
        UP,
        DOWN,
        LEFT,
        RIGHT
    };
    int adjacent_LOD_steps[4] = {0};    // array only used to check if lod should be calculated -> UNUSED

    std::atomic_bool CHUNK_FLAGS[2] = {false};

public:
    enum FLAG {
        DELETE,
        UPDATE
    };

    void set_flag(FLAG flag, bool set_value) { CHUNK_FLAGS[flag].store(set_value, std::memory_order_acquire); }
    bool get_flag(FLAG flag) { return CHUNK_FLAGS[flag].load(); }

    void _set_chunk_LOD(int new_LOD) { chunk_LOD = MAX(new_LOD, 1.0); }
    int _get_chunk_LOD() { return chunk_LOD; }

    void _set_chunk_position(Vector3 new_position) { chunk_position = new_position; }
    Vector3 _get_chunk_position() { return chunk_position; }


    // can probably remove this, but keeping for now
    //void _generate_chunk_collider();
    //RID _get_mesh_rid() { return mesh_rid; }
    //void _generate_lods(Vector2 size);

    void _generate_chunk_mesh();
    void _draw_mesh();
    
    // mostly for keeping the mesh generation code clean
    void _generate_chunk_normals(bool p_flip = false);
    void _generate_chunk_tangents();


	//static constexpr float render_distance = 4.f;

    /*
    0.5 -> vertex count = size / 2
    1.0 -> vertex count = size
    2.0 -> vertex count = size * 2
    */
    static constexpr float CHUNK_RESOLUTION = 1.f;

    static constexpr float CHUNK_SIZE = 256.f;    // chunk_size of 64 is pretty fast - 128 and above for testing
    static constexpr float AMPLITUDE = 32.f;


    //currently not using these:
    static constexpr float TEXTURE_SHEET_WIDTH = 8;
    static constexpr int CHUNK_LAST_INDEX = CHUNK_SIZE - 1;
    static constexpr float TEXTURE_TILE_SIZE = 1.0 / TEXTURE_SHEET_WIDTH;
    
    // scenario input is the return value of 'get_world_3d()->get_scenario()' from any node within the active main scene tree
    Chunk(RID scenario, Vector3 new_c_position, int new_lod);
    ~Chunk();

    void _clear_chunk_data();
    void _reset_chunk_data();
};