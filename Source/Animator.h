// Animator.h

#pragma once

#include "Common.h"
#include "Model.h"

class Animator
{
public:
    Animator(Model* m);

    void Update(float elapsedTime);

    void Play(int index, bool loop = true);
    void Stop();

    const std::vector<Model::NodePose>& GetPoses() const;
	bool IsPlaying() const { return playing; }
	bool IsLoop() const { return loop; }
	void SetSpeed(float s) { speed = s; }
	void SetTime(float t) { currentTime = t; }
	void SetAnimationIndex(int index) { currentAnimationIndex = index; }
	void Reset() { currentTime = 0.0f; currentAnimationIndex = -1; playing = false; }
	void SetLoop(bool lp) { loop = lp; }

private:
    Model* model = nullptr;

    std::vector<Model::NodePose> nodePoses;

    float currentTime = 0.0f;
    int currentAnimationIndex = -1;

    bool playing = false;
    bool loop = true;
    float speed = 1.0f;
};