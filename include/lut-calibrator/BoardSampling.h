#pragma once

namespace BoardUtils
{
	// NV12 shares U and V across an aligned 2x2 pixel block. Select the block
	// whose center is nearest the patch center, away from the black surround.
	constexpr int chromaPairStart(int extent, int cells, int cell)
	{
		return 2 * ((2 * cell + 1) * extent / (4 * cells));
	}

	constexpr bool fitsCalibrationPatches(int extent, int cells, int patchSize, int margin)
	{
		for (int cell = 0; cell < cells; ++cell)
		{
			const int start = chromaPairStart(extent, cells, cell);
			const int scale = cells * patchSize;
			if (start * scale < (cell * patchSize + margin) * extent ||
				(start + 2) * scale > ((cell + 1) * patchSize - margin) * extent)
				return false;
		}
		return extent > 0;
	}

	constexpr bool fitsNv12CalibrationBoard(int width, int height)
	{
		// The reference 1920x1080 board uses 40x36 cells with 3x2 margins.
		return fitsCalibrationPatches(width, 48, 40, 3) && fitsCalibrationPatches(height, 30, 36, 2);
	}
}
