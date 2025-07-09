#pragma once

// Vector-Mutable

#include "core/variant/variant_utility.h"
using UtilityFunctions = VariantUtilityFunctions;

#include "core/templates/vector.h"
/*
template <typename T>
class VectorMut : public Vector<T> {	
public:
	// need original function due to overrides
	const T &operator[](Size p_index) const { return _cowdata.get(p_index); }
	T &operator[](Size p_index) { return _cowdata.get_m(p_index); }
	//void operator=(const Vector &p_from) { _cowdata = p_from._cowdata; }
	//void operator=(Vector &&p_from) { _cowdata = std::move(p_from._cowdata); }
};


// need to force the variant typedefs to use Vector-Mutable
#undef PackedByteArray
#undef PackedInt32Array
#undef PackedFloat32Array
#undef PackedRealArray

#define PackedByteArray VectorMut<uint8_t>
#define PackedInt32Array VectorMut<int32_t>
#define PackedFloat32Array VectorMut<float>
#define PackedRealArray VectorMut<real_t>
*/
//using PackedInt64Array    = VectorMut<int64_t>;
//using PackedFloat64Array  = VectorMut<double>;
//using PackedStringArray   = VectorMut<String>;
//using PackedStringArray 	= VectorMut<String>;
//using PackedVector2Array  = VectorMut<Vector2>;
//using PackedVector3Array  = VectorMut<Vector3>;
//using PackedColorArray    = VectorMut<Color>;
//using PackedVector4Array  = VectorMut<Vector4>