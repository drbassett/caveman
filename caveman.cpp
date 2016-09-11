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

static void fillGradient(Bitmap bmp)
{
	auto *pPixels = bmp.pixels;
	for (u32 y = 0; y < bmp.height; ++y)
	{
		auto pRow = pPixels;
		for (u32 x = 0; x < bmp.width; ++x)
		{
			f32 red = (f32) y / (f32) bmp.height * 255.0f;
			f32 green = (f32) x / (f32) bmp.width * 255.0f; 
			f32 blue = 0.0f;
			f32 alpha = 0.0f;

			pRow[0] = (u8) blue;
			pRow[1] = (u8) green;
			pRow[2] = (u8) red;
			pRow[3] = (u8) alpha;
			pRow += 4;
		}
		pPixels += bmp.pitch;
	}
}

void update(Application& app)
{
	if (app.drawCanvas)
	{
		fillGradient(app.canvas);
	}
}

