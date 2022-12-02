#pragma once

#include <laml/laml.hpp>
#include <assimp/anim.h>
#include <vector>

#include "skeleton.h"

namespace rh {

	template<typename T>
	struct AnimChannel {
		u32 num_samples;
		std::vector<T> values;
		std::vector<f32> frame_time;
	};

	const u32 CONSTANT_TRANSLATION = 1;
	const u32 CONSTANT_ROTATION = 2;
	const u32 CONSTANT_SCALE = 4;

	struct AnimNode {
		u32 flag;
		AnimChannel<laml::Vec3> translations;
		AnimChannel<laml::Quat> rotations;
		AnimChannel<laml::Vec3> scales;
	};

	struct Animation {
		std::vector<AnimNode> nodes;
		f32 duration;
		f32 frame_rate;

		u32 num_nodes;
		std::string name;
	};

	bool extract_animation(const aiAnimation* s_anim, Animation& anim, const Skeleton& skeleton);
}