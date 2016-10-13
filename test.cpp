void drawTestGradient(Bitmap canvas)
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

void testClearBitmap(Bitmap canvas)
{
	ColorU8 color = {};
	color.r = 255;
	color.g = 127;
	color.b = 0;
	color.a = 0;
	clearBitmap(canvas, color);
}

void drawTestRectangles(Bitmap canvas)
{
	ColorU8 color = {};
	color.r = 0;
	color.g = 0;
	color.b = 255;
	color.a = 0;

	RectF32 rect = {};
	rect.width = 100.0f;
	rect.height = 100.0f;

	// bottom-left corner
	rect.min = {-50.0f, -50.0f};
	fillRect(canvas, rect, color);

	// top-left corner
	rect.min = {
		-50.0f,
		canvas.height - rect.height + 50.0f};
	fillRect(canvas, rect, color);

	// top-right corner
	rect.min = {
		canvas.width - rect.width + 50.0f,
		canvas.height - rect.height + 50.0f};
	fillRect(canvas, rect, color);

	// bottom-right corner
	rect.min = {
		canvas.width - rect.width + 50.0f,
		-50.0};
	fillRect(canvas, rect, color);

	// zero-area rectangle
	rect.min = {0.0f, 0.0f};
	rect.width = 0.0f;
	rect.height = 0.0f;
	fillRect(canvas, rect, color);
}

void drawTestLines(Bitmap canvas)
{
	ColorU8 color = {};
	color.r = 255;
	color.g = 128;
	color.b = 0;
	color.a = 0;

	LineF32 line = {};

	// bottom-left to top-right corner
	line.p1 = {0.0f, 0.0f};
	line.p2 = {canvas.width - 1.0f, canvas.height - 1.0f};
	drawLine(canvas, line, color);

	// top-left to bottom-right corner
	line.p1 = {0.0f, canvas.height - 1.0f};
	line.p2 = {canvas.width - 1.0f, 0.0f};
	drawLine(canvas, line, color);

	// along the left edge
	line.p1 = {0.0f, 0.0f};
	line.p2 = {0.0f, canvas.height - 1.0f};
	drawLine(canvas, line, color);

	// along the right edge
	line.p1 = {canvas.width - 1.0f, 0.0f};
	line.p2 = {canvas.width - 1.0f, canvas.height - 1.0f};
	drawLine(canvas, line, color);
	
	// along the bottom edge
	line.p1 = {0.0f, 0.0f};
	line.p2 = {canvas.width - 1.0f, 0.0f};
	drawLine(canvas, line, color);

	// along the top edge
	line.p1 = {0.0f, canvas.height - 1.0f};
	line.p2 = {canvas.width - 1.0f, canvas.height - 1.0f};
	drawLine(canvas, line, color);

	auto canvasWidth = (f32) canvas.width;
	auto canvasHeight = (f32) canvas.height;
	auto halfCanvasWidth = (f32) (canvas.width >> 1);
	auto halfCanvasHeight = (f32) (canvas.height >> 1);

	color.r = 128;
	color.g = 0;
	color.b = 255;
	color.a = 0;

	// between clip regions
	Vec2 points[] =
	{
		{-50.0f, -50.0f},
		{-50.0f, halfCanvasHeight},
		{-50.0f, canvasHeight + 50.0f},

		{halfCanvasWidth, -50.0f},
		{halfCanvasWidth, halfCanvasHeight},
		{halfCanvasWidth, canvasHeight + 50.0f},

		{canvasWidth + 50.0f, -50.0f},
		{canvasWidth + 50.0f, halfCanvasHeight},
		{canvasWidth + 50.0f, canvasHeight + 50.0f},
	};
	for (u32 i = 0; i < ArrayLength(points); ++i)
	{
		line.p1 = points[i];

		for (u32 j = 0; j < i; ++j)
		{
			line.p2 = points[j];
			drawLine(canvas, line, color);
		}

		for (u32 j = i + 1; j < ArrayLength(points); ++j)
		{
			line.p2 = points[j];
			drawLine(canvas, line, color);
		}
	}

	// southwest clip region
	line.p1 = {-10.0f, -10.0f};
	line.p2 = {-20.0f, -20.0f};
	drawLine(canvas, line, color);

	// west clip region
	line.p1 = {-10.0f, halfCanvasHeight};
	line.p2 = {-20.0f, halfCanvasHeight};
	drawLine(canvas, line, color);

	// northwest clip region
	line.p1 = {-10.0f, canvasHeight + 10.0f};
	line.p2 = {-20.0f, canvasHeight + 20.0f};
	drawLine(canvas, line, color);

	// north clip region
	line.p1 = {canvasWidth, canvasHeight + 10.0f};
	line.p2 = {canvasWidth, canvasHeight + 20.0f};
	drawLine(canvas, line, color);

	// northeast clip region
	line.p1 = {canvasWidth + 10.0f, canvasHeight + 10.0f};
	line.p2 = {canvasWidth + 20.0f, canvasHeight + 20.0f};
	drawLine(canvas, line, color);

	// east clip region
	line.p1 = {canvasWidth + 10.0f, halfCanvasHeight};
	line.p2 = {canvasWidth + 20.0f, halfCanvasHeight};
	drawLine(canvas, line, color);

	// southeast clip region
	line.p1 = {canvasWidth + 10.0f, -10.0f};
	line.p2 = {canvasWidth + 20.0f, -20.0f};
	drawLine(canvas, line, color);

	// south clip region
	line.p1 = {halfCanvasWidth, -10.0f};
	line.p2 = {halfCanvasWidth, -20.0f};
	drawLine(canvas, line, color);

	// zero-length line
	line.p1 = {0.0f, 0.0f};
	line.p2 = {0.0f, 0.0f};
	drawLine(canvas, line, color);
}

void testDrawText(const AsciiFont& font, Bitmap canvas)
{
	ColorU8 textColor;
	textColor.r = 255;
	textColor.g = 255;
	textColor.b = 255;
	textColor.a = 255;

	// draw a variety of characters as several lines of text
	{
		const char *lines[] =
		{
			"abcdefghijklmnopqrstuvwxyz",
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
			"0123456789",
			R"(`~!@#$%^&*()_-+={[}]:;"'<,>.?/)",
			"The quick brown fox jumped over the lazy dog",
		};

		i32 baseline = canvas.height - 20;
		for (u32 i = 0; i < ArrayLength(lines); ++i)
		{
			auto line = lines[i];
			auto lineLength = cStringLength(line);
			auto lineEnd = line + lineLength;

			i32 leftEdge = 10;
			drawText(font, canvas, line, lineEnd, leftEdge, baseline, textColor);
			baseline -= font.advanceY;
		}
	}

	{
		auto c = 'A';
		auto strBegin = &c;
		auto strEnd = strBegin + 1;

		i32 xMin = -5;
		i32 yMin = -5;
		i32 xMax = canvas.width - 5;
		i32 yMax = canvas.height - 5;

		// draw a character in each corner to test clipping
		drawText(font, canvas, strBegin, strEnd, xMin, yMin, textColor);
		drawText(font, canvas, strBegin, strEnd, xMin, yMax, textColor);
		drawText(font, canvas, strBegin, strEnd, xMax, yMin, textColor);
		drawText(font, canvas, strBegin, strEnd, xMax, yMax, textColor);
	}
}

