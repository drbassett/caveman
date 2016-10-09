#include <cassert>

//TODO override STB's memory allocation functions
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "platform.h"

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

struct GlyphMetrics
{
	i32 offsetTop, offsetLeft;
	u32 advanceX;
};

struct AsciiFont
{
	u32 bitmapWidth, bitmapHeight;
	u32 advanceY;
	GlyphMetrics glyphMetrics[256];
	u8 *bitmaps;
};

enum struct ShapeType
{
	Rectangle,
	Line,
};

struct Shape
{
	ColorU8 color;
	ShapeType type;
	union
	{
		RectF32 rect;
		LineF32 line;
	} data;
};

const u32 maxShapeCount = 1024;

struct Application
{
	MemStack scratchMem;

	AsciiFont font;

	f32 viewportX;
	f32 viewportY;
	// The viewportSize represents both the width and height
	// of the viewport. The viewport is always square to prevent
	// the rasterized image from stretching.
//TODO prevent the viewport size from becoming negative or close zero.
	f32 viewportSize;

	Bitmap canvas;
	bool drawCanvas;

//TODO allow the capacity of the shapes array to grow
	u32 shapeCount;
	Shape shapes[maxShapeCount];
};

inline MemStack newMemStack(size_t capacity)
{
	MemStack m = {};
	m.floor = (u8*) PLATFORM_alloc(capacity);
	assert(m.floor != nullptr);
	m.top = m.floor;
	m.ceiling = m.floor + capacity;
	return m;
}

inline u8* allocate(MemStack& m, size_t size)
{
	size_t capacity = m.ceiling - m.top;
	if (size > capacity)
	{
//TODO Handle overflow. Probably want to allocate additional
// memory. Reallocating the stack will invalidate pointers,
// requires keeping in memory both the old and new memory chunks,
// and could require a large data copy from the old to new memory.
// Could create a linked list of stacks. This is a little more
// complex to manage, but should work well. Will worry about this
// more later, when we have a better idea of the application's
// allocation patterns.
//
// Additional allocation could fail - in this case, we can either
// tell the user to download more RAM, or close the application.
		assert(false);
		volatile u8 **bad = nullptr;
		return (u8*) (*bad);
	}
	auto p = m.top;
	m.top += size;
	return p;
}

inline MemMarker mark(MemStack m)
{
	return MemMarker{m.top};
}

inline void release(MemStack& m, MemMarker marker)
{
	assert(m.top - marker._0 >= 0);
	m.top = marker._0;
}

inline u32 roundUpPowerOf2(u32 a)
{
	// Thanks to the Bit Twiddling Hacks page for this:
	// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn 
	--a;
	a |= a >> 1;
	a |= a >> 2;
	a |= a >> 4;
	a |= a >> 8;
	a |= a >> 16;
	return a + 1;
}

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

inline static size_t cStringLength(const char *str)
{
	const char *strBegin = str;
	while (*str != '\0')
	{
		++str;
	}
	return str - strBegin;
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

void drawText(
	const AsciiFont& font,
	Bitmap canvas,
	const char *strBegin,
	const char *strEnd,
	i32 leftEdge,
	i32 baseline,
	ColorU8 textColor)
{
	u32 bmpSize = font.bitmapWidth * font.bitmapHeight;
	while (strBegin != strEnd)
	{
		char c = *strBegin;
		++strBegin;

		GlyphMetrics glyph = font.glyphMetrics[c];

		i32 glyphX = leftEdge + glyph.offsetLeft;

		// clip the left edge
		i32 bmpStartCol;
		if (glyphX < 0)
		{
			bmpStartCol = -glyphX;
			glyphX = 0;
		} else
		{
			bmpStartCol = 0;
		}

		// clip the right edge
		i32 bmpEndCol;
		if (glyphX + (i32) font.bitmapWidth >= (i32) canvas.width)
		{
			bmpEndCol = (i32) canvas.width - glyphX;
		} else
		{
			bmpEndCol = font.bitmapWidth;
		}

		i32 glyphY = baseline - glyph.offsetTop;

		// clip the bottom edge
		i32 bmpEndRow;
		if (glyphY < (i32) font.bitmapHeight - 1)
		{
			bmpEndRow = glyphY + 1;
		} else
		{
			bmpEndRow = font.bitmapHeight;
		}

		// clip the top edge
		i32 bmpStartRow;
		if (glyphY >= (i32) canvas.height)
		{
			bmpStartRow = glyphY - (i32) canvas.height + 1;
			glyphY = canvas.height - 1;
		} else
		{
			bmpStartRow = 0;
		}

		auto pCanvas = canvas.pixels + canvas.pitch * glyphY;
		auto pBmp = font.bitmaps + bmpSize * c + bmpStartRow * font.bitmapWidth;
		for (i32 row = bmpStartRow; row < bmpEndRow; ++row)
		{
			auto pCanvasRow = pCanvas + 4 * glyphX;
			auto pBmpRow = pBmp + bmpStartCol;
			for (i32 col = bmpStartCol; col < bmpEndCol; ++col)
			{
				u8 alpha = *pBmpRow;
//TODO increase the canvas bit depth for better alpha compositing
				pCanvasRow[0] = (u8) (((u16) alpha * (u16) textColor.b + (u16) pCanvasRow[0] * (u16) (255 - alpha)) >> 8);
				pCanvasRow[1] = (u8) (((u16) alpha * (u16) textColor.g + (u16) pCanvasRow[1] * (u16) (255 - alpha)) >> 8);
				pCanvasRow[2] = (u8) (((u16) alpha * (u16) textColor.r + (u16) pCanvasRow[2] * (u16) (255 - alpha)) >> 8);
//TODO compute the correct destination alpha value - it should be a combination of the canvas and text alphas
				pCanvasRow[3] = 255;
				pCanvasRow += 4;
				++pBmpRow;
			}
			pCanvas -= canvas.pitch;
			pBmp += font.bitmapWidth;
		}

		leftEdge += glyph.advanceX;
	}
}

void addShape(Application& app, Shape shape)
{
	if (app.shapeCount == maxShapeCount)
	{
		assert(false);
		return;
	}

	app.shapes[app.shapeCount] = shape;
	++app.shapeCount;
}

void addRect(Application& app, RectF32 rect, ColorU8 color)
{
	Shape shape = {};
	shape.color = color;
	shape.type = ShapeType::Rectangle;
	shape.data.rect = rect;
	addShape(app, shape);
}

void addLine(Application& app, LineF32 line, ColorU8 color)
{
	Shape shape = {};
	shape.color = color;
	shape.type = ShapeType::Line;
	shape.data.line = line;
	addShape(app, shape);
}

bool init(Application& app, FilePath ttfFile)
{
	app.drawCanvas = true;

//TODO tune this allocation size
	app.scratchMem = newMemStack(64 * 1024 * 1024);
	if (app.scratchMem.top == nullptr)
	{
		assert(false);
//TODO show error message to user
		return false;
	}

	{
		bool ttfLoadSuccessful = true;

		auto memMark = mark(app.scratchMem);

		ReadFileError readError;
		u8 *fileContents;
		size_t fileSize;
		PLATFORM_readWholeFile(
			app.scratchMem,
			ttfFile,
			readError,
			fileContents,
			fileSize);
		if (fileContents == nullptr)
		{
//TODO show error message to user
			assert(false);
			goto ttfLoadError;
		}

		u32 pixelsPerInch = 96;
		u32 fontPoint = 12;
		u32 fontPointsPerInch = 72;
		u32 bitmapHeightPx = roundUpPowerOf2(pixelsPerInch * fontPoint / fontPointsPerInch);
//TODO use font metrics to compute a tighter bound on the width
		u32 bitmapWidthPx = bitmapHeightPx;
		auto bitmapSizePx = bitmapWidthPx * bitmapHeightPx;

		app.font.bitmapWidth = bitmapWidthPx;
		app.font.bitmapHeight = bitmapHeightPx;

		stbtt_fontinfo font;
		if (!stbtt_InitFont(&font, fileContents, 0))
		{
//TODO show error message to user
			assert(false);
			goto ttfLoadError;
		}

		auto bitmapStorageSize = 256 * bitmapSizePx;
//TODO allocate this in a different memory pool
		app.font.bitmaps = (u8*) PLATFORM_alloc(bitmapStorageSize);
		if (app.font.bitmaps == nullptr)
		{
			assert(false);
			goto ttfLoadError;
		}

		f32 scale = stbtt_ScaleForPixelHeight(&font, (f32) bitmapHeightPx);
		i32 ascentUnscaled, descentUnscaled, lineGapUnscaled;
		stbtt_GetFontVMetrics(&font, &ascentUnscaled, &descentUnscaled, &lineGapUnscaled);
		app.font.advanceY = (u32) round(((f32) ascentUnscaled - descentUnscaled + lineGapUnscaled) * scale);

		u8 *nextBitmap = app.font.bitmaps;
		u8 c = 0;
		for (;;)
		{
			GlyphMetrics metrics = {};

			auto glyphIndex = stbtt_FindGlyphIndex(&font, c);

			stbtt_MakeGlyphBitmap(
				&font,
				nextBitmap,
				bitmapWidthPx, bitmapHeightPx,
				bitmapWidthPx,
				scale, scale,
				glyphIndex);

			i32 advanceXUnscaled, leftOffsetUnscaled;
			stbtt_GetGlyphHMetrics(&font, glyphIndex, &advanceXUnscaled, &leftOffsetUnscaled);

			metrics.advanceX = (u32) round((f32) advanceXUnscaled * scale);
			metrics.offsetLeft  = (i32) round((f32) leftOffsetUnscaled * scale);

			stbtt_GetGlyphBitmapBox(
				&font, glyphIndex,
				scale, scale,
				nullptr, &metrics.offsetTop, nullptr, nullptr);

			app.font.glyphMetrics[c] = metrics;

			nextBitmap += bitmapSizePx;

			if (c == 255)
			{
				break;
			}
			++c;
		}

		goto ttfLoadSuccess;

ttfLoadError:
		ttfLoadSuccessful = false;
ttfLoadSuccess:
		release(app.scratchMem, memMark);
		if (!ttfLoadSuccessful)
		{
			return false;
		}
	}

	app.viewportX = -1.0;
	app.viewportY = -1.0;
	app.viewportSize = 2.0;

	assert(app.scratchMem.top == app.scratchMem.floor);

	return true;
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

		f32 viewportX = app.viewportX;
		f32 viewportY = app.viewportY;
		f32 scale = (f32) app.canvas.height / app.viewportSize;

		// Draw all shapes. A shape is transformed into window
		// space prior to drawing it.
		for (u32 i = 0; i < app.shapeCount; ++i)
		{
			Shape shape = app.shapes[i];
			switch (shape.type)
			{
			case ShapeType::Rectangle:
			{
				RectF32 rect = shape.data.rect;
				rect.x -= viewportX;
				rect.y -= viewportY;
				rect.x *= scale;
				rect.y *= scale;
				rect.width *= scale;
				rect.height *= scale;
				fillRect(app.canvas, rect, shape.color);
			} break;
			case ShapeType::Line:
			{
				LineF32 line = shape.data.line;
				line.x1 -= viewportX;
				line.y1 -= viewportY;
				line.x2 -= viewportX;
				line.y2 -= viewportY;
				line.x1 *= scale;
				line.y1 *= scale;
				line.x2 *= scale;
				line.y2 *= scale;
				drawLine(app.canvas, line, shape.color);
			} break;
			}
		}
	}

	assert(app.scratchMem.top == app.scratchMem.floor);
}

