#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <linalg.h>

// The anchors represent roughly the same gray patches in either driver mode:
// 18% linear gray is about 46% in sRGB. Fade smoothly on both sides of it.
inline float liftHueMidtones(float brightness, float strength, float blackLevel, bool linearRgb)
{
	const float low = linearRgb ? 0.025f : 0.17f;
	const float middle = linearRgb ? 0.18f : 0.46f;
	const float high = linearRgb ? 0.62f : 0.81f;
	if (strength <= 0.f || brightness <= std::max(low, blackLevel) || brightness >= high)
		return brightness;

	const float t = brightness <= middle ? (brightness - low) / (middle - low)
		: (high - brightness) / (high - middle);
	const float weight = t * t * (3.f - 2.f * t);
	// An elevated V1 power-off threshold may fall inside the lift window.
	return brightness + strength * std::min(0.12f * weight, brightness - blackLevel);
}

// Hue XY + brightness values, after the driver's optional linear RGB conversion.
inline void balanceHueZoneBrightness(std::span<linalg::aliases::float3> zones, float strength, float blackLevel)
{
	if (strength <= 0.f || zones.size() < 2)
		return;

	// Balance only the signal above the power-off threshold. Black zones still
	// count toward the mean, so an isolated highlight in a dark scene is subdued.
	double sum = 0;
	for (const auto& zone : zones)
		sum += std::max(0.f, zone.z - blackLevel);

	const float mean = static_cast<float>(sum / zones.size());
	for (auto& zone : zones)
	{
		const float signal = zone.z - blackLevel;
		if (signal <= 0.f)
			continue;

		// Limit shadow lift to twice the local signal even at full strength.
		const float target = std::min(mean, 2.f * signal);
		zone.z = std::lerp(zone.z, blackLevel + target, strength);
	}
}
