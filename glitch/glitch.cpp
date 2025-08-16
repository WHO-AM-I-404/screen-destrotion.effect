#include <Windows.h>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <mmsystem.h>
#include <thread>
#include <vector>

#pragma comment(lib, "winmm.lib")

const int REFRESH_RATE = 15; // Lebih cepat untuk efek lebih intens
const int MAX_GLITCH_INTENSITY = 500;
const int GLITCH_LINES = 200;
const int MAX_GLITCH_BLOCKS = 100;
const int SOUND_CHANCE = 3; // 1 dari 3 frame ada suara

HBITMAP hGlitchBitmap = NULL;
BYTE* pPixels = NULL;
int screenWidth, screenHeight;
BITMAPINFO bmi = {};
bool soundEnabled = true;

// Fungsi untuk memainkan suara glitch secara asinkron
void PlayGlitchSoundAsync() {
    std::thread([]() {
        int soundType = rand() % 6;
        switch (soundType) {
        case 0:
            PlaySound(TEXT("SystemHand"), NULL, SND_ALIAS | SND_ASYNC);
            break;
        case 1:
            PlaySound(TEXT("SystemExclamation"), NULL, SND_ALIAS | SND_ASYNC);
            break;
        case 2:
            Beep(rand() % 3000 + 500, rand() % 200 + 50);
            break;
        case 3:
            // Suara white noise digital
            for (int i = 0; i < 5; i++) {
                Beep(rand() % 4000 + 500, 20);
                Sleep(10);
            }
            break;
        case 4:
            // Suara frekuensi rendah
            Beep(rand() % 100 + 50, rand() % 300 + 100);
            break;
        case 5:
            // Kombinasi suara acak
            for (int i = 0; i < 3; i++) {
                Beep(rand() % 3000 + 500, rand() % 50 + 20);
                Sleep(15);
            }
            break;
        }
    }).detach();
}

void CaptureScreen(HWND hwnd) {
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    if (!hGlitchBitmap) {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = screenWidth;
        bmi.bmiHeader.biHeight = -screenHeight;
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

void ApplyColorShift(BYTE* pixels, int shift) {
    for (int i = 0; i < screenWidth * screenHeight * 4; i += 4) {
        BYTE temp = pixels[i];
        pixels[i] = pixels[i + shift];
        pixels[i + shift] = temp;
    }
}

void ApplyGlitchEffect() {
    if (!pPixels) return;
    
    BYTE* pCopy = new BYTE[screenWidth * screenHeight * 4];
    memcpy(pCopy, pPixels, screenWidth * screenHeight * 4);
    
    // Glitch garis horizontal intens
    for (int i = 0; i < GLITCH_LINES; ++i) {
        int y = rand() % screenHeight;
        int height = 1 + rand() % 100;
        int xOffset = (rand() % (MAX_GLITCH_INTENSITY * 2)) - MAX_GLITCH_INTENSITY;
        int rgbShift = rand() % 20 - 10;
        
        for (int h = 0; h < height && y + h < screenHeight; ++h) {
            int currentY = y + h;
            BYTE* source = pCopy + (currentY * screenWidth * 4);
            BYTE* dest = pPixels + (currentY * screenWidth * 4);
            
            if (xOffset > 0) {
                memmove(dest + xOffset * 4, source, (screenWidth - xOffset) * 4);
                for (int x = 0; x < xOffset; x++) {
                    int pos = (currentY * screenWidth + x) * 4;
                    if (pos < screenWidth * screenHeight * 4 - 4) {
                        pPixels[pos] = rand() % 256;
                        pPixels[pos + 1] = rand() % 256;
                        pPixels[pos + 2] = rand() % 256;
                    }
                }
            } 
            else if (xOffset < 0) {
                memmove(dest, source - xOffset * 4, (screenWidth + xOffset) * 4);
                for (int x = screenWidth + xOffset; x < screenWidth; x++) {
                    int pos = (currentY * screenWidth + x) * 4;
                    if (pos < screenWidth * screenHeight * 4 - 4) {
                        pPixels[pos] = rand() % 256;
                        pPixels[pos + 1] = rand() % 256;
                        pPixels[pos + 2] = rand() % 256;
                    }
                }
            }
        }
    }
    
    // Distorsi blok ekstrim
    for (int i = 0; i < MAX_GLITCH_BLOCKS; ++i) {
        int blockWidth = 100 + rand() % 300;
        int blockHeight = 100 + rand() % 300;
        int x = rand() % (screenWidth - blockWidth);
        int y = rand() % (screenHeight - blockHeight);
        int offsetX = (rand() % 300) - 150;
        int offsetY = (rand() % 300) - 150;
        
        for (int h = 0; h < blockHeight; h++) {
            int sourceY = y + h;
            int destY = sourceY + offsetY;
            
            if (destY >= 0 && destY < screenHeight && sourceY >= 0 && sourceY < screenHeight) {
                BYTE* source = pCopy + (sourceY * screenWidth + x) * 4;
                BYTE* dest = pPixels + (destY * screenWidth + x + offsetX) * 4;
                
                if (dest >= pPixels && dest + blockWidth*4 <= pPixels + screenWidth*screenHeight*4) {
                    memcpy(dest, source, blockWidth * 4);
                }
            }
        }
    }
    
    // Shift warna acak
    if (rand() % 4 == 0) {
        ApplyColorShift(pPixels, (rand() % 3) + 1);
    }
    
    // Efek noise digital
    for (int i = 0; i < screenWidth * screenHeight / 10; i++) {
        int x = rand() % screenWidth;
        int y = rand() % screenHeight;
        int pos = (y * screenWidth + x) * 4;
        
        if (pos < screenWidth * screenHeight * 4 - 4) {
            pPixels[pos] = rand() % 256;
            pPixels[pos + 1] = rand() % 256;
            pPixels[pos + 2] = rand() % 256;
        }
    }
    
    // Efek distorsi radial
    if (rand() % 5 == 0) {
        int centerX = rand() % screenWidth;
        int centerY = rand() % screenHeight;
        int radius = 100 + rand() % 500;
        int distortion = 10 + rand() % 50;
        
        for (int y = centerY - radius; y < centerY + radius; y++) {
            if (y < 0 || y >= screenHeight) continue;
            
            for (int x = centerX - radius; x < centerX + radius; x++) {
                if (x < 0 || x >= screenWidth) continue;
                
                float dx = x - centerX;
                float dy = y - centerY;
                float distance = sqrt(dx*dx + dy*dy);
                
                if (distance < radius) {
                    float amount = 1.0 - (distance / radius);
                    int shiftX = static_cast<int>(dx * amount * distortion);
                    int shiftY = static_cast<int>(dy * amount * distortion);
                    
                    int srcX = x - shiftX;
                    int srcY = y - shiftY;
                    
                    if (srcX >= 0 && srcX < screenWidth && srcY >= 0 && srcY < screenHeight) {
                        int srcPos = (srcY * screenWidth + srcX) * 4;
                        int destPos = (y * screenWidth + x) * 4;
                        
                        if (destPos < screenWidth * screenHeight * 4 - 4 && 
                            srcPos < screenWidth * screenHeight * 4 - 4) {
                            pPixels[destPos] = pCopy[srcPos];
                            pPixels[destPos + 1] = pCopy[srcPos + 1];
                            pPixels[destPos + 2] = pCopy[srcPos + 2];
                        }
                    }
                }
            }
        }
    }
    
    // Mainkan suara glitch secara acak
    if (soundEnabled && rand() % SOUND_CHANCE == 0) {
        PlayGlitchSoundAsync();
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
        
    case WM_TIMER: {
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
    }
        
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) 
            DestroyWindow(hwnd);
        else if (wParam == 'S') 
            soundEnabled = !soundEnabled;
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
    
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc,
                      0, 0, hInst, NULL, NULL, NULL, NULL,
                      "ExtremeGlitchClass", NULL };
    RegisterClassEx(&wc);
    
    HWND hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, "Desktop Apocalypse",
        WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), 
        GetSystemMetrics(SM_CYSCREEN), NULL, NULL, hInst, NULL);
    
    ShowWindow(hwnd, SW_SHOW);
    
    // Mainkan suara startup
    PlayGlitchSoundAsync();
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}
