extends Node

var image:Image = load(ProjectSettings.get_setting("shader_globals/heightmap").value).get_image()
var amplitude:float = ProjectSettings.get_setting("shader_globals/amplitude").value
var size = image.get_width()

# Compute shader values
#@export_file("*.glsl") var shader_file: String = "res://clipmap/partition_heightmap_generator.glsl"
#@export_range(128, 4096, 1, "exp") var dimension: int = 512
var shader_file: String = "res://clipmap/partition_heightmap_generator.glsl"
var dimension: int = 512

var seed_input: int
var heightmap_rect: TextureRect
var island_rect: TextureRect

var noise: FastNoiseLite
var gradient: Gradient
var gradient_tex: GradientTexture1D

var po2_dimensions: int
var start_time: int

var rd: RenderingDevice
var shader_rid: RID
var heightmap_rid: RID
var gradient_rid: RID
var uniform_set: RID
var pipeline: RID


func get_height(x,z):
	return image.get_pixel(fposmod(x,size), fposmod(z,size)).r * amplitude
	
# for computer shader
func _init() -> void:
	init_heightmap()
	pass
	
func _ready():
	ready_heightmap()
	pass


func init_heightmap() -> void:
	heightmap_rect = TextureRect.new()
	island_rect = TextureRect.new()

	# Create a noise function as the basis for our heightmap.
	noise = FastNoiseLite.new()
	noise.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	noise.fractal_octaves = 5
	noise.fractal_lacunarity = 1.9

	# Create a gradient to function as overlay.
	gradient = Gradient.new()
	gradient.add_point(0.6, Color(0.9, 0.9, 0.9, 1.0))
	gradient.add_point(0.8, Color(1.0, 1.0, 1.0, 1.0))
	# The gradient will start black, transition to grey in the first 70%, then to white in the last 30%.
	gradient.reverse()

	# Create a 1D texture (single row of pixels) from gradient.
	gradient_tex = GradientTexture1D.new()
	gradient_tex.gradient = gradient

func ready_heightmap():
	var heightmap := prepare_image()
	compute_island_cpu.call_deferred(heightmap)

	
# COMPUTE SHADER CODE BEGINS
# Generate a random integer, convert it to a string and set it as text for the TextEdit field.
func randomize_seed() -> void:
	seed_input = randi()
	
func prepare_image() -> Image:
	randomize_seed()
	po2_dimensions = nearest_po2(dimension)

	noise.frequency = 0.003 / (float(po2_dimensions) / 512.0)

	# Append GPU and CPU model names to make performance comparison more informed.
	# On unbalanced configurations where the CPU is much stronger than the GPU,
	# compute shaders may not be beneficial.
	#$CenterContainer/VBoxContainer/PanelContainer/VBoxContainer/HBoxContainer/CreateButtonGPU.text += "\n" + RenderingServer.get_video_adapter_name()
	#$CenterContainer/VBoxContainer/PanelContainer/VBoxContainer/HBoxContainer/CreateButtonCPU.text += "\n" + OS.get_processor_name()
	
	start_time = Time.get_ticks_usec()
	noise.seed = int(seed_input)
	# Create image from noise.
	var heightmap := noise.get_image(po2_dimensions, po2_dimensions, false, false)

	# Create ImageTexture to display original on screen.
	var clone := Image.new()
	clone.copy_from(heightmap)
	clone.resize(512, 512, Image.INTERPOLATE_NEAREST)
	var clone_tex := ImageTexture.create_from_image(clone)
	heightmap_rect.texture = clone_tex
	
	return heightmap


func compute_island_cpu(heightmap: Image) -> void:
	# This function is the CPU counterpart of the `main()` function in `compute_shader.glsl`.
	var center := Vector2i(po2_dimensions, po2_dimensions) / 2
	# Loop over all pixel coordinates in the image.
	for y in range(0, po2_dimensions):
		for x in range(0, po2_dimensions):
			var coord := Vector2i(x, y)
			var pixel := heightmap.get_pixelv(coord)
			# Calculate the distance between the coord and the center.
			var distance := Vector2(center).distance_to(Vector2(coord))
			# As the X and Y dimensions are the same, we can use center.x as a proxy for the distance
			# from the center to an edge.
			var gradient_color := gradient.sample(distance / float(center.x))
			# We use the v ('value') of the pixel here. This is not the same as the luminance we use
			# in the compute shader, but close enough for our purposes here.
			pixel.v *= gradient_color.v
			if pixel.v < 0.2:
				pixel.v = 0.0
			heightmap.set_pixelv(coord, pixel)
			
	var island_texture := ImageTexture.create_from_image(heightmap)
	RenderingServer.global_shader_parameter_set("heightmap", island_texture)
