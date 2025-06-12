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

void TerrainGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add", "value"), &TerrainGenerator::add);
	ClassDB::bind_method(D_METHOD("reset"), &TerrainGenerator::reset);
	ClassDB::bind_method(D_METHOD("get_total"), &TerrainGenerator::get_total);
}

TerrainGenerator::TerrainGenerator() {
	count = 0;
}