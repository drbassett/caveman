#include <cassert>

struct RectF32
{
	f32 x, y, width, height;
};

struct ColorU8
{
	u8 r, g, b, a;
};

struct Bitmap
{
	u32 width, height, pitch;
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

static void fillGradient(Bitmap canvas)
{
	auto *pPixels = canvas.pixels;
	for (u32 y = 0; y < canvas.height; ++y)
	{
		auto pRow = pPixels;
		for (u32 x = 0; x < canvas.width; ++x)
		{
			f32 red = (f32) y / (f32) canvas.height * 255.0f;
			f32 green = (f32) x / (f32) canvas.width * 255.0f; 
			f32 blue = 0.0f;
			f32 alpha = 0.0f;

			pRow[0] = (u8) blue;
			pRow[1] = (u8) green;
			pRow[2] = (u8) red;
			pRow[3] = (u8) alpha;
			pRow += 4;
		}
		pPixels += canvas.pitch;
	}
}

static void fillRect(Bitmap canvas, RectF32 rect, ColorU8 color)
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

void update(Application& app)
{
	if (app.drawCanvas)
	{
		fillGradient(app.canvas);

		RectF32 rect = {};
		rect.width = 50.0f;
		rect.height = 50.0f;

		ColorU8 color = {};
		color.r = 0;
		color.g = 0;
		color.b = 255;
		color.a = 0;

		rect.x = 0.0f;
		rect.y = 0.0f;
		fillRect(app.canvas, rect, color);

		rect.x = app.canvas.width - rect.width;
		rect.y = 0.0;
		fillRect(app.canvas, rect, color);

		rect.x = 0.0f;
		rect.y = app.canvas.height - rect.height;
		fillRect(app.canvas, rect, color);

		rect.x = app.canvas.width - rect.width;
		rect.y = app.canvas.height - rect.height;
		fillRect(app.canvas, rect, color);
	}
}

