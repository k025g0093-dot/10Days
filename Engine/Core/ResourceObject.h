#pragma once
#include "AllIncludeHeder.h"

class ResourceObject
{

public:
	ResourceObject(ID3D12Resource*resource):resource_(resource){}
	~ResourceObject() {
		//ここでリリースの宣言
		if (resource_) {
			resource_->Release();
		}
	}
	ID3D12Resource* Get() { return resource_; }
private:
	ID3D12Resource* resource_;

};

