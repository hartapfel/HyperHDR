#include <infinite-color-engine/TonalGrading.h>
#include <iostream>

using linalg::aliases::float3;

static bool matches(const TonalGrading& grade, float3 input, float3 expected)
{
	float3 actual = input;
	grade.apply(actual);
	for (int i = 0; i < 3; ++i)
		if (std::abs(actual[i] - expected[i]) > 1e-5f || !std::isfinite(actual[i]))
		{
			std::cerr << "Input " << input[i] << " expected " << expected[i] << " got " << actual[i] << '\n';
			return false;
		}
	return true;
}

int main()
{
	for (int band = 0; band < 3; ++band)
	{
		const float levels[] = {0.1f, 0.5f, 0.9f};
		TonalGrading grade;
		ToneGrade* bands[] = {&grade.shadows, &grade.midtones, &grade.highlights};
		for (int i = 0; i < 3; ++i)
		{
			*bands[band] = {0.5f, 1.f, 1.f};
			if (!matches(grade, float3(levels[i]), float3(levels[i] * (i == band ? 0.5f : 1.f))))
				return 1;
			*bands[band] = {1.f, 1.f, 2.f};
			if (!matches(grade, float3(levels[i]), float3(i == band ? levels[i]*levels[i] : levels[i])))
				return 1;
			*bands[band] = {1.f, 0.f, 1.f};
			// Preserve the target luma while introducing chroma.
			float3 color(levels[i] + 0.05f, levels[i] - 0.05f * 0.2126f/0.7152f, levels[i]);
			if (!matches(grade, color, i == band ? float3(color.x) : color))
				return 1;
		}
	}
	TonalGrading grade;
	grade.shadows.luminance = 2.f;
	if (!matches(grade, float3(0.35f), float3(0.525f)) || !matches(grade, float3(0.f), float3(0.f)))
		return 1;
	grade.shadows = grade.midtones = grade.highlights = {10.f, 10.f, 0.1f};
	return matches(grade, {0.4f, 0.3f, 0.2f}, {1.f, 0.5f, 0.f}) ? 0 : 1;
}
