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

		Color color;
		Vector3 normal; // normal, binormal, tangent.
		Vector3 binormal;
		Vector3 tangent;
		Vector2 uv;
		Vector2 uv2;
		Color custom[RS::ARRAY_CUSTOM_COUNT];

		Vector3 vertex; // Must be last.
		// ----------------------------------------------------------------

		Vector<int> bones;
		Vector<float> weights;

		bool operator==(const Vertex &p_vertex) const {
            if ((vertex != p_vertex.vertex) ||
                (uv != p_vertex.uv) ||
                (uv2 != p_vertex.uv2) ||
                (normal != p_vertex.normal) ||
                (binormal != p_vertex.binormal) ||
                (tangent != p_vertex.tangent) ||
                (color != p_vertex.color) ||
                (bones.size() != p_vertex.bones.size()) 
            ) {
                return false;
            }

            for (int i = 0; i < bones.size(); i++) {
                if (bones[i] != p_vertex.bones[i])
                    return false;
            }

            for (int i = 0; i < weights.size(); i++) {
                if (weights[i] != p_vertex.weights[i])
                    return false;
            }

            for (int i = 0; i < RS::ARRAY_CUSTOM_COUNT; i++) {
                if (custom[i] != p_vertex.custom[i])
                    return false;
            }

            if (smooth_group != p_vertex.smooth_group) {
                return false;
            }

            return true;
        }

		Vertex() {}
	};

    struct VertexHasher {
		static _FORCE_INLINE_ uint32_t hash(const Vertex &p_vtx) {
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

		SmoothGroupVertex(const Vertex &p_vertex) {
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
    
	static void _bind_methods();

	LocalVector<Vertex> vertex_array;
	LocalVector<int> index_array;
	RS::SurfaceData surface_data;

    Vector3 chunk_position = Vector3(0, 0, 0);
    int chunk_LOD = 1;
    
    // for storing neighboring lod chunks
    enum ADJACENT {
        UP,
        DOWN,
        LEFT,
        RIGHT
    };
    int adjacent_LOD_steps[4] = {0};    // array only used to check if lod should be calculated

    std::atomic_bool CHUNK_FLAGS[2] = {false};

public:
    enum FLAG {
        DELETE,
        UPDATE
    };

    void set_flag(FLAG flag, bool set_value) { CHUNK_FLAGS[flag].store(set_value, std::memory_order_acquire); }
    bool get_flag(FLAG flag) { return CHUNK_FLAGS[flag].load(); }

    void _set_chunk_LOD(int new_LOD) { chunk_LOD = new_LOD; }
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


	static constexpr float render_distance = 4;
    static constexpr float CHUNK_SIZE = 256;    // chunk_size of 64 is pretty fast - 128 and above for testing
    static constexpr float AMPLITUDE = 32.0f;

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