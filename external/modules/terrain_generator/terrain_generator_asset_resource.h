// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#pragma once

#include <godot/core/io/resource.h>
#include "constants.h"

class TerrainGeneratorAssets;

// Parent class of TerrainGeneratorMeshAsset and TerrainGeneratorTextureAsset
class TerrainGeneratorAssetResource : public Resource {
	GDCLASS(TerrainGeneratorAssetResource, Resource);
	friend class TerrainGeneratorAssets;

public:
	TerrainGeneratorAssetResource() {}
	~TerrainGeneratorAssetResource() {}

	virtual void clear() = 0;
	virtual void set_name(const String &p_name) = 0;
	virtual String get_name() const = 0;
	virtual void set_id(const int p_id) = 0;
	virtual int get_id() const = 0;

protected:
	String _name;
	int _id = 0;
};