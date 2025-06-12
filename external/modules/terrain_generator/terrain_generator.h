#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "core/object/ref_counted.h"
#include "noise.h"

class TerrainGenerator : public RefCounted {
	GDCLASS(TerrainGenerator, RefCounted);

	int count;

protected:
	static void _bind_methods();

public:
	void add(int p_value);
	void reset();
	int get_total() const;

	TerrainGenerator();
};

#endif // TERRAIN_GENERATOR_H