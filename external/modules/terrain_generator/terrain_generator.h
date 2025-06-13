#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "core/object/ref_counted.h"

// this class inherits from the noise class in noise.h
#include "fastnoise_lite.h"
#include "noise_texture_2d.h"
#include "texture_rect.h"
#include "gradient_texture.h"

#include "core/variant/variant_utility.h"
#include "scene/resources/image_texture.h"


class TerrainGenerator : public RefCounted {
	GDCLASS(TerrainGenerator, RefCounted);

	int count;

protected:
	static void _bind_methods();

	int dimension = 512;
	int seed_input;
	int po2_dimensions;

	TextureRect heightmap_rect;
	TextureRect island_rect;
	FastNoiseLite noise;
	Gradient gradient;
	GradientTexture1D gradient_tex;

public:
	void add(int p_value);
	void reset();
	int get_total() const;

	// Can most likely just be done in gdscript, but created just in case it becomes cleaner to move the process here
	Ref<Image> prepare_image();

	// Generate noise - apply the noise from the gradient to the heightmap image
	void generate_noise(Ref<Image> heightmap);

	TerrainGenerator();
};

#endif     //TERRAIN_GENERATOR_H