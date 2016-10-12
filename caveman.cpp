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

enum struct ApplicationState
{
	DEFAULT,
	PANNING,
	ZOOMING,
};

struct Application
{
	MemStack scratchMem;

	AsciiFont font;

	ApplicationState state;
	i32 mouseX, mouseY;

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

	i32 panStartX, panStartY;
	i32 zoomStartY;

	bool selectShape;
	bool shapeSelected;
	u32 selectedShapeIndex;
};

inline f32 min(f32 a, f32 b)
{
	return a < b ? a : b;
}

inline f32 max(f32 a, f32 b)
{
	return a > b ? a : b;
}

inline f32 clamp(f32 a, f32 minVal, f32 maxVal)
{
	assert(minVal <= maxVal);
	return min(max(a, minVal), maxVal);
}

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

//TODO this function is for testing convenience - remove it eventually
static void addShapes(Application& app)
{
	ColorU8 red = {};
	red.r = 255;

	ColorU8 green = {};
	green.g = 255;

	ColorU8 blue = {};
	blue.b = 255;

	ColorU8 white = {};
	white.r = 255;
	white.g = 255;
	white.b = 255;

	RectF32 rect = {};
	rect.width = 0.4f;
	rect.height = 0.4f;

	LineF32 line = {};

	rect.x = -0.5f;
	rect.y = -0.5f;
	addRect(app, rect, red);

	line.x1 = 0.1f;
	line.y1 = 0.1f;
	line.x2 = 0.5f;
	line.y2 = 0.5f;
	addLine(app, line, white);

	rect.x = -0.5f;
	rect.y = 0.1f;
	addRect(app, rect, green);

	line.x1 = 0.1f;
	line.y1 = 0.5f;
	line.x2 = 0.5f;
	line.y2 = 0.1f;
	addLine(app, line, white);

	rect.x = 0.1f;
	rect.y = -0.5f;
	addRect(app, rect, blue);
}

bool init(Application& app, FilePath ttfFile)
{
	app.state = ApplicationState::DEFAULT;
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

	app.selectShape = false;
	app.shapeSelected = false;

	addShapes(app);

	assert(app.scratchMem.top == app.scratchMem.floor);

	return true;
}

inline static void selectShape(Application& app)
{
	f32 unitsPerPixel = app.viewportSize / (f32) app.canvas.height;
	f32 pixelsPerUnit = 1.0f / unitsPerPixel;
	// transform the mouse position to global coordinates
	f32 testX = ((f32) app.mouseX * unitsPerPixel) + app.viewportX;
	f32 testY = ((f32) app.mouseY * unitsPerPixel) + app.viewportY;
	// Shapes are drawn such that the last one is on top.
	// Iterate through the shapes backwards so that if shapes
	// are overlapping, the visible one is chosen.
	u32 i = app.shapeCount;
	while (i > 0)
	{
		--i;
		Shape shape = app.shapes[i];
		switch (shape.type)
		{
		case ShapeType::Rectangle:
		{
			RectF32 rect = shape.data.rect;
			f32 minX = rect.x;
			f32 maxX = minX + rect.width;
			f32 minY = rect.y;
			f32 maxY = minY + rect.height;
			if (testX >= minX
				&& testX <= maxX
				&& testY >= minY
				&& testY <= maxY)
			{
				goto hitShape;
			}
		} break;
		case ShapeType::Line:
		{
			LineF32 line = shape.data.line;
			f32 lineDy = line.y2 - line.y1;
			f32 lineDx = line.x2 - line.x1;
			f32 lineLengthSq = lineDx * lineDx + lineDy * lineDy;
			f32 closestX, closestY;
//TODO checking for exactly zero is not sufficient. Make this test more robust.
			if (lineLengthSq == 0.0f)
			{
				// This branch is taken if the line is a single point.
				// In this case, the x and y points are coincident, thus
				// the closest point on the line is either one of these.
				closestX = line.x1;
				closestY = line.y1;
			} else
			{
				// `t` represents the parameter in the parametric
				// equation of the line
				f32 t = ((testX - line.x1) * lineDx + (testY - line.y1) * lineDy) / lineLengthSq;
				// clamp `t` to the endpoints of the line
				t = clamp(t, 0.0f, 1.0f);
				// evaluate the line equation for the closest `t` to the test point
				closestX = line.x1 + t * lineDx;
				closestY = line.y1 + t * lineDy;
			}

			// find the squared distance to the test point
			f32 distSq;
			{
				f32 dx = closestX - testX;
				f32 dy = closestY - testY;
				distSq = dx * dx + dy * dy;
			}

			// distPx = dist * ppu = sqrt(distSq) * ppu
			// distPx^2 = (sqrt(distSq) * ppu)^2 = distSq * ppu^2
			f32 distSqPx = distSq * (pixelsPerUnit * pixelsPerUnit);

			// Clicking exactly on a 1 pixel-wide line is tricky. Allow
			// 2 pixels of slop on each side to make the task easier.
			f32 maxDistPx = 5.0f;
			if (distSqPx <= maxDistPx * maxDistPx)
			{
				goto hitShape;
			}
		} break;
		default:
			unreachable();
		}
	}
	app.shapeSelected = false;
	return;

hitShape:
	app.selectedShapeIndex = i;
	app.shapeSelected = true;
}

// draws rectangles centered at the given points
static void drawSelectedShapeMarkers(
	Bitmap canvas, u32 markerCount, f32* xPx, f32* yPx)
{
	ColorU8 yellow = {};
	yellow.r = 255;
	yellow.g = 255;
	yellow.b = 0;
	yellow.a = 255;

	f32 halfSizePx = 5.0f;
	f32 sizePx = 2.0f * halfSizePx;

	RectF32 rect = {};
	rect.width = sizePx;
	rect.height = sizePx;

	for (u32 i = 0; i < markerCount; ++i)
	{
		rect.x = xPx[i] - halfSizePx;
		rect.y = yPx[i] - halfSizePx;
		fillRect(canvas, rect, yellow);
	}
}

void update(Application& app)
{
	f32 unitsPerPixel = app.viewportSize / (f32) app.canvas.height;
	f32 pixelsPerUnit = 1.0f / unitsPerPixel;

	switch (app.state)
	{
	case ApplicationState::DEFAULT:
	{
		if (app.selectShape)
		{
			app.selectShape = false;
			selectShape(app);
			app.drawCanvas = true;
		}
	} break;
	case ApplicationState::PANNING:
	{
		f32 dxPixels = (f32) (app.mouseX - app.panStartX);
		f32 dyPixels = (f32) (app.mouseY - app.panStartY);
		f32 panSpeed = 1.0f;
		app.viewportX += panSpeed * unitsPerPixel * dxPixels;
		app.viewportY += panSpeed * unitsPerPixel * dyPixels;
		app.panStartX = app.mouseX;
		app.panStartY = app.mouseY;
		app.drawCanvas = true;
	} break;
	case ApplicationState::ZOOMING:
	{
//TODO zoom to cursor instead of zooming to the center of the screen
		f32 dyPixels = (f32) (app.zoomStartY - app.mouseY);
		f32 zoomSpeed = 0.0025f;
		f32 oldViewportSize = app.viewportSize;
		app.viewportSize *= (1.0f + zoomSpeed * dyPixels);
		f32 sizeChange = app.viewportSize - oldViewportSize;
		// Changing the viewport x/y zooms toward the center of the screen.
		// Simply changing the viewport size zooms toward the bottom left
		// corner, which feels unnatural.
		app.viewportX -= 0.5f * sizeChange;
		app.viewportY -= 0.5f * sizeChange;
		app.zoomStartY = app.mouseY;
		app.drawCanvas = true;
	} break;
	default:
		unreachable();
		break;
	}

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
				rect.x = (rect.x - viewportX) * pixelsPerUnit;
				rect.y = (rect.y - viewportY) * pixelsPerUnit;
				rect.width *= pixelsPerUnit;
				rect.height *= pixelsPerUnit;
				fillRect(app.canvas, rect, shape.color);
			} break;
			case ShapeType::Line:
			{
				LineF32 line = shape.data.line;
				line.x1 = (line.x1 - viewportX) * pixelsPerUnit;
				line.y1 = (line.y1 - viewportY) * pixelsPerUnit;
				line.x2 = (line.x2 - viewportX) * pixelsPerUnit;
				line.y2 = (line.y2 - viewportY) * pixelsPerUnit;
				drawLine(app.canvas, line, shape.color);
			} break;
			default:
				unreachable();
				break;
			}
		}

		// draw markers for the selected shape
		if (app.shapeSelected)
		{
			Shape shape = app.shapes[app.selectedShapeIndex];
			switch (shape.type)
			{
			case ShapeType::Rectangle:
			{
				RectF32 rect = shape.data.rect;
				rect.x = (rect.x - viewportX) * pixelsPerUnit;
				rect.y = (rect.y - viewportY) * pixelsPerUnit;
				rect.width *= pixelsPerUnit;
				rect.height *= pixelsPerUnit;

				f32 minX = rect.x;
				f32 minY = rect.y;
				f32 maxX = minX + rect.width;
				f32 maxY = minY + rect.height;

				f32 markersX[4] = {minX, minX, maxX, maxX};
				f32 markersY[4] = {minY, maxY, minY, maxY};
				drawSelectedShapeMarkers(app.canvas, 4, markersX, markersY);
			} break;
			case ShapeType::Line:
			{
				LineF32 line = shape.data.line;
				line.x1 = (line.x1 - viewportX) * pixelsPerUnit;
				line.y1 = (line.y1 - viewportY) * pixelsPerUnit;
				line.x2 = (line.x2 - viewportX) * pixelsPerUnit;
				line.y2 = (line.y2 - viewportY) * pixelsPerUnit;

				f32 markersX[2] = {line.x1, line.x2};
				f32 markersY[2] = {line.y1, line.y2};
				drawSelectedShapeMarkers(app.canvas, 2, markersX, markersY);
			} break;
			default:
				unreachable();
				break;
			}
		}

		// draw help text in upper-left corner
		{
			const char *stateText = "";
			switch (app.state)
			{
			case ApplicationState::DEFAULT:
				break;
			case ApplicationState::PANNING:
				stateText = "Panning";
				break;
			case ApplicationState::ZOOMING:
				stateText = "Zooming";
				break;
			default:
				unreachable();
				break;
			}

			const char *lines[] =
			{
				"Hold Q: Pan",
				"Hold Z: Zoom",
				"S: Select shape under cursor",
				stateText,
			};

			ColorU8 yellow = {};
			yellow.r = 255;
			yellow.g = 255;
			yellow.b = 0;
			yellow.a = 255;

			i32 baseline = app.canvas.height - app.font.advanceY;
			for (size_t i = 0; i < ArrayLength(lines); ++i)
			{
				const char *line = lines[i];
				size_t lineLength = cStringLength(line);
				const char *lineEnd = line + lineLength;
				i32 leftEdge = 5;
				drawText(app.font, app.canvas, line, lineEnd, leftEdge, baseline, yellow);
				baseline -= app.font.advanceY;
			}
		}
	}

	assert(app.scratchMem.top == app.scratchMem.floor);
}

