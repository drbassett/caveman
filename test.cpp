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
	rect.x = -50.0f;
	rect.y = -50.0f;
	fillRect(canvas, rect, color);

	// top-left corner
	rect.x = -50.0f;
	rect.y = canvas.height - rect.height + 50.0f;
	fillRect(canvas, rect, color);

	// top-right corner
	rect.x = canvas.width - rect.width + 50.0f;
	rect.y = canvas.height - rect.height + 50.0f;
	fillRect(canvas, rect, color);

	// bottom-right corner
	rect.x = canvas.width - rect.width + 50.0f;
	rect.y = -50.0;
	fillRect(canvas, rect, color);

	// zero-area rectangle
	rect.x = 0.0f;
	rect.y = 0.0f;
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
	line.x1 = 0.0f;
	line.y1 = 0.0f;
	line.x2 = canvas.width - 1.0f;
	line.y2 = canvas.height - 1.0f;
	drawLine(canvas, line, color);

	// top-left to bottom-right corner
	line.x1 = 0.0f;
	line.y1 = canvas.height - 1.0f;
	line.x2 = canvas.width - 1.0f;
	line.y2 = 0.0f;
	drawLine(canvas, line, color);

	// along the left edge
	line.x1 = 0.0f;
	line.y1 = 0.0f;
	line.x2 = 0.0f;
	line.y2 = canvas.height - 1.0f;
	drawLine(canvas, line, color);

	// along the right edge
	line.x1 = canvas.width - 1.0f;
	line.y1 = 0.0f;
	line.x2 = canvas.width - 1.0f;
	line.y2 = canvas.height - 1.0f;
	drawLine(canvas, line, color);
	
	// along the bottom edge
	line.x1 = 0.0f;
	line.y1 = 0.0f;
	line.x2 = canvas.width - 1.0f;
	line.y2 = 0.0f;
	drawLine(canvas, line, color);

	// along the top edge
	line.x1 = 0.0f;
	line.y1 = canvas.height - 1.0f;
	line.x2 = canvas.width - 1.0f;
	line.y2 = canvas.height - 1.0f;
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
	f32 points[] =
	{
		-50.0f, -50.0f,
		-50.0f, halfCanvasHeight,
		-50.0f, canvasHeight + 50.0f,

		halfCanvasWidth, -50.0f,
		halfCanvasWidth, halfCanvasHeight,
		halfCanvasWidth, canvasHeight + 50.0f,

		canvasWidth + 50.0f, -50.0f,
		canvasWidth + 50.0f, halfCanvasHeight,
		canvasWidth + 50.0f, canvasHeight + 50.0f,
	};
	for (u32 i = 0; i < ArrayLength(points); i += 2)
	{
		line.x1 = points[i];
		line.y1 = points[i + 1];

		for (u32 j = 0; j < i; j += 2)
		{
			line.x2 = points[j];
			line.y2 = points[j + 1];
			drawLine(canvas, line, color);
		}

		for (u32 j = i + 2; j < ArrayLength(points); j += 2)
		{
			drawLine(canvas, line, color);
		}
	}

	// southwest clip region
	line.x1 = -10.0f;
	line.y1 = -10.0f;
	line.x2 = -20.0f;
	line.y2 = -20.0f;
	drawLine(canvas, line, color);

	// west clip region
	line.x1 = -10.0f;
	line.y1 = halfCanvasHeight;
	line.x2 = -20.0f;
	line.y2 = halfCanvasHeight;
	drawLine(canvas, line, color);

	// northwest clip region
	line.x1 = -10.0f;
	line.y1 = canvasHeight + 10.0f;
	line.x2 = -20.0f;
	line.y2 = canvasHeight + 20.0f;
	drawLine(canvas, line, color);

	// north clip region
	line.x1 = canvasWidth;
	line.y1 = canvasHeight + 10.0f;
	line.x2 = canvasWidth;
	line.y2 = canvasHeight + 20.0f;
	drawLine(canvas, line, color);

	// northeast clip region
	line.x1 = canvasWidth + 10.0f;
	line.y1 = canvasHeight + 10.0f;
	line.x2 = canvasWidth + 20.0f;
	line.y2 = canvasHeight + 20.0f;
	drawLine(canvas, line, color);

	// east clip region
	line.x1 = canvasWidth + 10.0f;
	line.y1 = halfCanvasHeight;
	line.x2 = canvasWidth + 20.0f;
	line.y2 = halfCanvasHeight;
	drawLine(canvas, line, color);

	// southeast clip region
	line.x1 = canvasWidth + 10.0f;
	line.y1 = -10.0f;
	line.x2 = canvasWidth + 20.0f;
	line.y2 = -20.0f;
	drawLine(canvas, line, color);

	// south clip region
	line.x1 = halfCanvasWidth;
	line.y1 = -10.0f;
	line.x2 = halfCanvasWidth;
	line.y2 = -20.0f;
	drawLine(canvas, line, color);

	// zero-length line
	line.x1 = 0.0f;
	line.y1 = 0.0f;
	line.x2 = 0.0f;
	line.y2 = 0.0f;
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

void testAddShapes(Application& app)
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
	rect.width = 50.0f;
	rect.height = 50.0f;

	LineF32 line = {};

	rect.x = 10.0f;
	rect.y = 10.0f;
	addRect(app, rect, red);

	line.x1 = 70.0f;
	line.y1 = 119.0f;
	line.x2 = 119.0f;
	line.y2 = 70.0f;
	addLine(app, line, white);

	rect.x = 70.0f;
	rect.y = 10.0f;
	addRect(app, rect, green);

	rect.x = 10.0f;
	rect.y = 70.0f;
	addRect(app, rect, blue);

	line.x1 = 70.0f;
	line.y1 = 70.0f;
	line.x2 = 119.0f;
	line.y2 = 119.0f;
	addLine(app, line, white);
}

