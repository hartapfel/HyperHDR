#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <linalg.h>

inline float scaleHueTone(float brightness, float multiplier, float blackLevel,
	double low, double middle, double shoulder)
{
	const double target = std::min(1.0, middle * multiplier);
	// Give large gains room to rejoin the highlight response without reversing it.
	const double high = std::clamp(target + (target - middle) / 2, shoulder, 1.0);
	if (multiplier == 1.f || brightness <= low || brightness <= blackLevel || brightness >= high)
		return brightness;

	const bool below = brightness < middle;
	const double width = below ? middle - low : high - middle;
	const double first = below ? low : target;
	const double last = below ? target : high;
	const double t = (brightness - (below ? low : middle)) / width;
	// Limit the cubic's endpoint slopes to keep the response monotone,
	// even when middle gray reaches full brightness.
	const double slope = std::min(width, 1.5 * (last - first));
	const double output = std::lerp(first, last, t) +
		(slope - (last - first)) * t * (1.0 - t) * (1.0 - 2.0 * t);
	if (blackLevel <= low)
		return static_cast<float>(output);

	// Approach an elevated V1 power-off threshold continuously.
	const double aboveGate = double(brightness) - blackLevel;
	return static_cast<float>(std::clamp(output, blackLevel + std::min(1.f, multiplier) * aboveGate,
		blackLevel + std::max(1.f, multiplier) * aboveGate));
}

inline float liftHueMidtones(float brightness, float multiplier, float blackLevel, bool linearRgb)
{
	return scaleHueTone(brightness, multiplier, blackLevel, linearRgb ? 0.025f : 0.17f,
		linearRgb ? 0.18f : 0.46f, linearRgb ? 0.62 : 0.81);
}

inline float liftHueShadows(float brightness, float multiplier, float blackLevel, bool linearRgb)
{
	// Five percent linear gray corresponds to about 25% in sRGB.
	return scaleHueTone(brightness, multiplier, blackLevel, linearRgb ? 0.002f : 0.026f,
		linearRgb ? 0.05f : 0.248f, linearRgb ? 0.18f : 0.46f);
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
