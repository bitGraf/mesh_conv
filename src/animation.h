#pragma once

#include <laml/laml.hpp>
#include <vector>

#include "skeleton.h"

namespace rh {

	const u32 CONSTANT_TRANSLATION = 1;
	const u32 CONSTANT_ROTATION = 2;
	const u32 CONSTANT_SCALE = 4;

	struct AnimNode {
		u32 flag;
        // each node is a list of transforms, one for each frame
		std::vector<laml::Vec3> translations;
        std::vector<laml::Quat> rotations;
        std::vector<laml::Vec3> scales;
	};

	struct Animation {
        // nodes is a list of animations assigned to each node
		f32 duration;
		f32 frame_rate;
        u32 num_nodes;
        u32 num_samples; // num_samples == nodes[n].translations.size()

		std::string name;
        std::vector<AnimNode> nodes;
	};

	//bool extract_animation(const aiAnimation* s_anim, Animation& anim, const Skeleton& skeleton);
}