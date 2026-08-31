#pragma once
#include <d3d12.h>
#include "allVector.h"

struct RawTransform
{
	Vector3 pos;
	float pad0;
	Vector3 rot;
	float pad1;
	Vector3 scale;
	float pad2;
};

struct InstanceData
{
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct IndexedIndirectCommand
{
	UINT baseInstance;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

struct DrawIndirectCommand
{
	UINT baseInstance;
	D3D12_DRAW_ARGUMENTS drawArguments;
};
