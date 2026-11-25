#pragma once

#include <algorithm>
#include <cmath>
#include <linalg.h>

struct ToneGrade
{
	float luminance = 1.f;
	float saturation = 1.f;
	float gamma = 1.f;
	bool operator==(const ToneGrade&) const = default;
};

struct TonalGrading
{
	ToneGrade shadows, midtones, highlights;

	bool neutral() const
	{
		return shadows == ToneGrade{} && midtones == ToneGrade{} && highlights == ToneGrade{};
	}

	// Input is the calibrated, gamma-encoded zone color. Select the bands once
	// so edits to brightness or saturation cannot change their own selection.
	void apply(linalg::aliases::float3& rgb) const
	{
		const float luma = linalg::dot(rgb, linalg::aliases::float3{0.2126f, 0.7152f, 0.0722f});
		const bool lowerHalf = luma < 0.5f;
		const ToneGrade& low = lowerHalf ? shadows : midtones;
		const ToneGrade& high = lowerHalf ? midtones : highlights;
		const float t = std::clamp((luma - (lowerHalf ? 0.2f : 0.5f)) / 0.3f, 0.f, 1.f);
		const float mix = t * t * (3.f - 2.f * t);
		const float luminance = std::lerp(low.luminance, high.luminance, mix);
		const float saturation = std::lerp(low.saturation, high.saturation, mix);
		const float gamma = std::lerp(low.gamma, high.gamma, mix);
		if (luminance == 1.f && saturation == 1.f && gamma == 1.f)
			return;

		const float peak = std::max({rgb.x, rgb.y, rgb.z});
		if (peak <= 0.f)
			return;
		const float minimum = std::min({rgb.x, rgb.y, rgb.z});
		const float value = std::min(1.f, luminance * (gamma == 1.f ? peak : std::pow(peak, gamma)));
		// Scale chroma around the HSV value, stopping at the gamut boundary
		// for all three components together to preserve hue.
		const float chromaGain = peak > minimum ? std::min(saturation, peak / (peak - minimum)) : 1.f;
		for (int channel = 0; channel < 3; ++channel)
			rgb[channel] = std::clamp(value * (1.f - chromaGain * ((peak - rgb[channel]) / peak)), 0.f, 1.f);
	}
};
