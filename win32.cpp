#include <windows.h>

LRESULT CALLBACK windowProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	} break;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

inline int run(HINSTANCE hInstance)
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

	if (window == NULL)
	{
//TODO consider using GetLastError to produce a more informative error message
		MessageBox(
			NULL,
			"Could not create application window",
			NULL,
			MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
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

