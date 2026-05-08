// Animator.h

#pragma once

#include "Common.h"
#include "Model.h"
#include "Component.h"

class Animator : public Component
{
public:
    Animator(Actor* owner, std::shared_ptr<Model> model);

    void Update(float elapsedTime) override;

    void DrawGUI(float elapsedTime) override;

    void Play(int index, bool loop = true);

    void Stop();

	Model* GetModel() const { return model.get(); }
    const std::vector<Model::NodePose>& GetPoses() const { return nodePoses; }
	bool IsPlaying() const { return playing; }
	bool IsLoop() const { return loop; }
	void SetSpeed(float s) { speed = s; }
	void SetTime(float t) { currentTime = t; }
	void SetAnimationIndex(int index) { currentAnimationIndex = index; }
	void Reset() { currentTime = 0.0f; currentAnimationIndex = -1; playing = false; }
	void SetLoop(bool lp) { loop = lp; }

private:
    std::shared_ptr<Model> model = nullptr;

    std::vector<Model::NodePose> nodePoses;

    float currentTime = 0.0f;
    int currentAnimationIndex = -1;

    bool playing = false;
    bool loop = true;
    float speed = 1.0f;
};