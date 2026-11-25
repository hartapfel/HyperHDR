#include <led-drivers/net/HueZoneBrightness.h>
#include <lut-calibrator/BoardSampling.h>

// The reference board survives 4:2:0 capture only when every shared
// chroma sample fits wholly inside a colored patch.
static_assert(BoardUtils::fitsNv12CalibrationBoard(256, 144));
static_assert(BoardUtils::fitsNv12CalibrationBoard(320, 180));
static_assert(BoardUtils::fitsNv12CalibrationBoard(360, 180));
static_assert(BoardUtils::fitsNv12CalibrationBoard(384, 216));
static_assert(!BoardUtils::fitsNv12CalibrationBoard(192, 108));
static_assert(!BoardUtils::fitsNv12CalibrationBoard(160, 90));

#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

bool checkFrame(const char* name, const std::vector<float>& input, float strength, float blackLevel,
	const std::vector<float>& expected)
{
	std::vector<linalg::aliases::float3> zones;
	for (size_t i = 0; i < input.size(); ++i)
		zones.emplace_back(0.1f * i, 0.2f, input[i]);
	const auto original = zones;
	balanceHueZoneBrightness(zones, strength, blackLevel);
	for (size_t i = 0; i < zones.size(); ++i)
	{
		if (zones[i].x != original[i].x || zones[i].y != original[i].y ||
			!(std::abs(zones[i].z - expected.at(i)) < 1e-6f))
		{
			std::cerr << name << ", zone " << i << ": expected brightness " << expected.at(i)
				<< ", got " << zones[i].z << '\n';
			return false;
		}
	}
	return true;
}

bool checkMidtoneResponse(bool linear)
{
	const float low = linear ? 0.025f : 0.17f;
	const float middle = linear ? 0.18f : 0.46f;
	const float high = linear ? 0.62f : 0.81f;
	// Sweep the full HueStream range, including black gates inside the lift window.
	for (float threshold : { 0.f, 0.009f, middle, high - 0.01f, 1.f })
		for (float strength : { 0.f, 0.75f, 1.f })
		{
			float previous = 0.f;
			for (int code = 0; code <= 65535; ++code)
			{
				const float input = code / 65535.f;
				const float output = liftHueMidtones(input, strength, threshold, linear);
				if (!(output >= input && output >= previous && output <= 1.f) ||
					((strength == 0.f || input <= std::max(threshold, low) || input >= high) && output != input))
				{
					std::cerr << "Invalid midtone response at code " << code << ", threshold " << threshold << '\n';
					return false;
				}
				previous = output;
			}
		}

	// A 75% slider gives a 9 percentage-point peak lift and half that
	// halfway to either shoulder, before the user's overall brightness factor.
	for (const auto& [input, lift] : { std::pair{low, 0.f}, std::pair{(low + middle) / 2.f, 0.045f},
		std::pair{middle, 0.09f}, std::pair{(middle + high) / 2.f, 0.045f}, std::pair{high, 0.f} })
	{
		std::vector<linalg::aliases::float3> zones(3, { 0.31f, 0.33f, input });
		balanceHueZoneBrightness(zones, 0.75f, 0.f);
		for (const auto& zone : zones)
			if (std::abs(liftHueMidtones(zone.z, 0.75f, 0.f, linear) - (input + lift)) > 0.00001f)
			{
				std::cerr << "Unexpected Hue stream brightness for " << input << '\n';
				return false;
			}
	}
	return true;
}

int main()
{
	return checkMidtoneResponse(true) && checkMidtoneResponse(false) &&
		checkFrame("disabled", { 0.1f, 0.9f }, 0, 0, { 0.1f, 0.9f }) &&
		checkFrame("half strength", { 0.1f, 0.9f }, 0.5f, 0, { 0.15f, 0.7f }) &&
		checkFrame("uniform", { 0.4f, 0.4f }, 1, 0, { 0.4f, 0.4f }) &&
		checkFrame("isolated highlight", { 0, 1, 0, 0 }, 1, 0, { 0, 0.25f, 0, 0 }) &&
		checkFrame("near-black noise", { 0.001f, 1 }, 1, 0, { 0.002f, 0.5005f }) &&
		checkFrame("black threshold", { 0.005f, 0.01f, 0.61f }, 1, 0.01f, { 0.005f, 0.01f, 0.21f }) &&
		checkFrame("blackout", { 0, 0 }, 1, 0, { 0, 0 }) &&
		checkFrame("single zone", { 0.8f }, 1, 0, { 0.8f }) &&
		checkFrame("empty", {}, 1, 0, {}) ? 0 : 1;
}
