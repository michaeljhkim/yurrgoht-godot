#include "terrain_generator.h"

void TerrainGenerator::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			process(get_process_delta_time());
			break;

		case NOTIFICATION_READY:
			ready();
			break;

		case NOTIFICATION_EXIT_TREE: { 		// Thread must be disposed (or "joined"), for portability.
			//clean_up();
		} break;
	}
}

// Connect to generator and start mesh generation
void TerrainGenerator::ready() {
	// Find generator
	if (!generatorNode.is_empty())  {
		generator = get_node(generatorNode);
		// Check if generator has value and height functions
		if (generator->has_method("get_value")) {
			generatorHasValueFunc = true;
		}
		if (generator->has_method("get_height")) {
			generatorHasHeightFunc = true;
		}
	}
	// Find player node
	if (!playerNode.is_empty()) {
		player = get_node(playerNode);
	}
	// Create mutex
	mutex.instantiate();
	// Create thread
	thread.instantiate();
	thread->start(map_update().bind(0));
}



/*
- Utility Functions
*/

// Get value for each trig in cell using marhing squares
PackedInt64Array TerrainGenerator::get_trigs_marching(PackedInt64Array corners) {
	// Case 1
	// |1 1|
	// |1 1|
	if ((corners[0] == corners[1]) && 
		(corners[0] == corners[2]) &&
		(corners[0] == corners[3])
	) {
		return PackedInt64Array{
			corners[0], corners[0], corners[0], corners[0], 
			corners[0], corners[0], corners[0], corners[0]
		};
	}
	// Case 2
	// |1 2|
	// |1 1|
	else if ((corners[0] == corners[2]) && 
			(corners[0] == corners[3]) &&
			(corners[0] != corners[1])
	) {
		return PackedInt64Array{
			corners[0],corners[0],corners[0],corners[1],
			corners[0],corners[0],corners[0],corners[0]
		};
	}
	
	// Case 3
	// |2 1|
	// |1 1|
	else if ((corners[1] == corners[2]) && 
			(corners[1] == corners[3]) &&
			(corners[1] != corners[0])
	) {
		return PackedInt64Array{
			corners[0],corners[1],corners[1],corners[1],
			corners[1],corners[1],corners[1],corners[1]
		};
	}

	// Case 4
	// |2 2|
	// |1 1|
	else if ((corners[0] == corners[1]) && 
			(corners[0] != corners[2]) &&
			(corners[2] == corners[3])
	) {
		return PackedInt64Array{
			corners[0],corners[0],corners[0],corners[0],
			corners[2],corners[2],corners[2],corners[2]
		};
	}

	// Case 5
	// |2 3|
	// |1 1|
	else if ((corners[0] != corners[1]) && 
			(corners[0] != corners[2]) &&
			(corners[2] == corners[3])
	) {
		return PackedInt64Array{
			corners[0],corners[2],corners[2],corners[1],
			corners[2],corners[2],corners[2],corners[2]
		};
	}

	// Case 6
	// |1 1|
	// |1 2|
	else if ((corners[0] == corners[1]) && 
			(corners[0] == corners[2]) &&
			(corners[2] != corners[3])
	) {
		return PackedInt64Array{
			corners[0],corners[0],corners[0],corners[0],
			corners[0],corners[0],corners[0],corners[3]
		};
	}

	// Case 7
	// |1 2|
	// |1 2|
	else if ((corners[0] == corners[2]) && 
			(corners[0] != corners[1]) &&
			(corners[1] == corners[3])
	) {
		return PackedInt64Array{
			corners[0],corners[0],corners[1],corners[1],
			corners[0],corners[0],corners[1],corners[1]
		};
	}

	// Case 8
	// |1 2|
	// |1 3|
	else if ((corners[0] == corners[2]) && 
			(corners[0] != corners[1]) &&
			(corners[0] != corners[3]) &&
			(corners[1] != corners[3])
	) {
		return PackedInt64Array{
			corners[0],corners[0],corners[0],corners[1],
			corners[0],corners[0],corners[0],corners[3]
		};
	}

	// Case 9
	// |1 2|
	// |2 1|
	else if ((corners[0] == corners[3]) && 
			(corners[0] != corners[1]) &&
			(corners[0] != corners[2]) &&
			(corners[1] == corners[2])
	) {
		return PackedInt64Array{
			corners[0],corners[0],corners[0],corners[1],
			corners[1],corners[0],corners[0],corners[0]
		};
	}

	// Case 10
	// |1 1|
	// |2 1|
	else if ((corners[0] == corners[1]) && 
			(corners[0] == corners[3]) &&
			(corners[0] != corners[2])
	) {
		return PackedInt64Array{
			corners[0],corners[0],corners[0],corners[0],
			corners[2],corners[0],corners[0],corners[0]
		};
	}

	// Case 11
	// |1 2|
	// |3 1|
	else if ((corners[0] == corners[3]) && 
			(corners[0] != corners[1]) &&
			(corners[0] != corners[2]) &&
			(corners[1] != corners[2])
	) {
		return PackedInt64Array{
			corners[0],corners[0],corners[0],corners[1],
			corners[2],corners[0],corners[0],corners[0]
		};
	}

	// Case 12
	// |2 1|
	// |1 3|
	else if ((corners[1] == corners[2]) && 
			(corners[0] != corners[3])
	) {
		return PackedInt64Array{
			corners[0],corners[1],corners[1],corners[1],
			corners[1],corners[1],corners[1],corners[3]
		};
	}

	// Case 13
	// |2 1|
	// |3 1|
	else if ((corners[0] != corners[1]) && 
			(corners[0] != corners[2]) &&
			(corners[1] == corners[3]) 
	) {
		return PackedInt64Array{
			corners[0],corners[1],corners[1],corners[1],
			corners[2],corners[1],corners[1],corners[1]
		};
	}

	// Case 14
	// |1 1|
	// |2 3|
	else if ((corners[0] == corners[1]) && 
			(corners[0] != corners[2]) &&
			(corners[0] != corners[3]) &&
			(corners[2] != corners[3])
	) {
		return PackedInt64Array{
			corners[0],corners[0],corners[0],corners[0],
			corners[2],corners[0],corners[0],corners[3]
		};
	}

	// Case 15
	// |1 2|
	// |3 4|
	else {
		return PackedInt64Array{
			corners[0],corners[0],corners[1],corners[1],
			corners[2],corners[2],corners[3],corners[3]
		};
	}
}


/*
 Terrain Generation
*/

// Clean from previously generated content
void TerrainGenerator::clean() {
	// Remove all nodes
	for (int i = 0; i < get_child_count(); i++) {
		for (int j = 0; j < get_child_count(); j++) {
			get_child(j)->queue_free();
		}
		get_child(i)->queue_free();
	}
	// Empty arrays
	loadedChunkPositions.clear();
	loadedChunks.clear();
	mutex->lock();
	chunkLoadQueue.clear();
	chunkRemoveQueue.clear();
	mutex->unlock();
	
	// Restart thread
	threadTime = 0;
	if (!threadActive) {
		threadActive = true;
		thread->wait_to_finish();
		thread.unref();

		thread.instantiate();
		thread->start(map_update().bind(0));
	}
}


// Generate chunk
void TerrainGenerator::generate_chunk(Vector2 chunkIndex) {
	PackedVector3Array faces;
	PackedInt64Array cornerValues;
	PackedInt64Array cornerHeights;
	
	// Calculate 2d origin
	Vector2 origin2d = chunkIndex * gridSize;
	
	// Calculate cell size
	Vector3 cellSize(1,1,1);
	Vector2 textureSize = Vector2(1,1) / tilesheetSize;
	Vector2 tileSize = Vector2(1,1) - tileMargin;

	// Generate surfaces
	Dictionary surfaces;
	for (int surfaceIndex = 0; surfaceIndex < materials.size(); surfaceIndex++) {
		// Create new SurfaceTool
		SurfaceTool st;
		st.begin(Mesh::PRIMITIVE_TRIANGLES);
		// Set material
		st.set_material(materials[surfaceIndex]);
		// Set material filter
		enum Filter filterMode = Filter::ALL;
		if (surfaceIndex < materialFilters.size())
			filterMode = materialFilters[surfaceIndex];
		var filterValues = []
		if surfaceIndex < materialValues.size():
			for val in materialValues[surfaceIndex].replace(" ","").split(","):
				filterValues.append(int(val))
			
		# Loop through grid
		for y in int(gridSize.y):
			for x in int(gridSize.x):
				var cellPos = Vector3(x,0,y)
				var cellPos2d = Vector2(x,y)
			
				# Define trig values
				var trigValues = PackedInt64Array()
				# Generating values using marching squares
				if marchingSquares and generatorHasValueFunc:
					var cellCornerValues = PackedInt64Array()
					if cellPos2d.x == 0 or cellPos2d.y == 0:
						for v in cornerVectors:
							cellCornerValues.append(generator.get_value(v+cellPos2d+origin2d))
					else:
							cellCornerValues.append(cornerValues[(y-1)*gridSize.x*4+x*4+2])
							cellCornerValues.append(cornerValues[(y-1)*gridSize.x*4+x*4+3])
							cellCornerValues.append(cornerValues[y*gridSize.x*4+(x-1)*4+3])
							cellCornerValues.append(generator.get_value(cornerVectors[3]+cellPos2d+origin2d))
					cornerValues.append_array(cellCornerValues)
					trigValues = get_trigs_marching(cellCornerValues)
				# Generating values without using marching squares
				else:
					var value = 0
					if generatorHasValueFunc:
						value = generator.get_value(cellPos2d+origin2d)
					trigValues = [value]
			
				# Generating heights
				if generatorHasHeightFunc:
					var cellCornerHeights = PackedInt64Array()
					if cellPos2d.x == 0 or cellPos2d.y == 0:
						for v in cornerVectors:
							cellCornerHeights.append(generator.get_height(v+cellPos2d+origin2d))
					else:
							cellCornerHeights.append(cornerHeights[(y-1)*gridSize.x*4+x*4+2])
							cellCornerHeights.append(cornerHeights[(y-1)*gridSize.x*4+x*4+3])
							cellCornerHeights.append(cornerHeights[y*gridSize.x*4+(x-1)*4+3])
							cellCornerHeights.append(generator.get_height(cornerVectors[3]+cellPos2d+origin2d))
					cornerHeights.append_array(cellCornerHeights)
							
				# Add triangles
				for trig in range(8):
					# Find value
					var trigValue = trigValues[0]
					if marchingSquares:
						var trigIndex = trig
						trigValue = trigValues[trigIndex]

					# Filter by values
					if filterMode == WHITELIST:
						if not trigValue in filterValues:
							continue
					elif filterMode == BLACKLIST:
						if trigValue in filterValues:
							continue
					
					# Add vertices
					for i in range(3):
						var vert = Vector3(cell[trig*6+i*2], 0, cell[trig*6+i*2+1])
						# Add height
						if generatorHasHeightFunc:
							var cellIndex = y*gridSize.x*4+x*4
							if vert.x == 0.5 and vert.z == 0.5:
								vert.y = lerp(cornerHeights[cellIndex],cornerHeights[cellIndex+3],0.5)
								vert.y = lerp(vert.y,lerp(cornerHeights[cellIndex+1],cornerHeights[cellIndex+2],0.5),0.5)
							elif vert.x == 0.5:
								vert.y = lerp(cornerHeights[cellIndex+vert.z*2], cornerHeights[cellIndex+vert.z*2+1], 0.5)
							elif vert.z == 0.5:
								vert.y = lerp(cornerHeights[cellIndex+vert.x], cornerHeights[cellIndex+2+vert.x], 0.5)
							else:
								vert.y = cornerHeights[cellIndex+vert.z*2+vert.x]
						# Calculate UV
						var uvPos = Vector2.ZERO
						uvPos.y = floor(trigValue/tilesheetSize.x)
						uvPos.x = trigValue - uvPos.y*tilesheetSize.x
						uvPos += tileMargin/2
						# Add vertex
						st.set_uv(Vector2(vert.x,vert.z)*tileSize*textureSize+uvPos*textureSize)
						var vertex = (vert+cellPos+offset)*cellSize
						st.add_vertex(vertex)
						faces.append(vertex)
					
		# Generate normals and tangents
		st.generate_normals()
		st.generate_tangents()
		# Commit
		surfaces.append(st.commit())
	}
		
	# Add collision
	var collisionShape
	if addCollision:
		var concaveShape = ConcavePolygonShape3D.new()
		concaveShape.set_faces(faces)
		collisionShape = CollisionShape3D.new()
		collisionShape.shape = concaveShape

	mutex.lock()
	chunkLoadQueue.append([surfaces, collisionShape, origin2d, chunkIndex])
	mutex.unlock()
}

// Remove chunk
void TerrainGenerator::remove_chunk(int chunkIndex) {
	// Free nodes
	MeshInstance3D* chunk = loadedChunks[chunkIndex];
	
	for (int i = 0; i < chunk->get_child_count(); i++) {
		chunk->get_child(i)->queue_free();
	}

	chunk->call_deferred("queue_free");

	// Emit signal
	emit_signal("chunk_removed",loadedChunkPositions[chunkIndex]);
	
	// Remove from array
	loadedChunks.remove_at(chunkIndex);
	loadedChunkPositions.remove_at(chunkIndex);
}



Callable TerrainGenerator::map_update() {
	while (threadActive) {
		if (threadTime < mapUpdateTime)
			continue;

		threadTime = 0.0;

		// Define current chunk
		Vector2 currentChunk(0,0);
		mutex->lock();
		Vector3 _playerPosition = playerPosition;
		mutex->unlock();
		currentChunk.x = floor(_playerPosition.x/gridSize.x);
		currentChunk.y = floor(_playerPosition.z/gridSize.y);
		
		// Define needed chunks
		PackedVector2Array neededChunks;
		neededChunks.append(currentChunk);
		if (chunkLoadRadius != 0) {
			for (int ring = 1; ring <= chunkLoadRadius+1; ring++) {
				for (int x = 0; x < (ring*2+1); x++) {
					neededChunks.append(Vector2(x-ring, -ring) + currentChunk);
					neededChunks.append(Vector2(x-ring, ring) + currentChunk);
				}
				for (int y = 0; y < (ring*2); y++) {
					neededChunks.append(Vector2(ring, y-ring) + currentChunk);
					neededChunks.append(Vector2(-ring, y-ring) + currentChunk);
				}
			}
		}
		// Generate needed chunks
		for (Vector2 chunkIndex : neededChunks) {
			if (!loadedChunkPositions.has(chunkIndex)) {
				generate_chunk(chunkIndex);
				break;
			}
		}
		
		// Remove unneeded chunks
		for (int i = 0; i < loadedChunkPositions.size(); i++) {
			Vector2 chunkPos = loadedChunkPositions[i];
			if (neededChunks.has(chunkPos)) {
				mutex->lock();
				chunkRemoveQueue.append(i);
				mutex->unlock();
				break;
			}
		}

		// Finish thread if dynamicGeneration is disabled and generation is done
		if (!dynamicGeneration) {
			if (loadedChunkPositions.size() == neededChunks.size()) {
				threadActive = false;
			}
		}

	}

}

void TerrainGenerator::process(double delta) {
	threadTime += delta;
	mutex->lock();
	playerPosition = get_position();
	if (player != nullptr) {
		playerPosition = player->get_position();
	}
	if (chunkLoadQueue.size() > 0) {
		for (Chunk chunk : chunkLoadQueue) {
			MeshInstance3D* meshInstance = memnew(MeshInstance3D);
			//meshInstance.get_position().x = chunk[2].x;
			//meshInstance.get_position().z = chunk[2].y;
			meshInstance->set_position(Vector3(chunk.origin2d.x, 0, chunk.origin2d.y));

			add_child(meshInstance);

			int surfaceIndex = 0;
			for (Ref<ArrayMesh> surface : chunk.surfaces) {
				if (surfaceIndex == 0) {
					meshInstance->set_mesh(surface);
				}
				//else:
					//st.commit(meshInstance.mesh)
				surfaceIndex += 1;
			}
			if (chunk.collisionShape != nullptr) {
				StaticBody3D* staticBody;
				staticBody->add_child(chunk.collisionShape);
				meshInstance->add_child(staticBody);
			}
			// Add to loaded
			loadedChunkPositions.append(chunk.chunkIndex);
			loadedChunks.append(meshInstance);
			emit_signal("chunk_spawned", chunk.chunkIndex, meshInstance);
		}
		chunkLoadQueue.clear();
	}
	if (!chunkRemoveQueue.is_empty()) {
		chunkRemoveQueue.sort();
		for (Chunk chunkIndex : chunkRemoveQueue) {
			remove_chunk(chunkIndex);
		}
		chunkLoadQueue.clear();
	}

	mutex->unlock();
}

void TerrainGenerator::clean_up() {
	/*
	for (KeyValue<Vector2i, int32_t> chunk : _chunks) {
		Ref<core_bind::Thread> thread = Object::cast_to<Chunk>(get_child(chunk.value))->get_thread();
		if (thread != nullptr)
			thread->wait_to_finish();

		thread.unref();
	}
	*/
	
	_chunks.clear();
	set_process(false);

	for (int c = 0; c < get_child_count(); c++) {
		//remove_child(child);
		get_child(c)->queue_free();
		//child.unref();
    }
}


void TerrainGenerator::_delete_far_away_chunks(Vector3i player_chunk) {
	_old_player_chunk = player_chunk;
	// If we need to delete chunks, give the new chunk system a chance to catch up.
	effective_render_distance = std::max(1, effective_render_distance - 1);

	int deleted_this_frame = 0;
	// We should delete old chunks more aggressively if moving fast.
	// An easy way to calculate this is by using the effective render distance.
	// The specific values in this formula are arbitrary and from experimentation.
	int max_deletions = CLAMP(2 * (render_distance - effective_render_distance), 2, 8);

	// Also take the opportunity to delete far away chunks.
	// remove_child

	const Array key_list = _chunks.keys();
	
	for (const Vector2i chunk_key : key_list) {
		if (Vector2(player_chunk.x, player_chunk.z).distance_to(Vector2(chunk_key)) > _delete_distance) {
			Node* child = get_node(_chunks[chunk_key]);

			// No longer need this, but this is just in case
			if (child == nullptr) {
				NodePath name = _chunks[chunk_key];
				print_line("NULL CHILD: " + name);
				continue;
			} 

			child->queue_free();
			_chunks.erase(chunk_key);

			deleted_this_frame += 1;

			// Limit the amount of deletions per frame to avoid lag spikes.
			if (deleted_this_frame > max_deletions) {
				_deleting = true;	// Continue deleting next frame.
				return;
			}
		}
	}

	// We're done deleting.
	_deleting = false;
}

void TerrainGenerator::set_player_character(CharacterBody3D* p_node) {
	if (p_node == nullptr) return;

	player_character = p_node;
}

CharacterBody3D* TerrainGenerator::get_player_character() const {
	return player_character;
}

void TerrainGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_player_character", "p_node"), &TerrainGenerator::set_player_character);
	ClassDB::bind_method(D_METHOD("get_player_character"), &TerrainGenerator::get_player_character);
	
	ClassDB::add_property("TerrainGenerator", PropertyInfo(Variant::OBJECT, "player_character", PROPERTY_HINT_NODE_TYPE, "CharacterBody3D"), "set_player_character", "get_player_character");
}


// constructor - sets up default values
TerrainGenerator::TerrainGenerator() {
	//count = 0;

	// enable process
	set_process(true);
}