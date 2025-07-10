#pragma once

#include "godot_cpp/wrapper_constants.h"

#include "scene/resources/shader.h"

/*
namespace godot {
    class Shader : public ::Shader {
        GDSOFTCLASS(Shader, ::Shader);
    public:
        Array get_shader_uniform_list(bool p_get_groups) {
            List<PropertyInfo> uniform_list;
            ::Shader::get_shader_uniform_list(&uniform_list, p_get_groups);
            Array ret;
            for (const PropertyInfo &pi : uniform_list) {
                ret.push_back(pi.operator Dictionary());
            }
            return ret;
        }
    };
}
*/