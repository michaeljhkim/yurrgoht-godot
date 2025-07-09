#pragma once

#include "core/templates/list.h"
// Vector-Mutable

template <typename T>
class VectorMut : public Vector<T> {	
public:
	// need original function due to overrides
	_FORCE_INLINE_ const T &operator[](Size p_index) const { return _cowdata.get(p_index); }
	_FORCE_INLINE_ T &operator[](Size p_index) { return _cowdata.get_m(p_index) }
	//void operator=(const Vector &p_from) { _cowdata = p_from._cowdata; }
	//void operator=(Vector &&p_from) { _cowdata = std::move(p_from._cowdata); }
};

#include "core/variant/variant_utility.h"
using UtilityFunctions = VariantUtilityFunctions;

// need to force the variant typedefs to use Vector-Mutable
using PackedByteArray 	  = VectorMut<uint8_t>;
using PackedInt32Array 	  = VectorMut<int32_t>;
using PackedFloat32Array  = VectorMut<float>;
using PackedRealArray     = VectorMut<real_t>;

//using PackedInt64Array    = VectorMut<int64_t>;
//using PackedFloat64Array  = VectorMut<double>;
//using PackedStringArray   = VectorMut<String>;
//using PackedStringArray 	= VectorMut<String>;
//using PackedVector2Array  = VectorMut<Vector2>;
//using PackedVector3Array  = VectorMut<Vector3>;
//using PackedColorArray    = VectorMut<Color>;
//using PackedVector4Array  = VectorMut<Vector4>;

//#define push_error(pargs) push_error(sarray(pargs))