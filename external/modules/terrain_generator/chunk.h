#pragma once

#include "servers/rendering_server.h"
#include "core/object/ref_counted.h"
#include "core/math/aabb.h"

#include "modules/noise/fastnoise_lite.h"	// this class inherits from the noise class in noise.h
#include "scene/resources/mesh.h"

// For multi-threading
#include "core/core_bind.h"
#include "thirdparty/misc/mikktspace.h"
#include <atomic>

#include "scene/resources/3d/primitive_meshes.h"


class Chunk : public RefCounted {
	GDCLASS(Chunk, RefCounted);

private:
    // render server pointers
    RID render_instance_rid;
    RID mesh_rid;
    Ref<StandardMaterial3D> material;
    //Ref<ShaderMaterial> shader_material;
    //Ref<Shader> shader;
    //Ref<PlaneMesh> plane_mesh;

    Ref<FastNoiseLite> noise;

    Vector3 position = Vector3(0, 0, 0);
    Vector3 grid_position = Vector3(0, 0, 0);
    int LOD_factor = 0;

    std::atomic<bool> CHUNK_FLAGS[2] = {false};

protected:
    // Copied from SurfaceTool
    struct Vertex {
		// Trivial data for which the hash is computed using hash_buffer.
		// ----------------------------------------------------------------
		uint32_t smooth_group = 0; // Must be first.
        
		//Color color;
		Vector3 normal;     
		Vector3 binormal = Vector3();
		Vector3 tangent = Vector3();
		Vector2 uv;
		Vector2 uv2;
		Vector3 vertex;     // Must be last.
		// ----------------------------------------------------------------

        // return only if none of the conditions are true
		bool operator==(const Vertex &p_vertex) const {
            return (
                (vertex == p_vertex.vertex) &&
                (uv == p_vertex.uv) &&
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
	static void mikktSetTSpaceDefault(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fvBiTangent[], 
            const float fMagS, const float fMagT, const tbool bIsOrientationPreserving, const int iFace, const int iVert);


    /*
    ----- CHUNK SPECIFIC MEMBER VALUES -----
    */
	static void _bind_methods();

    // flattened vertices -> de-indexed vertices
    LocalVector<Vertex> flat_vertex_array;
	LocalVector<Vertex> vertex_array;
	LocalVector<int> index_array;
    HashMap<uint32_t, RS::SurfaceData> lod_surface_cache;

    // for storing neighboring lod chunks -> UNUSED
    enum ADJACENT {
        UP,
        DOWN,
        LEFT,
        RIGHT
    };
    int adjacent_LOD_steps[4] = {0};    // array only used to check if lod should be calculated -> UNUSED

    // mostly for keeping the mesh generation code clean
    void generate_normals(bool p_flip = false);
    void generate_tangents(Vector3 &first_vertex, Vector3 &last_vertex);

public:
    /*
    0.5 -> vertex count = size / 2
    1.0 -> vertex count = size
    2.0 -> vertex count = size * 2

    - 256 * 1.f is the highest I should go
    - 512 * 1.f and above for testing

    - 1024.f * 0.25f
        -> performs the same as 256 * 1.f (same vertex count)
        -> terrain is quite large, but performs smoothly
        -> lowest vertex count is 256 / 32 = 8 -> 8x8 = 64 vertices
    */
    static constexpr float CHUNK_RESOLUTION = 4.f;
    static constexpr float LOD_LIMIT = 8.0f;

    static constexpr float CHUNK_SIZE = 512.f;
    static constexpr float AMPLITUDE = 32.f;

    //currently not using these:
    static constexpr float TEXTURE_SHEET_WIDTH = 8;
    static constexpr int CHUNK_LAST_INDEX = CHUNK_SIZE - 1;
    static constexpr float TEXTURE_TILE_SIZE = 1.0 / TEXTURE_SHEET_WIDTH;

	// size should most likely be established elsewhere, but do not need to currently
    Vector2 size{CHUNK_SIZE, CHUNK_SIZE};

    enum FLAG : uint8_t {
        DELETE,
        UPDATE
    };
    void set_flag(FLAG flag, bool value) { CHUNK_FLAGS[flag].store(value, std::memory_order_acquire); }
    bool get_flag(FLAG flag) { return CHUNK_FLAGS[flag].load(std::memory_order_acquire); }

    void setup_reuse(int new_LOD, Vector3 new_position, Vector3 new_grid_pos) {
        LOD_factor = new_LOD;
        position = new_position;
        grid_position = new_grid_pos;
        generate_mesh();
    }
    void setup_update(int new_LOD, Vector3 new_grid_pos) {
        clear_data();
        LOD_factor = new_LOD;
        grid_position = new_grid_pos;
        generate_mesh();

        set_flag(Chunk::FLAG::UPDATE, false);
    }

    // for update checks
    float grid_distance(Vector3 new_grid_pos) {
        return grid_position.distance_to(new_grid_pos);
    }

	void set_material(Ref<Material> p_material) {
	    material = p_material;
    }

	void set_instance_material(Ref<Material> p_material) {
	    material = p_material;
	    RS::get_singleton()->instance_set_surface_override_material(render_instance_rid, 0, material->get_rid());
    }

    // can probably remove this, but keeping for now
    //void _generate_chunk_collider();
    //void _generate_lods(Vector2 size);
	//static constexpr float render_distance = 4.f;

    void generate_mesh();
    void draw_mesh();
    
    // scenario input is the return value of 'get_world_3d()->get_scenario()' from any node within the active main scene tree
    Chunk(RID scenario, int new_lod, Vector3 new_c_position, Vector3 new_grid_pos, Ref<Material> p_material);
    ~Chunk();

    void clear_data();
    void reset_data();
};