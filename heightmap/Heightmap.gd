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
	init_compute_shader()
	
func _ready():
	ready_compute_shader()
	pass


func init_compute_shader() -> void:
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

func ready_compute_shader():
	var heightmap := prepare_image()
	compute_island_gpu.call_deferred(heightmap)

	
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



func init_gpu() -> void:
	# These resources are expensive to make, so create them once and cache for subsequent runs.

	# Create a local rendering device (required to run compute shaders).
	rd = RenderingServer.create_local_rendering_device()

	if rd == null:
		OS.alert("""Couldn't create local RenderingDevice on GPU: %s

Note: RenderingDevice is only available in the Forward+ and Mobile rendering methods, not Compatibility.""" % RenderingServer.get_video_adapter_name())
		return

	# Prepare the shader.
	shader_rid = load_shader(rd, shader_file)

	# Create format for heightmap.
	var heightmap_format := RDTextureFormat.new()
	# There are a lot of different formats. It might take some studying to be able to be able to
	# choose the right ones. In this case, we tell it to interpret the data as a single byte for red.
	# Even though the noise image only has a luminance channel, we can just interpret this as if it
	# was the red channel. The byte layout is the same!
	heightmap_format.format = RenderingDevice.DATA_FORMAT_R8_UNORM
	heightmap_format.width = po2_dimensions
	heightmap_format.height = po2_dimensions
	# The TextureUsageBits are stored as 'bit fields', denoting what can be done with the data.
	# Because of how bit fields work, we can just sum the required ones: 8 + 64 + 128
	heightmap_format.usage_bits = \
			RenderingDevice.TEXTURE_USAGE_STORAGE_BIT + \
			RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT + \
			RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT

	# Prepare heightmap texture. We will set the data later.
	heightmap_rid = rd.texture_create(heightmap_format, RDTextureView.new())

	# Create uniform for heightmap.
	var heightmap_uniform := RDUniform.new()
	heightmap_uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	heightmap_uniform.binding = 0  # This matches the binding in the shader.
	heightmap_uniform.add_id(heightmap_rid)

	# Create format for the gradient.
	var gradient_format := RDTextureFormat.new()
	# The gradient could have been converted to a single channel like we did with the heightmap,
	# but for illustrative purposes, we use four channels (RGBA).
	gradient_format.format = RenderingDevice.DATA_FORMAT_R8G8B8A8_UNORM
	gradient_format.width = gradient_tex.width  # Default: 256
	# GradientTexture1D always has a height of 1.
	gradient_format.height = 1
	gradient_format.usage_bits = \
		RenderingDevice.TEXTURE_USAGE_STORAGE_BIT + \
		RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT

	# Storage gradient as texture.
	gradient_rid = rd.texture_create(gradient_format, RDTextureView.new(), [gradient_tex.get_image().get_data()])

	# Create uniform for gradient.
	var gradient_uniform := RDUniform.new()
	gradient_uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	gradient_uniform.binding = 1  # This matches the binding in the shader.
	gradient_uniform.add_id(gradient_rid)

	uniform_set = rd.uniform_set_create([heightmap_uniform, gradient_uniform], shader_rid, 0)

	pipeline = rd.compute_pipeline_create(shader_rid)




# Object destructor, triggered before the engine deletes this Node.
func _notification(what: int) -> void:
	if what == NOTIFICATION_PREDELETE:
		cleanup_gpu()
		
# not sure when to call this yet - probably after heightmap creation, or just on close 
func cleanup_gpu() -> void:
	if rd == null:
		return

	# All resources must be freed after use to avoid memory leaks.
	rd.free_rid(pipeline)
	pipeline = RID()

	rd.free_rid(uniform_set)
	uniform_set = RID()

	rd.free_rid(gradient_rid)
	gradient_rid = RID()

	rd.free_rid(heightmap_rid)
	heightmap_rid = RID()

	rd.free_rid(shader_rid)
	shader_rid = RID()

	rd.free()
	rd = null

# Import, compile and load shader, return reference.
func load_shader(p_rd: RenderingDevice, path: String) -> RID:
	var shader_file_data: RDShaderFile = load(path)
	var shader_spirv: RDShaderSPIRV = shader_file_data.get_spirv()
	return p_rd.shader_create_from_spirv(shader_spirv)
	
	
# Time to actually do the computations
func compute_island_gpu(heightmap: Image) -> void:
	if rd == null:
		init_gpu()

	if rd == null:
		$CenterContainer/VBoxContainer/PanelContainer2/VBoxContainer/HBoxContainer2/Label2.text = \
			"RenderingDevice is not available on the current rendering driver"
		return

	# Store heightmap as texture.
	rd.texture_update(heightmap_rid, 0, heightmap.get_data())

	var compute_list := rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(compute_list, pipeline)
	rd.compute_list_bind_uniform_set(compute_list, uniform_set, 0)
	# This is where the magic happens! As our shader has a work group size of 8x8x1, we dispatch
	# one for every 8x8 block of pixels here. This ratio is highly tunable, and performance may vary.
	@warning_ignore("integer_division")
	rd.compute_list_dispatch(compute_list, po2_dimensions / 8, po2_dimensions / 8, 1)
	rd.compute_list_end()

	rd.submit()
	# Wait for the GPU to finish.
	# Normally, you would do this after a few frames have passed so the compute shader can run in the background.
	rd.sync()

	# Retrieve processed data.
	var output_bytes := rd.texture_get_data(heightmap_rid, 0)
	# Even though the GPU was working on the image as if each byte represented the red channel,
	# we'll interpret the data as if it was the luminance channel.
	var island_img := Image.create_from_data(po2_dimensions, po2_dimensions, false, Image.FORMAT_L8, output_bytes)
	var island_texture := ImageTexture.create_from_image(island_img)
	#island_img.save_png("user://heightmap_test.png")
	
	RenderingServer.global_shader_parameter_set("heightmap", island_texture)
