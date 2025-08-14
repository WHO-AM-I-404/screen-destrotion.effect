#include <Windows.h>
#include <cstdlib>
#include <ctime>

const int REFRESH_RATE = 30; // ms
const int MAX_GLITCH_INTENSITY = 50;
const int GLITCH_LINES = 20;

HBITMAP hGlitchBitmap = NULL;
BYTE* pPixels = NULL;
int screenWidth, screenHeight;
BITMAPINFO bmi = {};

void CaptureScreen(HWND hwnd) {
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    if (!hGlitchBitmap) {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = screenWidth;
        bmi.bmiHeader.biHeight = -screenHeight; // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        hGlitchBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pPixels, NULL, 0);
    }
    
    SelectObject(hdcMem, hGlitchBitmap);
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

void ApplyGlitchEffect() {
    if (!pPixels) return;
    
    // Salin buffer asli untuk manipulasi
    BYTE* pCopy = new BYTE[screenWidth * screenHeight * 4];
    memcpy(pCopy, pPixels, screenWidth * screenHeight * 4);
    
    // Terapkan efek glitch
    for (int i = 0; i < GLITCH_LINES; ++i) {
        int y = rand() % screenHeight;
        int height = 1 + rand() % 30;
        int xOffset = (rand() % (MAX_GLITCH_INTENSITY * 2)) - MAX_GLITCH_INTENSITY;
        
        for (int h = 0; h < height && y + h < screenHeight; ++h) {
            int currentY = y + h;
            BYTE* source = pCopy + (currentY * screenWidth * 4);
            BYTE* dest = pPixels + (currentY * screenWidth * 4);
            
            if (xOffset > 0) {
                memmove(dest + xOffset * 4, source, (screenWidth - xOffset) * 4);
                memset(dest, 0, xOffset * 4); // Bagian kosong diisi hitam
            } 
            else if (xOffset < 0) {
                memmove(dest, source - xOffset * 4, (screenWidth + xOffset) * 4);
                memset(dest + (screenWidth + xOffset) * 4, 0, -xOffset * 4);
            }
        }
    }
    
    delete[] pCopy;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HDC hdcLayered = NULL;
    static BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    
    switch (msg) {
    case WM_CREATE:
        hdcLayered = CreateCompatibleDC(NULL);
        SetTimer(hwnd, 1, REFRESH_RATE, NULL);
        return 0;
        
    case WM_TIMER:
        CaptureScreen(hwnd);
        ApplyGlitchEffect();
        
        HDC hdcScreen = GetDC(NULL);
        POINT ptZero = { 0 };
        SIZE size = { screenWidth, screenHeight };
        
        SelectObject(hdcLayered, hGlitchBitmap);
        UpdateLayeredWindow(hwnd, hdcScreen, NULL, &size, hdcLayered, 
                           &ptZero, 0, &blend, ULW_ALPHA);
        
        ReleaseDC(NULL, hdcScreen);
        return 0;
        
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;
        
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        DeleteObject(hGlitchBitmap);
        DeleteDC(hdcLayered);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    srand(static_cast<unsigned>(time(NULL)));
    
    // Register window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc,
                      0, 0, hInst, NULL, NULL, NULL, NULL,
                      L"GlitchWindowClass", NULL };
    RegisterClassEx(&wc);
    
    // Create layered window
    HWND hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
        wc.lpszClassName, L"Desktop Glitch",
        WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), 
        GetSystemMetrics(SM_CYSCREEN), NULL, NULL, hInst, NULL);
    
    ShowWindow(hwnd, SW_SHOW);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}
