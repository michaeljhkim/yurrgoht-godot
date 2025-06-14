@tool
extends StaticBody3D

@export var generate: bool:
	set(value):
		generate = value
		if value:
			_generate_mesh()

@export var size: int = 64
@export var subdivide: int = 63
@export var amplitude: float = 16.0
@export var noise: FastNoiseLite = FastNoiseLite.new()
var array_size: int

func _ready():
	array_size = size * size
	
	# Ensure noise is initialized properly in the editor
	_generate_mesh()

func _generate_mesh():
	pass
