#pragma once
#include "allVector.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <algorithm>

class Model;

struct OBB
{
	Vector3 center;
	Vector3 orientations[3];
	Vector3 size;
};

struct AABB {
	Vector3 min;
	Vector3 max;
};

class Create3DObjectOBB
{

public:

	OBB CreateOBBForModel(
		const Model& model,
		const Vector3 position
	);



};

