#pragma once
#include "Component.h"

class MeshModel;

class MeshFilter : public Component {
public:
    MeshModel* model = nullptr;

    Component* Clone() const override {
        auto* mf = new MeshFilter();
        mf->model = model;
        return mf;
    }

};
