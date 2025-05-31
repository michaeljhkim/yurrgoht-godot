shader_type spatial;

global uniform sampler2D heightmap;
global uniform sampler2D normalmap;
global uniform float amplitude;

varying vec2 texture_position;

// Called for every vertex the material is visible on.
void vertex() {
	vec3 world_position = VERTEX + MODEL_MATRIX[3].xyz;
	texture_position = world_position.xz / float(textureSize(heightmap,0).x);
	VERTEX.y = texture(heightmap, texture_position).r * amplitude;
}

// Called for every pixel the material is visible on.
void fragment() {
	NORMAL_MAP = texture(normalmap, texture_position).rgb;
}

//void light() {
//	// Called for every pixel for every light affecting the material.
//	// Uncomment to replace the default light processing function with this one.
//}
