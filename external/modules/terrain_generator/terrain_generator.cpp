#include "terrain_generator.h"

void TerrainGenerator::add(int p_value) {
	count += p_value;
}

void TerrainGenerator::reset() {
	count = 0;
}

int TerrainGenerator::get_total() const {
	return count;
}



// Define values for the noise texture
Ref<Image> TerrainGenerator::prepare_image() {
	po2_dimensions = VariantUtilityFunctions::nearest_po2(dimension);
	noise.set_frequency(0.01 / (float(po2_dimensions) / float(dimension)));
	noise.set_seed(int(seed_input));

	Ref<Image> heightmap = noise.get_image(po2_dimensions, po2_dimensions, false, false);

	Image clone;
	clone.copy_from(heightmap);
	clone.resize(dimension, dimension, Image::INTERPOLATE_NEAREST);

	Ref<ImageTexture> clone_tex = ImageTexture::create_from_image(&clone);
	heightmap_rect.set_texture(clone_tex);

	return heightmap;
}

// Generate noise - apply the noise from the gradient to the heightmap image
void TerrainGenerator::generate_noise(Ref<Image> heightmap) {
	Vector2i center = Vector2i(po2_dimensions, po2_dimensions) / 2;

	for (int y = 0; y < po2_dimensions; y++) {
		for (int x = 0; x < po2_dimensions; x++) {
			Vector2i coord = Vector2i(x, y);
			Color pixel = heightmap->get_pixelv(coord);
			real_t distance = Vector2(center).distance_to(Vector2(coord));

			Color gradient_color = gradient.get_color_at_offset(distance / float(center.x));
			pixel.set_v(pixel.get_v() * gradient_color.get_v());

			if (pixel.get_v() < 0.2)
				pixel.set_v(0.0);
			
			heightmap->set_pixelv(coord, pixel);
		}
	}
	ImageTexture::create_from_image(heightmap);
}


// Functions that will be used in GDSCRIPT
void TerrainGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add", "value"), &TerrainGenerator::add);
	ClassDB::bind_method(D_METHOD("reset"), &TerrainGenerator::reset);
	ClassDB::bind_method(D_METHOD("get_total"), &TerrainGenerator::get_total);

	ClassDB::bind_method(D_METHOD("add", "value"), &TerrainGenerator::add);
	ClassDB::bind_method(D_METHOD("reset"), &TerrainGenerator::reset);
	ClassDB::bind_method(D_METHOD("get_total"), &TerrainGenerator::get_total);
}


// constructor - sets up default values
TerrainGenerator::TerrainGenerator() {
	count = 0;

	noise.set_noise_type(FastNoiseLite::NoiseType::TYPE_SIMPLEX_SMOOTH);
	noise.set_fractal_octaves(3);
	noise.set_fractal_lacunarity(0.6);

	// Create a gradient to function as overlay.
	gradient.add_point(0.6, Color(0.9, 0.9, 0.9, 1.0));
	gradient.add_point(0.8, Color(1.0, 1.0, 1.0, 1.0));
	
	// The gradient will start black, transition to grey in the first 70%, then to white in the last 30%.
	gradient.reverse();

	// Create a 1D texture (single row of pixels) from gradient.
	gradient_tex.set_gradient(&gradient);
}