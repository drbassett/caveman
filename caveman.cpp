#include <cassert>

#define ArrayLength(a) (sizeof(a) / sizeof(a[0]))

struct RectF32
{
	f32 x, y, width, height;
};

struct LineF32
{
	f32 x1, y1, x2, y2;
};

struct ColorU8
{
	u8 r, g, b, a;
};

struct Bitmap
{
	u32 width, height;
	i32 pitch;
	u8 *pixels;
};

struct Application
{
	Bitmap canvas;
	bool drawCanvas;
};

inline static f32 clamp(f32 f, f32 min, f32 max)
{
	assert(min <= max);
	if (f < min)
	{
		return min;
	} else if (f > max)
	{
		return max;
	} else
	{
		return f;
	}
}

inline static void swap(i32& a, i32& b)
{
	auto tmp = a;
	a = b;
	b = tmp;
}

void clearBitmap(Bitmap canvas, ColorU8 color)
{
	auto *pPixels = canvas.pixels;
	for (u32 y = 0; y < canvas.height; ++y)
	{
		auto pRow = pPixels;
		for (u32 x = 0; x < canvas.width; ++x)
		{
			pRow[0] = color.b;
			pRow[1] = color.g;
			pRow[2] = color.r;
			pRow[3] = color.a;
			pRow += 4;
		}
		pPixels += canvas.pitch;
	}
}

void fillRect(Bitmap canvas, RectF32 rect, ColorU8 color)
{
	assert(rect.width >= 0.0);
	assert(rect.height >= 0.0);

	u32 clipXMin = (u32) clamp(rect.x, 0.0f, (f32) canvas.width);
	u32 clipXMax = (u32) clamp(rect.x + rect.width, 0.0f, (f32) canvas.width);
	u32 clipYMin = (u32) clamp(rect.y, 0.0f, (f32) canvas.height);
	u32 clipYMax = (u32) clamp(rect.y + rect.height, 0.0f, (f32) canvas.height);

	auto *pPixels = canvas.pixels + clipYMin * canvas.pitch;
	for (u32 y = clipYMin; y < clipYMax; ++y)
	{
		auto pRow = pPixels + clipXMin * 4;
		for (u32 x = clipXMin; x < clipXMax; ++x)
		{
			pRow[0] = color.b;
			pRow[1] = color.g;
			pRow[2] = color.r;
			pRow[3] = color.a;
			pRow += 4;
		}
		pPixels += canvas.pitch;
	}
}

const u8 COHEN_SUTHERLAND_LEFT_REGION = 0x1;
const u8 COHEN_SUTHERLAND_RIGHT_REGION = 0x2;
const u8 COHEN_SUTHERLAND_BOTTOM_REGION = 0x4;
const u8 COHEN_SUTHERLAND_TOP_REGION = 0x8;

static u8 cohenSutherlandComputeRegion(
	f32 xMin, f32 yMin, f32 xMax, f32 yMax, f32 x, f32 y)
{
	assert(xMin <= xMax);
	assert(yMin <= yMax);

	u8 region = 0;
	if (x < xMin)
	{
		region |= COHEN_SUTHERLAND_LEFT_REGION;
	}
	if (x > xMax)
	{
		region |= COHEN_SUTHERLAND_RIGHT_REGION;
	}
	if (y < yMin)
	{
		region |= COHEN_SUTHERLAND_BOTTOM_REGION;
	}
	if (y > yMax)
	{
		region |= COHEN_SUTHERLAND_TOP_REGION;
	}
	return region;
}

inline static void cohenSutherlandClipPoint(
	f32 xMin, f32 yMin, f32 xMax, f32 yMax, LineF32 line, u8& region, f32& x, f32& y)
{
	if (region & COHEN_SUTHERLAND_TOP_REGION)
	{
		x = line.x1 + (line.x2 - line.x1) * (yMax - line.y1) / (line.y2 - line.y1);
		y = yMax;
	} else if (region & COHEN_SUTHERLAND_BOTTOM_REGION)
	{
		x = line.x1 + (line.x2 - line.x1) * (yMin - line.y1) / (line.y2 - line.y1);
		y = yMin;
	} else if (region & COHEN_SUTHERLAND_RIGHT_REGION)
	{
		x = xMax;
		y = line.y1 + (line.y2 - line.y1) * (xMax - line.x1) / (line.x2 - line.x1);
	} else if (region & COHEN_SUTHERLAND_LEFT_REGION)
	{
		x = xMin;
		y = line.y1 + (line.y2 - line.y1) * (xMin - line.x1) / (line.x2 - line.x1);
	}

	region = cohenSutherlandComputeRegion(xMin, yMin, xMax, yMax, x, y);
}

inline static bool clipLineCohenSutherland(
	f32 xMin, f32 yMin, f32 xMax, f32 yMax, LineF32& line)
{
	assert(xMin <= xMax);
	assert(yMin <= yMax);

	u8 region1 = cohenSutherlandComputeRegion(
		xMin, yMin, xMax, yMax, line.x1, line.y1);
	u8 region2 = cohenSutherlandComputeRegion(
		xMin, yMin, xMax, yMax, line.x2, line.y2);

//TODO see if this loop can be unrolled - this may yield faster code
	for (;;)
	{
		if ((region1 | region2) == 0)
		{
			return true;
		}
		if ((region1 & region2) != 0)
		{
			return false;
		}
		
		if (region1 == 0)
		{
			cohenSutherlandClipPoint(
				xMin, yMin, xMax, yMax, line, region2, line.x2, line.y2);
		} else
		{
			cohenSutherlandClipPoint(
				xMin, yMin, xMax, yMax, line, region1, line.x1, line.y1);
		}
	}
}

void drawLine(Bitmap canvas, LineF32 line, ColorU8 color)
{
	// The main Bresenham's loop always runs at least one iteration,
	// and writes a pixel to the bitmap every iteration. When the
	// canvas has zero area, it has no memory allocated to it. Thus,
	// when the canvas has zero area, this function will access invalid
	// memory.
	assert(canvas.width > 0 && canvas.height > 0);

	f32 maxX = (f32) (canvas.width - 1);
	f32 maxY = (f32) (canvas.height - 1);
//TODO investigate the Liang-Barsky clipping algorithm
	if (!clipLineCohenSutherland(0.0f, 0.0f, maxX, maxY, line))
	{
		return;
	}

	i32 x1 = (i32) line.x1;
	i32 y1 = (i32) line.y1;
	i32 x2 = (i32) line.x2;
	i32 y2 = (i32) line.y2;

	assert(x1 >= 0);
	assert((u32) x1 < canvas.width);
	assert(x2 >= 0);
	assert((u32) x2 < canvas.width);

	assert(y1 >= 0);
	assert((u32) y1 < canvas.height);
	assert(y2 >= 0);
	assert((u32) y2 < canvas.height);

	if (x1 > x2)
	{
		swap(x1, x2);
		swap(y1, y2);
	}

	i32 dx = x2 - x1;
	i32 dy = y2 - y1;

	// Lines with a negative slope need to decrement rows rather
	// than increment them, and the start and end indices need
	// to be swapped.
	i32 incrY;
	u32 startY, endY;
	if (dy < 0)
	{
		incrY = -canvas.pitch;
		dy = -dy;
		startY = y2;
		endY = y1;
	} else
	{
		incrY = canvas.pitch;
		startY = y1;
		endY = y2;
	}

	// If the magnitude of the slope is greater than one, increment
	// the y value by one each iteration instead of the x value, and
	// increment the x value dependent on the error instead of the y
	// value. Otherwise, the line will not draw correctly since the
	// algorithm assumes that vertical changes will be either 0 or 1
	// pixels, which is not the case when the slope is greater than one.
	i32 i, endI, d1, d2;
	size_t incr1, incr2;
	if (dx >= dy)
	{
		i = x1;
		endI = x2;
		d1 = dy;
		d2 = dx;
		incr1 = 4;
		incr2 = incrY;
	} else
	{
		i = startY;
		endI = endY;
		d1 = dx;
		d2 = dy;
		incr1 = incrY;
		incr2 = 4;
	}

	// Bresenham's algorithm
	i32 error = 0;
	auto pPixels = canvas.pixels + y1 * canvas.pitch + 4 * x1;
	while (i <= endI)
	{
		pPixels[0] = color.b;
		pPixels[1] = color.g;
		pPixels[2] = color.r;
		pPixels[3] = color.a;
		pPixels += incr1;
		++i;

		error += d1;
		if ((error << 1) >= d2)
		{
			pPixels += incr2;
			error -= d2;
		}
	}
}

void update(Application& app)
{
	// If the canvas has no area (width or height is zero), no
	// pixels can be drawn, so we can skip drawing altogether.
	// This case also causes the line drawing algorithm to fail,
	// so this test avoids this problem as well.
	if (app.drawCanvas && app.canvas.width > 0 && app.canvas.height > 0)
	{
		ColorU8 background = {};
		clearBitmap(app.canvas, background);
	}
}

