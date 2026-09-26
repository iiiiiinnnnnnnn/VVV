// VMDLModelSerialize.cpp
#include "Resource/VMDLModel.h"
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>

template <class Archive> void VMDLModel::Node::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(parentIndex), CEREAL_NVP(position), CEREAL_NVP(rotation),
		CEREAL_NVP(scale));
}

template <class Archive> void VMDLModel::Material::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(baseTextureFileName), CEREAL_NVP(normalTextureFileName),
		CEREAL_NVP(emissiveTextureFileName), CEREAL_NVP(occlusionTextureFileName),
		CEREAL_NVP(metalnessRoughnessTextureFileName), CEREAL_NVP(baseTextureDDS),
		CEREAL_NVP(normalTextureDDS), CEREAL_NVP(emissiveTextureDDS),
		CEREAL_NVP(occlusionTextureDDS), CEREAL_NVP(metalnessRoughnessTextureDDS),
		CEREAL_NVP(baseColor), CEREAL_NVP(emissiveColor), CEREAL_NVP(metalness),
		CEREAL_NVP(roughness), CEREAL_NVP(occlusionStrength), CEREAL_NVP(alphaCutoff),
		CEREAL_NVP(alphaMode));
}

template <class Archive> void VMDLModel::MaterialPbrSettings::serialize(Archive& archive)
{
	archive(CEREAL_NVP(occlusion), CEREAL_NVP(shadowStrength));
}

template <class Archive> void VMDLModel::MaterialVMatSettings::serialize(Archive& archive)
{
	archive(CEREAL_NVP(fresnelColor), CEREAL_NVP(fresnelPower), CEREAL_NVP(fresnelStrength),
		CEREAL_NVP(isFlatShading));
}

template <class Archive> void VMDLModel::VmdlMaterialData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(baseColor), CEREAL_NVP(emissiveColor),
		CEREAL_NVP(metalness), CEREAL_NVP(roughness), CEREAL_NVP(occlusion),
		CEREAL_NVP(occlusionStrength), CEREAL_NVP(shadowStrength), CEREAL_NVP(alphaCutoff),
		CEREAL_NVP(alphaMode), CEREAL_NVP(fresnelColor), CEREAL_NVP(fresnelPower),
		CEREAL_NVP(fresnelStrength), CEREAL_NVP(isFlatShading));
}

template <class Archive> void VMDLModel::Vertex::serialize(Archive& archive)
{
	archive(CEREAL_NVP(position), CEREAL_NVP(boneWeight), CEREAL_NVP(boneIndex),
		CEREAL_NVP(texcoord), CEREAL_NVP(normal), CEREAL_NVP(tangent));
}

template <class Archive> void VMDLModel::Bone::serialize(Archive& archive)
{
	archive(CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetTransform));
}

template <class Archive> void VMDLModel::Mesh::serialize(Archive& archive)
{
	archive(CEREAL_NVP(vertices), CEREAL_NVP(indices), CEREAL_NVP(bones), CEREAL_NVP(nodeIndex),
		CEREAL_NVP(materialIndex));
}

template <class Archive> void VMDLModel::VectorKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(value));
}

template <class Archive> void VMDLModel::QuaternionKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(value));
}

template <class Archive> void VMDLModel::FootIKRange::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(footIndex), CEREAL_NVP(startRatio), CEREAL_NVP(endRatio),
		CEREAL_NVP(weight), CEREAL_NVP(fadeInRatio), CEREAL_NVP(fadeOutRatio));
}

template <class Archive> void VMDLModel::NodeAnim::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(positionKeyframes), CEREAL_NVP(rotationKeyframes), CEREAL_NVP(scaleKeyframes));
}

template <class Archive> void VMDLModel::Animation::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(secondsLength), CEREAL_NVP(nodeAnims),
		CEREAL_NVP(footIKRanges));
}

template <class Archive> void VMDLModel::VmdlRigidBody::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetPosition),
		CEREAL_NVP(offsetRotation), CEREAL_NVP(mass), CEREAL_NVP(kinematic));
}

template <class Archive> void VMDLModel::VmdlCollider::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(layer), CEREAL_NVP(nodeIndex), CEREAL_NVP(shape),
		CEREAL_NVP(center), CEREAL_NVP(rotation), CEREAL_NVP(size), CEREAL_NVP(trigger));
}

template <class Archive> void VMDLModel::VmdlSpring::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetPosition),
		CEREAL_NVP(offsetRotation), CEREAL_NVP(stiffness), CEREAL_NVP(drag));
}

template <class Archive> void VMDLModel::VmdlSpringCollider::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetPosition), CEREAL_NVP(radius));
}

template <class Archive> void VMDLModel::VmdlMorph::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(meshVisibility), CEREAL_NVP(applyOnInitialize));
}

template <class Archive> void VMDLModel::VmdlTrail::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(rootOffset), CEREAL_NVP(tipOffset),
		CEREAL_NVP(color), CEREAL_NVP(tipRatio), CEREAL_NVP(lifeTime), CEREAL_NVP(maxPoints),
		CEREAL_NVP(offsetAngle));
}

template <class Archive> void VMDLModel::VmdlExtensionData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(rootOffset), CEREAL_NVP(rigidBodies), CEREAL_NVP(colliders),
		CEREAL_NVP(springs), CEREAL_NVP(springColliders), CEREAL_NVP(morphs));
}

template <class Archive> void VMDLModel::VmdlIKLeg::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name), CEREAL_NVP(root), CEREAL_NVP(mid), CEREAL_NVP(tip), CEREAL_NVP(contact));
}

template <class Archive> void VMDLModel::VmdlIKSettings::serialize(Archive& archive)
{
	archive(CEREAL_NVP(type), CEREAL_NVP(centerNode), CEREAL_NVP(legs));
}

template <class Archive> void VMDLModel::VmdlIKPole::serialize(Archive& archive)
{
	archive(CEREAL_NVP(custom), CEREAL_NVP(position));
}

template <class Archive> void VMDLModel::VmdlIKRaySettings::serialize(Archive& archive)
{
	archive(CEREAL_NVP(custom), CEREAL_NVP(startOffset), CEREAL_NVP(length));
}

template <class Archive> void VMDLModel::VmdlComponentTransform::serialize(Archive& archive)
{
	archive(CEREAL_NVP(position), CEREAL_NVP(rotation), CEREAL_NVP(scale));
}

template <class Archive> void VMDLModel::VmdlComponentTransformData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(rigidBodies), CEREAL_NVP(colliders), CEREAL_NVP(springs),
		CEREAL_NVP(springColliders), CEREAL_NVP(trails), CEREAL_NVP(particles),
		CEREAL_NVP(sounds), CEREAL_NVP(cameraShakes), CEREAL_NVP(radialBlurs));
}

template <class Archive> void VMDLModel::ExternalMeshGroup::serialize(Archive& archive)
{
	archive(CEREAL_NVP(path), CEREAL_NVP(meshIndices), CEREAL_NVP(initialVisibility));
}

template <class Archive> void VMDLModel::VmdlMultiLegIKSettings::serialize(Archive& archive)
{
	archive(CEREAL_NVP(bodyHeightOffset), CEREAL_NVP(contactOffset),
		CEREAL_NVP(maxUpCorrection), CEREAL_NVP(maxDownCorrection));
}

template <class Archive> void VMDLModel::VmdlFootWeightTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(sampleRate), CEREAL_NVP(weights));
}

template <class Archive> void VMDLModel::VmdlAnimationEditorData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(footWeightTracks));
}

template <class Archive> void VMDLModel::VmdlBoolKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(value));
}

template <class Archive> void VMDLModel::VmdlColliderAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(colliderIndex), CEREAL_NVP(keys));
}

template <class Archive> void VMDLModel::VmdlMorphKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(morphIndex));
}

template <class Archive> void VMDLModel::VmdlMorphAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(keys));
}

template <class Archive> void VMDLModel::VmdlTrailAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(trailIndex), CEREAL_NVP(keys));
}

template <class Archive> void VMDLModel::VmdlTrailData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(trails), CEREAL_NVP(initialActive), CEREAL_NVP(tracks));
}

template <class Archive> void VMDLModel::VmdlParticleEmitter::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(texturePath),
		CEREAL_NVP(columns), CEREAL_NVP(rows), CEREAL_NVP(frame), CEREAL_NVP(animated),
		CEREAL_NVP(animationSpeed), CEREAL_NVP(capacity), CEREAL_NVP(offset),
		CEREAL_NVP(spawnExtents), CEREAL_NVP(velocityMin), CEREAL_NVP(velocityMax),
		CEREAL_NVP(acceleration), CEREAL_NVP(emissionRate), CEREAL_NVP(burstCount),
		CEREAL_NVP(lifetimeMin), CEREAL_NVP(lifetimeMax), CEREAL_NVP(sizeMin),
		CEREAL_NVP(sizeMax), CEREAL_NVP(color), CEREAL_NVP(fadeInDuration),
		CEREAL_NVP(fadeOutDuration), CEREAL_NVP(localVelocity), CEREAL_NVP(additive));
}

template <class Archive> void VMDLModel::VmdlParticleAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(emitterIndex), CEREAL_NVP(keys));
}

template <class Archive> void VMDLModel::VmdlParticleData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(emitters), CEREAL_NVP(initialActive), CEREAL_NVP(tracks));
}

template <class Archive> void VMDLModel::VmdlAnimationControlData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(colliderInitialActive), CEREAL_NVP(colliderTracks), CEREAL_NVP(morphTracks));
}

template <class Archive> void VMDLModel::VmdlSoundSource::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(spatial), CEREAL_NVP(volume),
		CEREAL_NVP(minDistance), CEREAL_NVP(maxDistance), CEREAL_NVP(lowPassHz),
		CEREAL_NVP(farLowPassHz), CEREAL_NVP(reverbMix));
}

template <class Archive> void VMDLModel::VmdlSoundSourceBinding::serialize(Archive& archive)
{
	archive(CEREAL_NVP(track), CEREAL_NVP(variant), CEREAL_NVP(pitchMin), CEREAL_NVP(pitchMax));
}

template <class Archive> void VMDLModel::VmdlSoundKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(sourceIndex), CEREAL_NVP(track),
		CEREAL_NVP(variant), CEREAL_NVP(volume), CEREAL_NVP(pitchMin), CEREAL_NVP(pitchMax));
}

template <class Archive> void VMDLModel::VmdlSoundAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(keys));
}

template <class Archive> void VMDLModel::VmdlSoundData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(sources), CEREAL_NVP(tracks));
}

template void VMDLModel::Node::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive&);
template void VMDLModel::Node::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlComponentTransform::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlComponentTransform::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlComponentTransformData::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlComponentTransformData::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::Material::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::Material::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::MaterialPbrSettings::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::MaterialPbrSettings::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::MaterialVMatSettings::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::MaterialVMatSettings::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlMaterialData::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlMaterialData::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::Vertex::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive&);
template void VMDLModel::Vertex::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::Bone::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive&);
template void VMDLModel::Bone::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive&);
template void VMDLModel::Mesh::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive&);
template void VMDLModel::Mesh::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive&);
template void VMDLModel::VectorKeyframe::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VectorKeyframe::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::QuaternionKeyframe::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::QuaternionKeyframe::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::FootIKRange::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::FootIKRange::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::NodeAnim::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::NodeAnim::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::Animation::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::Animation::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlRigidBody::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlRigidBody::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlCollider::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlCollider::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlSpring::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlSpring::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlSpringCollider::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlSpringCollider::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlMorph::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlMorph::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::ExternalMeshGroup::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::ExternalMeshGroup::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlTrail::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlTrail::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlExtensionData::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlExtensionData::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlIKLeg::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlIKLeg::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlIKSettings::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlIKSettings::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlIKPole::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlIKPole::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlIKRaySettings::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlIKRaySettings::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlMultiLegIKSettings::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlMultiLegIKSettings::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlFootWeightTrack::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlFootWeightTrack::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlAnimationEditorData::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlAnimationEditorData::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlBoolKeyframe::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlBoolKeyframe::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlColliderAnimationTrack::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlColliderAnimationTrack::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlMorphKeyframe::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlMorphKeyframe::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlMorphAnimationTrack::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlMorphAnimationTrack::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlTrailAnimationTrack::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlTrailAnimationTrack::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlTrailData::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlTrailData::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlParticleEmitter::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlParticleEmitter::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlParticleAnimationTrack::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlParticleAnimationTrack::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlParticleData::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlParticleData::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlAnimationControlData::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlAnimationControlData::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlSoundSource::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlSoundSource::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlSoundSourceBinding::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlSoundSourceBinding::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlSoundKeyframe::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlSoundKeyframe::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlSoundAnimationTrack::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlSoundAnimationTrack::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlSoundData::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlSoundData::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
