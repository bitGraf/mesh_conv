#pragma once

#include <laml/laml.hpp>

#include <vector>

namespace rh {

	// A single keyed (by frame time and track number) value for an animation
	template<typename T>
	struct AnimationKey
	{
		T Value;
		f32 FrameTime;       // 0.0f = beginning of animation clip, 1.0f = end of animation clip 
		u32 Track;

		AnimationKey(const f32 frameTime, const u32 track, const T& value)
			: FrameTime(frameTime)
			, Track(track)
			, Value(value)
		{}
	};
	using TranslationKey = AnimationKey<laml::Vec3>;
	using RotationKey = AnimationKey<laml::Quat>;
	using ScaleKey = AnimationKey<laml::Vec3>;

	// Animation is a collection of keyed values for translation, rotation, and scale of a number of "tracks"
	// Typically one "track" = one bone of a skeleton.
	// (but later we may also want to animate other things, so a "track" isn't necessarily a bone)
	class Animation
	{
	public:
		Animation(const std::string_view name, const float duration);

		const std::string& GetName() const { return m_Name; }
		float GetDuration() const { return m_Duration; }

		void SetKeyFrames(std::vector<TranslationKey> translations, std::vector<RotationKey> rotations, std::vector<ScaleKey> scales);

		const auto& GetTranslationKeys() const { return m_TranslationKeys; }
		const auto& GetRotationKeys() const { return m_RotationKeys; }
		const auto& GetScaleKeys() const { return m_ScaleKeys; }

	private:
		std::vector<TranslationKey> m_TranslationKeys;
		std::vector<RotationKey> m_RotationKeys;
		std::vector<ScaleKey> m_ScaleKeys;
		std::string m_Name;
		f32 m_Duration;
	};
}