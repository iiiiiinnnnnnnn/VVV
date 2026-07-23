// VMDLModelSerialize.cpp

#include "Resource/VMDLModel.h"
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>

template<class Archive>
void VMDLModel::Node::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(parentIndex),
		CEREAL_NVP(position),
		CEREAL_NVP(rotation),
		CEREAL_NVP(scale)
	);
}

template<class Archive>
void VMDLModel::Material::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(baseTextureFileName),
		CEREAL_NVP(normalTextureFileName),
		CEREAL_NVP(emissiveTextureFileName),
		CEREAL_NVP(occlusionTextureFileName),
		CEREAL_NVP(metalnessRoughnessTextureFileName),

		CEREAL_NVP(baseTextureDDS),
		CEREAL_NVP(normalTextureDDS),
		CEREAL_NVP(emissiveTextureDDS),
		CEREAL_NVP(occlusionTextureDDS),
		CEREAL_NVP(metalnessRoughnessTextureDDS),

		CEREAL_NVP(baseColor),
		CEREAL_NVP(emissiveColor),
		CEREAL_NVP(metalness),
		CEREAL_NVP(roughness),
		CEREAL_NVP(occlusionStrength),
		CEREAL_NVP(alphaCutoff),
		CEREAL_NVP(alphaMode)
	);
}

template<class Archive>
void VMDLModel::MaterialPbrSettings::serialize(Archive& archive)
{
	archive(CEREAL_NVP(occlusion), CEREAL_NVP(shadowStrength));
}

template<class Archive>
void VMDLModel::Vertex::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(position),
		CEREAL_NVP(boneWeight),
		CEREAL_NVP(boneIndex),
		CEREAL_NVP(texcoord),
		CEREAL_NVP(normal),
		CEREAL_NVP(tangent)
	);
}

template<class Archive>
void VMDLModel::Bone::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(nodeIndex),
		CEREAL_NVP(offsetTransform)
	);
}

template<class Archive>
void VMDLModel::Mesh::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(vertices),
		CEREAL_NVP(indices),
		CEREAL_NVP(bones),
		CEREAL_NVP(nodeIndex),
		CEREAL_NVP(materialIndex)
	);
}

template<class Archive>
void VMDLModel::VectorKeyframe::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(seconds),
		CEREAL_NVP(value)
	);
}

template<class Archive>
void VMDLModel::QuaternionKeyframe::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(seconds),
		CEREAL_NVP(value)
	);
}

template<class Archive>
void VMDLModel::FootIKRange::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(footIndex),
		CEREAL_NVP(startRatio),
		CEREAL_NVP(endRatio),
		CEREAL_NVP(weight),
		CEREAL_NVP(fadeInRatio),
		CEREAL_NVP(fadeOutRatio)
	);
}
template<class Archive>
void VMDLModel::NodeAnim::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(positionKeyframes),
		CEREAL_NVP(rotationKeyframes),
		CEREAL_NVP(scaleKeyframes)
	);
}

template<class Archive>
void VMDLModel::Animation::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(secondsLength),
		CEREAL_NVP(nodeAnims),
		CEREAL_NVP(footIKRanges)
	);
}

template<class Archive>
void VMDLModel::VmdlRigidBody::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name), CEREAL_NVP(nodeIndex),
		CEREAL_NVP(offsetPosition), CEREAL_NVP(offsetRotation),
		CEREAL_NVP(mass), CEREAL_NVP(kinematic));
}

template<class Archive>
void VMDLModel::VmdlCollider::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(nodeIndex),
		CEREAL_NVP(shape),
		CEREAL_NVP(center),
		CEREAL_NVP(rotation),
		CEREAL_NVP(size),
		CEREAL_NVP(trigger));
}

template<class Archive>
void VMDLModel::VmdlSpring::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name), CEREAL_NVP(nodeIndex),
		CEREAL_NVP(offsetPosition), CEREAL_NVP(offsetRotation),
		CEREAL_NVP(stiffness), CEREAL_NVP(drag));
}

template<class Archive>
void VMDLModel::VmdlSpringCollider::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetPosition), CEREAL_NVP(radius));
}

template<class Archive>
void VMDLModel::VmdlShape::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(meshVisibility));
}

template<class Archive>
void VMDLModel::VmdlTrail::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(rootOffset), CEREAL_NVP(tipOffset), CEREAL_NVP(color), CEREAL_NVP(tipRatio), CEREAL_NVP(lifeTime), CEREAL_NVP(maxPoints));
}

template<class Archive>
void VMDLModel::VmdlExtensionData::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(rootOffset),
		CEREAL_NVP(rigidBodies),
		CEREAL_NVP(colliders),
		CEREAL_NVP(springs),
		CEREAL_NVP(springColliders),
		CEREAL_NVP(shapes));
}

template<class Archive>
void VMDLModel::VmdlIKSettings::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(type),
		CEREAL_NVP(pelvis),
		CEREAL_NVP(leftThigh),
		CEREAL_NVP(leftCalf),
		CEREAL_NVP(leftFoot),
		CEREAL_NVP(leftBall),
		CEREAL_NVP(rightThigh),
		CEREAL_NVP(rightCalf),
		CEREAL_NVP(rightFoot),
		CEREAL_NVP(rightBall));
}

template<class Archive>
void VMDLModel::VmdlFootWeightTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(sampleRate), CEREAL_NVP(weights));
}

template<class Archive>
void VMDLModel::VmdlAnimationEditorData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(footWeightTracks));
}

template<class Archive>
void VMDLModel::VmdlBoolKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(value));
}

template<class Archive>
void VMDLModel::VmdlColliderAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(colliderIndex), CEREAL_NVP(keys));
}

template<class Archive>
void VMDLModel::VmdlShapeKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(shapeIndex));
}

template<class Archive>
void VMDLModel::VmdlShapeAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(keys));
}

template<class Archive>
void VMDLModel::VmdlTrailAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(trailIndex), CEREAL_NVP(keys));
}

template<class Archive>
void VMDLModel::VmdlTrailData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(trails), CEREAL_NVP(initialActive), CEREAL_NVP(tracks));
}

template<class Archive>
void VMDLModel::VmdlPlacementData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(scale), CEREAL_NVP(initialized));
}

template<class Archive>
void VMDLModel::VmdlAnimationControlData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(colliderInitialActive), CEREAL_NVP(colliderTracks), CEREAL_NVP(shapeTracks));
}
