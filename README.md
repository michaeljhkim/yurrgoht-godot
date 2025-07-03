# yurrgoht-godot
Procedurally generated 3D open world game (hopefully). 

The most progress so far has been made in the terrain generation system. Written in C++, it generates large terrain meshes outside of the main scene tree. Meshes are created from scratch—without helper classes like SurfaceTool, ArrayMesh, or even Godot nodes.

Several reasons required much of the processing to be done manually:
- The main scene tree does not support multi-threaded interactions.
- ArrayMesh, SurfaceTool, and parts of RenderingServer include unnecessary checks for this use case.
- Godot-specific object instantiations were computationally expensive—especially when running noise algorithms for every vertex.
- Chunk meshes had seam issues where lighting and textures were not continuous.

Despite this, much of the internal logic from the above classes was efficient and was forked for mesh generation. Below are some of the more notable features of the project:

## Multi-threaded mesh calculations
A custom ThreadManager struct was created to manage four threads. It assigns tasks based on thread workload or the number of tasks remaining.
Tasks are stored in a custom Queue class combining a HashMap and Vector. This structure enables fast existence checks to determine whether inserting a task is valid. These checks occur multiple times per frame to accommodate player movement during chunk generation.

## Manual Mesh Generation
The entire mesh—including the surface—was calculated manually. The following functions were forked and optimized:
- PlaneMesh::_create_mesh_array()
- SurfaceTool::index(), deindex(), generate_normals(), generate_tangents()
- RenderingServer::mesh_create_surface_data_from_arrays(), _surface_set_data()
Irrelevant members, validation checks, redundant copying, and unnecessary loops were removed to achieve significant performance improvements. Level of Detail (LOD) was also generated manually to control terrain complexity dynamically.

## Chunk Based Terrain
The terrain is split into smaller chunks, spawned based on the player’s position.
If a chunk moves beyond render distance, its mesh is cleared and added to a pool of reusable chunks. This reduces memory allocations and improves performance over time.