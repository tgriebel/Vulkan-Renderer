#pragma once



#pragma once
#include "../src/scene/entity.h"
#include "../src/scene/sceneBase.h"
#include <chrono>

class NesScene : public Scene
{
private:
public:
	NesScene() : Scene()
	{
	}

	void Update() override;
	void Init() override;
	void Shutdown() override;
};
