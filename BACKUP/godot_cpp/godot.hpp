#pragma once

// C++ headers
#include <array>

// gdextension
//#include "core/extension/gdextension.h"
#include "modules/register_module_types.h"
#include "core/variant/variant.h"
#include "modules/regex/regex.h"
#include "modules/register_module_types.h"
#include "servers/text/text_server_extension.h"

#include "classes/rendering_server.hpp"
#include "classes/editor_plugin.hpp"

// not sure if I should do this, it might be cyclical
namespace godot {}

#include "wrapper_constants.h"