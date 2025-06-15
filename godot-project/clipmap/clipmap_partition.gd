extends MeshInstance3D

var x = 0
var z = 0
var clipmap_position: Vector3

var size = 64
var subdivide = 63
var amplitude = 16
var noise = FastNoiseLite.new()


const SEAM_LEFT = 1
const SEAM_RIGHT = 2
const SEAM_BOTTOM = 4
const SEAM_TOP = 8
const SEAM_CONFIG_COUNT = 16

# Called when the node enters the scene tree for the first time.
func _ready():
	var length = ProjectSettings.get_setting("shader_globals/clipmap_partition_length").value
	var lod_step = ProjectSettings.get_setting("shader_globals/lod_step").value
	var mesh_quality = ProjectSettings.get_setting("shader_globals/mesh_quality").value
	
	#mesh = PlaneMesh.new()
	#mesh.size = Vector2.ONE * length
	position = Vector3(x,0,z) * length
	
	# make it so that the LOD changes every 2 chunks out.
	# so transitions to less detailed chunks should be happening further from player now, regardless of square size
	var lod = max(abs(x),abs(z)) * lod_step
	var subdivision_length = pow(2,lod)
	var subdivides = max(length * mesh_quality / subdivision_length - 1, 0)
	
	mesh.subdivide_width = subdivides
	mesh.subdivide_depth = subdivides

	
	
# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta):
	pass
