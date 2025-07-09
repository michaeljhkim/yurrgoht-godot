#pragma once

#include "scene/3d/node_3d.h"
#define get_node_internal(args...) cast_to<Node>(call("get_node_internal", args).get_validated_object())