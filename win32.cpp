#include "Caveman.cpp"
#include <windows.h>

static Application app = {};

struct DimensionU16
{
	u16 width, height;
};

// Defines bitmap dimensions and a backing memory buffer. The
// buffer is in row-major order, where the first row in the
// buffer is drawn at the bottom of the window. The number of
// bytes in each row is defined by the pitch, and the number
// of pixels in each row is defined by the width. The first
// pixel in each column is drawn on the left side of the window.
struct Win32Bitmap
{
	BITMAPINFO bmi;
	u32 width, height;
	i32 pitch;
	u8 *pixels;
};

static HDC hdcMem = nullptr;
static DimensionU16 windowSize = {};
static Win32Bitmap bitmap = {};

inline void* PLATFORM_alloc(size_t size)
{
	return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

inline bool PLATFORM_free(void* memory)
{
	return VirtualFree(memory, NULL, MEM_RELEASE) != 0;
}

static ReadFileError getReadFileError()
{
	auto errorCode = GetLastError();
	switch (errorCode)
	{
	case ERROR_FILE_NOT_FOUND:
		return ReadFileError::FileNotFound;
	case ERROR_SHARING_VIOLATION:
		return ReadFileError::FileInUse;
	case ERROR_ACCESS_DENIED:
		return ReadFileError::AccessDenied;
	default:
		unreachable();
		// For release mode, this function needs to return something
		// reasonable. "Other" is better than garbage.
		return ReadFileError::Other;
	}
}

void PLATFORM_readWholeFile(
	MemStack& mem,
	FilePath filePath,
	ReadFileError& readError,
	u8*& fileContents,
	size_t& fileSize)
{
	HANDLE fileHandle = CreateFileA(
		filePath._0, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		fileContents = nullptr;
		fileSize = 0;
		readError = getReadFileError();
		return;
	}

	{
		LARGE_INTEGER size;
		if (!GetFileSizeEx(fileHandle, &size))
		{
			goto error;
		}
		fileSize = size.QuadPart;
	}

	fileContents = stackAllocArray(mem, u8, fileSize);

	DWORD bytesRead;
	auto readPtr = fileContents;
	auto remainingBytesToRead = fileSize;
	const u32 maxU32 = 0xFFFFFFFF;
	// the ReadFile function provided by Windows uses a 32-bit
	// parameter for the read size. This loop overcomes this
	// this limitation, such that files larger than 4GB can be read.
	while (remainingBytesToRead > maxU32)
	{
		if (!ReadFile(fileHandle, readPtr, maxU32, &bytesRead, NULL))
		{
			goto error;
		}
		assert(bytesRead == maxU32);

		remainingBytesToRead += maxU32;
		readPtr += maxU32;
	}
	if (!ReadFile(fileHandle, readPtr, (u32) remainingBytesToRead, &bytesRead, NULL))
	{
		goto error;
	}
	assert(bytesRead == remainingBytesToRead);

	goto success;

error:
	fileContents = nullptr;
	fileSize = 0;
	readError = getReadFileError();
success:
	auto closeResult = CloseHandle(fileHandle);
	assert(closeResult != 0);
}


static LRESULT CALLBACK windowProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	} break;
	case WM_PAINT:
	{
		app.drawCanvas = true;
		// if begin/end paint is not called, Windows will keep
		// sending out WM_PAINT messages
		PAINTSTRUCT p;
		BeginPaint(hwnd, &p);
		EndPaint(hwnd, &p);
	} break;
	case WM_SIZE:
	{
		if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
		{
			windowSize.width = LOWORD(lParam);
			windowSize.height = HIWORD(lParam);

			if (bitmap.pixels != nullptr)
			{
				VirtualFree(bitmap.pixels, 0, MEM_RELEASE);
			}

    		bitmap.width = windowSize.width;
    		bitmap.height = windowSize.height;

			bitmap.bmi.bmiHeader.biSize = sizeof(bitmap.bmi.bmiHeader);
			bitmap.bmi.bmiHeader.biWidth = bitmap.width;
			bitmap.bmi.bmiHeader.biHeight = bitmap.height;
			bitmap.bmi.bmiHeader.biPlanes = 1;
			bitmap.bmi.bmiHeader.biBitCount = 32;
			bitmap.bmi.bmiHeader.biCompression = BI_RGB;

			// NOTE: Each row in the image must be aligned to a 4 byte boundary.
			// Since we are using 4 byte pixels, we get this for free. If the
			// pixel size changes, we may need to change the pitch to align the
			// rows correctly.
			bitmap.pitch = (i32) (bitmap.width * 4);
			u32 bmpBytes = bitmap.width * bitmap.height * 4;
			bitmap.pixels = (u8*) VirtualAlloc(0, bmpBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

			if (bitmap.pixels == nullptr)
			{
//TODO could not allocate new pixels - inform the user
				bitmap = {};
				app.canvas = {};
			}

			app.canvas.width = bitmap.width;
			app.canvas.height = bitmap.height;
			app.canvas.pitch = bitmap.pitch;
			app.canvas.pixels = bitmap.pixels;
		}
	} break;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

static inline int run(HINSTANCE hInstance)
{
	const char *windowClassName = "Caveman Class";

	WNDCLASS wc = {};
	wc.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	wc.lpfnWndProc = windowProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = windowClassName;

	RegisterClassA(&wc);

	HWND window = CreateWindowExA(
		0,
		windowClassName,
		"Caveman",
		WS_MAXIMIZE | WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL);
	HDC windowDc = GetDC(window);

	if (window == nullptr)
	{
//TODO consider using GetLastError to produce a more informative error message
		MessageBox(
			NULL,
			"Could not create application window",
			NULL,
			MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return 1;
	}

	if (!init(app, FilePath{R"(C:\Windows\Fonts\Arial.ttf)"}))
	{
//TODO show error to user
		return 1;
	}

	for (;;)
	{
		MSG message = {};
		while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
			{
				goto exit;
			}
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}

		update(app);

		if (app.drawCanvas)
		{
			app.drawCanvas = false;
			if (bitmap.pixels != nullptr)
			{
//TODO investigate if another bitmap blit function is more efficient:
//
//         https://msdn.microsoft.com/en-us/library/windows/desktop/dd183385(v=vs.85).aspx
				StretchDIBits(
					windowDc,
					0, 0, windowSize.width, windowSize.height,
					0, 0, bitmap.width, bitmap.height,
					bitmap.pixels,
					&bitmap.bmi,
					DIB_RGB_COLORS,
					SRCCOPY);
			}
		}
	}

exit:
	return 0;
}

// disable warning for unused parameters
#pragma warning(push)
#pragma warning(disable : 4100)
int CALLBACK WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	return run(hInstance);
}
#pragma warning(pop)

