#include "DebugCamer.h"

void DebugCamer::Initialize(float width, float height) {
	width_ = width;
	height_ = height;
	matRot_ = MakeIdentity4x4();
}

void DebugCamer::Update() {
	if (!isDebug) {
		if (Input::GetKeyDown('1')) {
			isDebug = true;
		}
	}
	else {
		if (Input::GetKeyDown('1')) {
			isDebug = false;
		}
	}

	if (isDebug) {


		if (Input::GetMouseButton(0)) {
			float rotX = Input::GetMouseDeltaY() * 0.005f;
			float rotY = Input::GetMouseDeltaX() * 0.005f;

			Matrix4x4 matRotDelta = MakeIdentity4x4();
			matRotDelta = Multiply(matRotDelta, MakeRotateXMatrix(rotX));
			matRotDelta = Multiply(matRotDelta, MakeRotateYMatrix(rotY));

			matRot_ = Multiply(matRotDelta, matRot_);
		}

		if (Input::GetMouseButton(1)) {
			translate.x -= Input::GetMouseDeltaX() * 0.05f;
			translate.y += Input::GetMouseDeltaY() * 0.05f;
		}

		Matrix4x4 matTransCamera = MakeTranslateMatrix(translate);
		Matrix4x4 matTransPivot = MakeTranslateMatrix(pivot);
		Matrix4x4 cameraMatrix = Multiply(matTransCamera, matRot_);
		cameraMatrix = Multiply(cameraMatrix, matTransPivot);

		Matrix4x4 viewMatrix = Inverse(cameraMatrix);
		Matrix4x4 projMatrix = MakePerspectiveFovMatrix(0.45f, width_ / height_, 0.1f, 100.0f);
		viewProjectionMatrix_ = Multiply(viewMatrix, projMatrix);
	}
}