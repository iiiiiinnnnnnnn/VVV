// Animator.cpp

#include "Animator.h"

Animator::Animator(Model* m) : model(m)
{
}

void Animator::Play(int index, bool lp)
{
    if (!model) return;

    currentAnimationIndex = index;
    currentTime = 0.0f;
    playing = true;
    loop = lp;
}

void Animator::Stop()
{
    playing = false;
}

void Animator::Update(float elapsedTime)
{
    if (!model || !playing || currentAnimationIndex < 0)
        return;

    const Model::Animation& anim = model->GetAnimations()[currentAnimationIndex];

    model->ComputeAnimation(currentAnimationIndex, currentTime, nodePoses);
    model->SetNodePoses(nodePoses);

    currentTime += elapsedTime * speed;

    if (currentTime > anim.secondsLength)
    {
        if (loop)
            currentTime -= anim.secondsLength;
        else
            currentTime = anim.secondsLength;
    }
}

const std::vector<Model::NodePose>& Animator::GetPoses() const
{
    return nodePoses;
}