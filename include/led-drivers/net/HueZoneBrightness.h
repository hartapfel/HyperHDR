#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <linalg.h>

// Shape only the lower half of the Hue brightness command. The squared
// envelope joins the existing curve with unit slope at black and at the shoulder.
inline float liftHueShadows(float brightness, float strength, float blackLevel)
{
	constexpr float shoulder = 0.5f;
	if (strength <= 0.f || brightness <= blackLevel || brightness >= shoulder)
		return brightness;

	const float range = shoulder - blackLevel;
	const float t = (brightness - blackLevel) / range;
	// Fade in near black and taper the lift as brightness approaches the shoulder.
	const float envelope = (t / (t + 0.02f)) * (1.f - t);
	return brightness + strength * 0.25f * range * envelope * envelope;
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
