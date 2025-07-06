extends StaticBody3D

var point: PackedVector3Array
var normal : PackedVector3Array
var color : PackedColorArray
var triangle : PackedInt32Array
#var triangle_neighbor : PackedInt32Array
var pvertex : Array[PlanetVertex]
enum Biome{POLAR, MOUNTAIN, FOREST, DESERT, NB_BIOME}
enum Topology{Mountain, Plain}

class Plate:
	var velocity : Vector3
	func _init(orientation:Vector3):
		var sq_norm = 2.0
		while sq_norm > 1.0 && sq_norm != 0.0:
			velocity = Vector3(randf(), randf(), randf())
			sq_norm = velocity.length_squared()
		velocity = velocity.cross(orientation)

class PlanetVertex:
	var color_mountain := Color(0.652344, 0.402344, 0.269531)
	var color_snow := Color(0.976563, 0.9375, 0.660156)
	var color_grass := Color(0.132813, 0.691406, 0.285156)
	var color_desert := Color(0.996094, 0.785156, 0.0585938)
	var biome_color = [color_snow, color_mountain, color_grass, color_desert]
	var orientation : Vector3
	var height: float #distance from world center
	var neighbor : Array[int]
	var biome: Biome
	var temperature: float
	var color:Color
	#var terrain_type: TerrainType
	func _init(_orientation:Vector3, _height:float, _biome:Biome):
		orientation = _orientation
		height = _height
		biome = _biome
		color = biome_color[biome as int]
		neighbor = []

class TempPlanetVertex:#holding temporary data useful to generation
	var vertex : PlanetVertex
	var plate: Plate
	func _init(_orientation:Vector3, _height:float, _plate:Plate, biome:Biome):
		vertex = PlanetVertex.new(_orientation, _height, biome)
		plate = _plate
	static func mix(A,B,a,b):
		return (a*A+b*B)/(a+b)
	static func from(A:TempPlanetVertex, B:TempPlanetVertex) -> TempPlanetVertex:
		var sqd = (B.get_pos()-A.get_pos()).length_squared()
		var x = randf()
		var m = 8+x
		var n = 8-x
		var dominant_parent = A if randf()>0.5 else B
		var orientation = mix(A.vertex.orientation,B.vertex.orientation, m, n).normalized()
		var height = (dominant_parent.vertex.height + mix(A.vertex.height, B.vertex.height, m, n))/2
		if A.plate != B.plate:
			var AC = orientation-A.vertex.orientation
			var BC = orientation-B.vertex.orientation
			height += 88.0*(A.plate.velocity.dot(AC.normalized())+B.plate.velocity.dot(BC.normalized()))/(1+(sqd/10000)**1.5)
		var v 
		if sqd < 800000:
			v = TempPlanetVertex.new(orientation, height, dominant_parent.plate, dominant_parent.vertex.biome)
		else:
			v = TempPlanetVertex.new(orientation, height, Plate.new(orientation), randi_range(0,Biome.NB_BIOME-1))
		return v
	func get_pos():
		return vertex.orientation*vertex.height

class Triangle:
	var edge : Array[Edge] #referencing 3 edges. edge[i] faces vertex[i].
	var vertex_xor #bitwise xor of the three vertice
	func sq_area(tmpvertex_array):
		var _AB = edge[0]
		var C = tmpvertex_array[other_vertex(_AB)].get_pos()
		var A = tmpvertex_array[_AB.vertex[0]].get_pos()
		var B = tmpvertex_array[_AB.vertex[1]].get_pos()
		var cross = (B-A).cross(C-A)
		return cross.length_squared()
	func split_in_2():
		var BC
		var AB
		var AC
		var has_split_edge = false
		for i in range(3):
			if edge[i].is_split():
				BC = edge[i]
				AB = edge[(i+1)%3]
				AC = edge[(i+2)%3]
				has_split_edge = true
				break
		if not has_split_edge:
			return [self]
		var A = other_vertex(BC)
		var a = BC.splitting_vertex
		var Aa = Edge.new([A,a] as Array[int], [self, self] as Array[Triangle])
		var B = AB.vertex[AB.vertex[0] == A as int]
		var C = AC.vertex[AC.vertex[0] == A as int]
		var Ba = BC.child_without(C)
		var aC = BC.child_without(B)
		var AaB = Triangle.new([Aa, Ba, AB] as Array[Edge], self)
		var AaC = Triangle.new([Aa, aC, AC] as Array[Edge], self)
		return [AaB, AaC]
		
	func split_in_4(vertex_array:Array[TempPlanetVertex]):
		for i in range(3):
			var e = edge[i]
			if not e.is_split() :
				e.split(vertex_array)
		var A = edge[0].vertex[0]
		var B = edge[0].vertex[1]
		var C = edge[1].vertex[0]
		if C == A or C == B:
			C= edge[1].vertex[1]
		var AB =  edge[0]
		var AC
		var BC
		if A in edge[1].vertex :
			AC = edge[1]
			BC = edge[2]
		else :
			AC = edge[2]
			BC = edge[1]
		var Ac = AB.child[0]
		var c = AB.splitting_vertex
		var cB = AB.child[1]
		var index = (BC.vertex[0] == C) as int
		var Ba = BC.child[index]
		var a = BC.splitting_vertex
		var aC = BC.child[1-index]
		index = (AC.vertex[0] == A) as int
		var Cb = AC.child[index]
		var b = AC.splitting_vertex
		var bA = AC.child[1-index]
		var ab = Edge.new([a,b]as Array[int], [self,self] as Array[Triangle])
		var bc = Edge.new([b,c]as Array[int], [self,self] as Array[Triangle])
		var ac = Edge.new([c,a]as Array[int], [self,self] as Array[Triangle])
		var tAbc = Triangle.new([bA, bc, Ac] as Array[Edge], self )
		var taBc = Triangle.new([Ba, ac, cB] as Array[Edge], self)
		var tabC = Triangle.new([ab, Cb, aC] as Array[Edge], self)
		var tabc = Triangle.new([ab, ac, bc] as Array[Edge], self)
		return [tabc, tAbc, taBc, tabC]
	func _init(_edge, parent):
		edge = _edge as Array[Edge]
		for e in edge :
			e.replace_triangle(parent, self)
		var vertex = edge[0].vertex.duplicate()
		if edge[1].vertex[0] in vertex:
			vertex.append(edge[1].vertex[1])
		else:
			vertex.append(edge[1].vertex[0])
		vertex_xor = 0
		for v in vertex:
			vertex_xor = vertex_xor^v
	func other_vertex(e:Edge):
		return e.vertex[0] ^ e.vertex[1] ^ vertex_xor
	func get_vertex_unordered():
		var _a = edge[0].vertex[0]
		var _b = edge[0].vertex[1]
		var _c = other_vertex(edge[0])
		return [_a, _b, _c]
	func get_vertex(pvertex_array):
		var vertex = get_vertex_unordered()
		var a = pvertex_array[vertex[0]].vertex.orientation
		var b = pvertex_array[vertex[1]].vertex.orientation
		var c = pvertex_array[vertex[2]].vertex.orientation
		if (b-a).cross(c-a).dot(a) > 0 :
			var tmp = vertex[2]
			vertex[2] = vertex[1]
			vertex[1] = tmp
		return vertex

class Edge:
	var vertex: Array[int] #two vertice
	var triangle: Array[Triangle] #two triangles
	var child: Array[Edge] #(smaller) edges resulting from vertex insertion
	var splitting_vertex
	func _init(_vertex, _triangle):
		vertex = _vertex 
		triangle = _triangle
		child = []
	func is_split():
		return not child.is_empty()
	func other_triangle(t:Triangle):
		return triangle[(triangle[0]==t) as int]
	func replace_triangle(parent, _child):
		triangle[(triangle[0]!=parent) as int] = _child
	func split(vertex_array:Array[TempPlanetVertex])->int:
		var A = vertex_array[vertex[0]]
		var B = vertex_array[vertex[1]]
		var v = TempPlanetVertex.from(A,B)
		var index = len(vertex_array)
		vertex_array.append(v)
		child = [Edge.new([vertex[0], index] as Array[int], triangle), Edge.new([index, vertex[1]] as Array[int], triangle)]
		splitting_vertex = index
		return index
	func child_without(v): #v must be one of the two vertice of self.
		return child[v==vertex[0] as int]
	func length_squared(tmpvertex_array):
		return (tmpvertex_array[vertex[0]].get_pos() - tmpvertex_array[vertex[1]].get_pos()).length_squared()

func _enter_tree():
	gen2(500,0.00001)

func compute_normals():
	normal.resize(len(point))
	normal.fill(Vector3.ZERO)
	for t in range(0, len(triangle), 3):
		var a = point[triangle[t]]
		var b = point[triangle[t+1]]
		var c = point[triangle[t+2]]
		var _normal = (b-a).cross(c-a)
		for j in range(3):
			normal[triangle[t+j]] += _normal
	for j in range(len(point)):
		normal[j] = normal[j].normalized()

func init_tetrahedron(radius, temppvertex_out, triangle_out):
	var a = Vector3(0,1,0)
	var b = Vector3(0.816497,-0.333333,-0.471405)
	var c =	Vector3(-0.816497,-0.333333,-0.471405)
	var d = Vector3(0,-0.333333,0.942809)
	for p in [a,b,c,d] :
		temppvertex_out.append(TempPlanetVertex.new(p, radius, Plate.new(p), randi_range(0, Biome.NB_BIOME-1) as Biome))
	var ab = Edge.new([0,1] as Array[int], [null,null] as Array[Triangle])
	var ac = Edge.new([0,2] as Array[int], [null,null] as Array[Triangle])
	var ad = Edge.new([0,3] as Array[int], [null,null] as Array[Triangle])
	var bc = Edge.new([1,2] as Array[int], [null,null] as Array[Triangle])
	var bd = Edge.new([1,3] as Array[int], [null,null] as Array[Triangle])
	var cd = Edge.new([2,3] as Array[int], [null,null] as Array[Triangle])
	triangle_out.append(Triangle.new([ab, ac, bc] as Array[Edge], null))
	triangle_out.append(Triangle.new([ab, ad, bd] as Array[Edge], null))
	triangle_out.append(Triangle.new([bc, cd, bd] as Array[Edge], null))
	triangle_out.append(Triangle.new([ac, ad, cd] as Array[Edge], null))

func gen_mesh():
	var surface_array = []
	surface_array.resize(Mesh.ARRAY_MAX)
	surface_array[Mesh.ARRAY_VERTEX] = point
	surface_array[Mesh.ARRAY_INDEX] = triangle
	#surface_array[Mesh.ARRAY_COLOR] = color
	surface_array[Mesh.ARRAY_NORMAL] = normal
	var mesh = ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, surface_array)
	var mesh_instance = MeshInstance3D.new()
	mesh_instance.mesh = mesh
	mesh_instance.material_override = StandardMaterial3D.new()
	mesh_instance.material_override.vertex_color_use_as_albedo = true
	return mesh_instance

func gen2(radius:float, points_per_m2:float):
	#var noise = FastNoiseLite.new()
	var sq_area_threshold = 1/points_per_m2**2
	var temp_triangle : Array[Triangle]
	var temp_pvertex : Array[TempPlanetVertex]
	init_tetrahedron(radius, temp_pvertex, temp_triangle)
	var final_triangle = []
	while not temp_triangle.is_empty():
		var t = temp_triangle.pop_back()
		if t.sq_area(temp_pvertex)>(sq_area_threshold):
			temp_triangle.append_array(t.split_in_4(temp_pvertex))
		else:
			final_triangle.append(t)
	var final_triangle_forrealthistime = []
	for t in final_triangle:
		var tmp = t.split_in_2()
		while not tmp.is_empty():
			final_triangle_forrealthistime.append_array(tmp.pop_back().split_in_2())
	for tmp in temp_pvertex:
		color.append(tmp.vertex.color)
		point.append(tmp.get_pos())
	for t in final_triangle_forrealthistime:
		triangle.append_array(t.get_vertex(temp_pvertex))
	compute_normals()
	var mesh_instance = gen_mesh()
	var collision_shape = CollisionShape3D.new()
	collision_shape.shape = mesh_instance.mesh.create_trimesh_shape()
	add_child(mesh_instance)
	add_child(collision_shape)
	
