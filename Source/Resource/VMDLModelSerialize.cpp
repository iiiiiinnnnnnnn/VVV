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
	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(metalnessTextureFileName), CEREAL_NVP(roughnessTextureFileName),
			CEREAL_NVP(metalnessTextureDDS), CEREAL_NVP(roughnessTextureDDS), CEREAL_NVP(shaderId));
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

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(transform));
}

template <class Archive> void VMDLModel::VmdlCollider::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(layer), CEREAL_NVP(nodeIndex), CEREAL_NVP(shape),
		CEREAL_NVP(center), CEREAL_NVP(rotation), CEREAL_NVP(size), CEREAL_NVP(trigger));

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(transform), CEREAL_NVP(navMeshObstacle));
}

template <class Archive> void VMDLModel::VmdlSpring::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetPosition),
		CEREAL_NVP(offsetRotation), CEREAL_NVP(stiffness), CEREAL_NVP(drag));

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(transform), CEREAL_NVP(maxAngleDegrees), CEREAL_NVP(minAnglesDegrees), CEREAL_NVP(maxAnglesDegrees));
}

template <class Archive> void VMDLModel::VmdlSpringCollider::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetPosition), CEREAL_NVP(radius));

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(transform));
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

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(transform));
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

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(syncOffset));
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

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(meshKeys), CEREAL_NVP(cacheMeshIndices));
}

template <class Archive> void VMDLModel::VmdlMultiLegIKSettings::serialize(Archive& archive)
{
	archive(CEREAL_NVP(bodyHeightOffset), CEREAL_NVP(contactOffset),
		CEREAL_NVP(maxUpCorrection), CEREAL_NVP(maxDownCorrection));

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(interpolationEasing));
}

template <class Archive> void VMDLModel::VmdlFootWeightTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(sampleRate), CEREAL_NVP(weights));
}

template <class Archive> void VMDLModel::VmdlAnimationEditorData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(footWeightTracks));

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(ikContactOffsetTracks));
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

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(transform), CEREAL_NVP(effectName), CEREAL_NVP(modelDerived), CEREAL_NVP(billboard), CEREAL_NVP(rendererType), CEREAL_NVP(parentEmitterIndex), CEREAL_NVP(ribbonRootOffset), CEREAL_NVP(ribbonTipOffset), CEREAL_NVP(ribbonLifetime), CEREAL_NVP(ribbonMaxPoints), CEREAL_NVP(ribbonTipRatio), CEREAL_NVP(ribbonSampleInterval), CEREAL_NVP(ribbonEndColor));
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

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(transform), CEREAL_NVP(soundPath), CEREAL_NVP(variant), CEREAL_NVP(pitchMin), CEREAL_NVP(pitchMax));
}

template <class Archive> void VMDLModel::VmdlSoundSourceBinding::serialize(Archive& archive)
{
	archive(CEREAL_NVP(soundPath), CEREAL_NVP(variant), CEREAL_NVP(pitchMin), CEREAL_NVP(pitchMax));
}

template <class Archive> void VMDLModel::VmdlSoundKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(sourceIndex), CEREAL_NVP(variant),
		CEREAL_NVP(volume), CEREAL_NVP(pitchMin), CEREAL_NVP(pitchMax));
}

template <class Archive> void VMDLModel::VmdlSoundAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(keys));
}

template <class Archive> void VMDLModel::VmdlSoundData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(sources), CEREAL_NVP(tracks));
}

template <class Archive> void VMDLModel::VmdlPointLight::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(color), CEREAL_NVP(intensity),
		CEREAL_NVP(range), CEREAL_NVP(active), CEREAL_NVP(transform));

	if constexpr (IsSerialArchive<Archive>)
		archive(CEREAL_NVP(transform));
}

template <class Archive> void VMDLModel::VmdlLightData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(pointLights));
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
template void VMDLModel::VmdlPointLight::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlPointLight::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);
template void VMDLModel::VmdlLightData::serialize<cereal::BinaryInputArchive>(
	cereal::BinaryInputArchive&);
template void VMDLModel::VmdlLightData::serialize<cereal::BinaryOutputArchive>(
	cereal::BinaryOutputArchive&);

template <class Archive> void VMDLModel::VmdlCameraShake::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(range), CEREAL_NVP(distanceAttenuation), CEREAL_NVP(duration), CEREAL_NVP(intensity), CEREAL_NVP(transform));
}

template <class Archive> void VMDLModel::VmdlRadialBlur::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(range), CEREAL_NVP(distanceAttenuation), CEREAL_NVP(duration), CEREAL_NVP(power), CEREAL_NVP(attackRate), CEREAL_NVP(transform));
}

template <class Archive> void VMDLModel::VmdlPresentationKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(componentIndex));
}

template <class Archive> void VMDLModel::VmdlPresentationAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(keys));
}

template <class Archive> void VMDLModel::VmdlPresentationData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(cameraShakes), CEREAL_NVP(radialBlurs), CEREAL_NVP(cameraShakeTracks), CEREAL_NVP(radialBlurTracks));
}

template void VMDLModel::Node::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::Node::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlComponentTransform::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlComponentTransform::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlComponentTransformData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlComponentTransformData::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::Material::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::Material::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::MaterialPbrSettings::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::MaterialPbrSettings::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::MaterialVMatSettings::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::MaterialVMatSettings::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlMaterialData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlMaterialData::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::Vertex::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::Vertex::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::Bone::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::Bone::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::Mesh::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::Mesh::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VectorKeyframe::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VectorKeyframe::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::QuaternionKeyframe::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::QuaternionKeyframe::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::FootIKRange::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::FootIKRange::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::NodeAnim::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::NodeAnim::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::Animation::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::Animation::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlRigidBody::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlRigidBody::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlCollider::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlCollider::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlSpring::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlSpring::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlSpringCollider::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlSpringCollider::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlMorph::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlMorph::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::ExternalMeshGroup::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::ExternalMeshGroup::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlTrail::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlTrail::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlExtensionData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlExtensionData::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlIKLeg::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlIKLeg::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlIKSettings::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlIKSettings::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlIKPole::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlIKPole::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlIKRaySettings::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlIKRaySettings::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlMultiLegIKSettings::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlMultiLegIKSettings::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlFootWeightTrack::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlFootWeightTrack::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlAnimationEditorData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlAnimationEditorData::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlBoolKeyframe::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlBoolKeyframe::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlColliderAnimationTrack::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlColliderAnimationTrack::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlMorphKeyframe::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlMorphKeyframe::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlMorphAnimationTrack::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlMorphAnimationTrack::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlTrailAnimationTrack::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlTrailAnimationTrack::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlTrailData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlTrailData::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlParticleEmitter::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlParticleEmitter::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlParticleAnimationTrack::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlParticleAnimationTrack::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlParticleData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlParticleData::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlAnimationControlData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlAnimationControlData::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlSoundSource::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlSoundSource::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlSoundSourceBinding::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlSoundSourceBinding::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlSoundKeyframe::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlSoundKeyframe::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlSoundAnimationTrack::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlSoundAnimationTrack::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlSoundData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlSoundData::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlPointLight::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlPointLight::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlLightData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlLightData::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlCameraShake::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlCameraShake::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlRadialBlur::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlRadialBlur::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlPresentationKeyframe::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlPresentationKeyframe::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlPresentationAnimationTrack::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlPresentationAnimationTrack::serialize<SerialOutputArchive>(SerialOutputArchive&);

template void VMDLModel::VmdlPresentationData::serialize<SerialInputArchive>(SerialInputArchive&);

template void VMDLModel::VmdlPresentationData::serialize<SerialOutputArchive>(SerialOutputArchive&);
