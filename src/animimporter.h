#pragma once

#include "skeleton.h"
#include "animation.h"

namespace rh {

	Skeleton* ImportSkeleton(const std::string_view filename);
	Skeleton* ImportSkeleton(const aiScene* scene);

	Animation* ImportAnimation(const std::string_view filename, const Skeleton* skeleton);
	std::vector<std::string> GetAnimationNames(const aiScene* scene);
	Animation* ImportAnimation(const aiScene* scene, const std::string_view animationName, const Skeleton* skeleton);
}