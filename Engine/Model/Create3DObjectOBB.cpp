#include "Create3DObjectOBB.h"
#include "Model.h"

OBB Create3DObjectOBB::CreateOBBForModel(
	const Model& model,
	const Vector3 position
) {


	Vector3 positionMin = { FLT_MAX, FLT_MAX, FLT_MAX };
	Vector3 positionMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	const Vertex* verts = model.GetVertexData();
	UINT count = model.GetVertexCount();

	for (size_t j = 0; j < count; j++) {
		if (verts[j].position.x < positionMin.x) {
			positionMin.x = verts[j].position.x;
		}
		if (verts[j].position.x > positionMax.x) {
			positionMax.x = verts[j].position.x;
		}
		if (verts[j].position.y < positionMin.y) {
			positionMin.y = verts[j].position.y;
		}
		if (verts[j].position.y > positionMax.y) {
			positionMax.y = verts[j].position.y;
		}
		if (verts[j].position.z < positionMin.z) {
			positionMin.z = verts[j].position.z;
		}
		if (verts[j].position.z > positionMax.z) {
			positionMax.z = verts[j].position.z;
		}
	}

	OBB obb;
	obb.center = position + (positionMin + positionMax) * 0.5f;
	obb.size = (positionMax - positionMin) * 0.5f;

	//ワールドから回転行列を持ってくる
	Matrix4x4 worldMatrix = model.GetWorldMatrix();
	obb.orientations[0] = Vector3(worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2]).Normalized();  // X軸
	obb.orientations[1] = Vector3(worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2]).Normalized();  // Y軸
	obb.orientations[2] = Vector3(worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2]).Normalized();  // Z軸

	return obb;

}

