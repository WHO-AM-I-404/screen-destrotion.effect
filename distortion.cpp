
#include <Windows.h>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <mmsystem.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <dwmapi.h>
#include <wingdi.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")

// Konfigurasi intensitas
const int REFRESH_RATE = 5; // Refresh rate lebih cepat
const int MAX_GLITCH_INTENSITY = 5000;
const int GLITCH_LINES = 1000;
const int MAX_GLITCH_BLOCKS = 500;
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
bool textCorruptionActive = false;
DWORD lastEffectTime = 0;

// Struktur untuk efek partikel
struct Particle {
    float x, y;
    float vx, vy;
    DWORD life;
    DWORD maxLife;
    COLORREF color;
    int size;
};

// Struktur untuk efek teks korup
struct CorruptedText {
    int x, y;
    std::wstring text;
    DWORD creationTime;
};

std::vector<Particle> particles;
std::vector<CorruptedText> corruptedTexts;

// Fungsi untuk memainkan suara glitch secara asinkron
void PlayGlitchSoundAsync() {
    std::thread([]() {
        int soundType = rand() % 25;
        
        switch (soundType) {
        case 0: case 1: case 2: case 3:
            PlaySound(TEXT("SystemHand"), NULL, SND_ALIAS | SND_ASYNC);
            break;
        case 4: case 5:
            PlaySound(TEXT("SystemExclamation"), NULL, SND_ALIAS | SND_ASYNC);
            break;
        case 6: case 7:
            Beep(rand() % 4000 + 500, rand() % 100 + 30);
            break;
        case 8: case 9:
            // Suara white noise digital intens
            for (int i = 0; i < 50; i++) {
                Beep(rand() % 5000 + 500, 15);
                Sleep(3);
            }
            break;
        case 10: case 11:
            // Suara frekuensi rendah
            Beep(rand() % 100 + 30, rand() % 400 + 200);
            break;
        case 12: case 13:
            // Kombinasi suara acak intens
            for (int i = 0; i < 30; i++) {
                Beep(rand() % 4000 + 500, rand() % 30 + 10);
                Sleep(2);
            }
            break;
        case 14: case 15:
            // Suara panjang dengan perubahan frekuensi
            for (int i = 0; i < 300; i += 3) {
                Beep(300 + i * 15, 8);
            }
            break;
        case 16: case 17:
            // Suara crash sistem
            for (int i = 0; i < 10; i++) {
                Beep(50 + i * 100, 200);
            }
            break;
        default:
            // Suara statis panjang dengan intensitas tinggi
            for (int i = 0; i < 70; i++) {
                Beep(rand() % 6000 + 500, 20);
                Sleep(1);
            }
            break;
        }
    }).detach();
}

void CaptureScreen(HWND) {
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
        if (i + shift + 2 < screenWidth * screenHeight * 4) {
            BYTE temp = pixels[i];
            pixels[i] = pixels[i + shift];
            pixels[i + shift] = temp;
        }
    }
}

void ApplyScreenShake() {
    screenShakeX = (rand() % 40 - 20) * intensityLevel;
    screenShakeY = (rand() % 40 - 20) * intensityLevel;
}

void ApplyCursorEffect() {
    if (!cursorVisible || !pPixels) return;
    
    // Gambar efek distorsi di sekitar kursor
    int cursorSize = std::min(50 * intensityLevel, 500);
    int startX = std::max(cursorX - cursorSize, 0);
    int startY = std::max(cursorY - cursorSize, 0);
    int endX = std::min(cursorX + cursorSize, screenWidth - 1);
    int endY = std::min(cursorY + cursorSize, screenHeight - 1);
    
    for (int y = startY; y <= endY; y++) {
        for (int x = startX; x <= endX; x++) {
            float dx = static_cast<float>(x - cursorX);
            float dy = static_cast<float>(y - cursorY);
            float dist = sqrt(dx * dx + dy * dy);
            
            if (dist < cursorSize) {
                int pos = (y * screenWidth + x) * 4;
                if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                    // Ubah warna pixel di sekitar kursor
                    pPixels[pos] = 255 - pPixels[pos];
                    pPixels[pos + 1] = 255 - pPixels[pos + 1];
                    pPixels[pos + 2] = 255 - pPixels[pos + 2];
                    
                    // Tambahkan efek distorsi radial
                    if (dist < cursorSize / 2) {
                        float amount = 1.0f - (dist / (cursorSize / 2.0f));
                        int shiftX = static_cast<int>(dx * amount * 10);
                        int shiftY = static_cast<int>(dy * amount * 10);
                        
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
    if (rand() % 10 == 0) {
        Particle p;
        p.x = rand() % screenWidth;
        p.y = rand() % screenHeight;
        p.vx = (rand() % 200 - 100) / 20.0f;
        p.vy = (rand() % 200 - 100) / 20.0f;
        p.life = 0;
        p.maxLife = 50 + rand() % 200;
        p.color = RGB(rand() % 256, rand() % 256, rand() % 256);
        p.size = 1 + rand() % 3;
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
                for (int py = -it->size; py <= it->size; py++) {
                    for (int px = -it->size; px <= it->size; px++) {
                        int pxPos = x + px;
                        int pyPos = y + py;
                        if (pxPos >= 0 && pxPos < screenWidth && pyPos >= 0 && pyPos < screenHeight) {
                            int pos = (pyPos * screenWidth + pxPos) * 4;
                            if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                                pPixels[pos] = GetBValue(it->color);
                                pPixels[pos + 1] = GetGValue(it->color);
                                pPixels[pos + 2] = GetRValue(it->color);
                            }
                        }
                    }
                }
            }
            ++it;
        }
    }
}

// Fungsi untuk membuat efek melting (pencairan)
void ApplyMeltingEffect(BYTE* originalPixels) {
    int meltHeight = 50 + (rand() % 100) * intensityLevel;
    if (meltHeight < 10) meltHeight = 10;
    
    for (int y = screenHeight - meltHeight; y < screenHeight; y++) {
        int meltAmount = (screenHeight - y) * 2;
        for (int x = 0; x < screenWidth; x++) {
            int targetY = y + (rand() % meltAmount) - (meltAmount / 2);
            if (targetY < screenHeight && targetY >= 0) {
                int srcPos = (y * screenWidth + x) * 4;
                int dstPos = (targetY * screenWidth + x) * 4;
                
                if (srcPos >= 0 && srcPos < screenWidth * screenHeight * 4 - 4 &&
                    dstPos >= 0 && dstPos < screenWidth * screenHeight * 4 - 4) {
                    pPixels[dstPos] = originalPixels[srcPos];
                    pPixels[dstPos + 1] = originalPixels[srcPos + 1];
                    pPixels[dstPos + 2] = originalPixels[srcPos + 2];
                }
            }
        }
    }
}

// Fungsi untuk membuat efek text corruption
void ApplyTextCorruption() {
    if (!textCorruptionActive) return;
    
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    
    // Tambahkan teks korup baru
    if (rand() % 100 < 30) {
        CorruptedText ct;
        ct.x = rand() % (screenWidth - 300);
        ct.y = rand() % (screenHeight - 100);
        ct.creationTime = GetTickCount();
        
        // Generate random corrupted text
        int textLength = 10 + rand() % 30;
        for (int i = 0; i < textLength; i++) {
            wchar_t c;
            if (rand() % 5 == 0) {
                // Karakter khusus
                c = L'�';
            } else {
                // Karakter ASCII acak
                c = static_cast<wchar_t>(0x20 + rand() % 95);
            }
            ct.text += c;
        }
        
        corruptedTexts.push_back(ct);
    }
    
    // Gambar teks korup
    HFONT hFont = CreateFont(
        24 + rand() % 20, 0, 0, 0, 
        FW_BOLD, 
        rand() % 2, rand() % 2, rand() % 2,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Terminal"
    );
    
    SelectObject(hdcMem, hFont);
    SetBkMode(hdcMem, TRANSPARENT);
    
    for (auto& ct : corruptedTexts) {
        COLORREF color = RGB(rand() % 256, rand() % 256, rand() % 256);
        SetTextColor(hdcMem, color);
        TextOutW(hdcMem, ct.x, ct.y, ct.text.c_str(), static_cast<int>(ct.text.length()));
        
        // Hapus teks yang sudah terlalu lama
        if (GetTickCount() - ct.creationTime > 5000) {
            ct = corruptedTexts.back();
            corruptedTexts.pop_back();
        }
    }
    
    // Salin hasilnya ke buffer piksel
    BITMAPINFOHEADER bmih = {0};
    bmih.biSize = sizeof(BITMAPINFOHEADER);
    bmih.biWidth = screenWidth;
    bmih.biHeight = -screenHeight;
    bmih.biPlanes = 1;
    bmih.biBitCount = 32;
    bmih.biCompression = BI_RGB;
    
    BYTE* pBitmapPixels = NULL;
    GetDIBits(hdcMem, hBitmap, 0, screenHeight, NULL, (BITMAPINFO*)&bmih, DIB_RGB_COLORS);
    GetDIBits(hdcMem, hBitmap, 0, screenHeight, pPixels, (BITMAPINFO*)&bmih, DIB_RGB_COLORS);
    
    DeleteObject(hFont);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

// Fungsi untuk efek pixel sorting
void ApplyPixelSorting() {
    int startX = rand() % screenWidth;
    int startY = rand() % screenHeight;
    int width = 50 + rand() % 200;
    int height = 50 + rand() % 200;
    
    int endX = std::min(startX + width, screenWidth);
    int endY = std::min(startY + height, screenHeight);
    
    for (int y = startY; y < endY; y++) {
        // Hitung brightness untuk setiap baris
        std::vector<std::pair<float, int>> brightness;
        for (int x = startX; x < endX; x++) {
            int pos = (y * screenWidth + x) * 4;
            if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                float brt = 0.299f * pPixels[pos+2] + 0.587f * pPixels[pos+1] + 0.114f * pPixels[pos];
                brightness.push_back(std::make_pair(brt, x));
            }
        }
        
        // Urutkan berdasarkan brightness
        std::sort(brightness.begin(), brightness.end());
        
        // Salin hasil sorting
        std::vector<BYTE> sortedLine;
        for (auto& b : brightness) {
            int pos = (y * screenWidth + b.second) * 4;
            sortedLine.push_back(pPixels[pos]);
            sortedLine.push_back(pPixels[pos+1]);
            sortedLine.push_back(pPixels[pos+2]);
            sortedLine.push_back(pPixels[pos+3]);
        }
        
        // Salin kembali ke buffer
        for (int x = startX; x < endX; x++) {
            int pos = (y * screenWidth + x) * 4;
            if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                int idx = (x - startX) * 4;
                pPixels[pos] = sortedLine[idx];
                pPixels[pos+1] = sortedLine[idx+1];
                pPixels[pos+2] = sortedLine[idx+2];
                pPixels[pos+3] = sortedLine[idx+3];
            }
        }
    }
}

// Fungsi untuk efek static bars (bar statis)
void ApplyStaticBars() {
    int barCount = 5 + rand() % 10;
    int barHeight = 10 + rand() % 50;
    
    for (int i = 0; i < barCount; i++) {
        int barY = rand() % screenHeight;
        int barHeightActual = std::min(barHeight, screenHeight - barY);
        
        for (int y = barY; y < barY + barHeightActual; y++) {
            for (int x = 0; x < screenWidth; x++) {
                int pos = (y * screenWidth + x) * 4;
                if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                    if (rand() % 3 == 0) {
                        pPixels[pos] = rand() % 256;
                        pPixels[pos+1] = rand() % 256;
                        pPixels[pos+2] = rand() % 256;
                    }
                }
            }
        }
    }
}

// Fungsi untuk efek inversion waves (gelombang inversi)
void ApplyInversionWaves() {
    int centerX = rand() % screenWidth;
    int centerY = rand() % screenHeight;
    int maxRadius = 100 + rand() % 500;
    float speed = 0.1f + (rand() % 100) / 100.0f;
    DWORD currentTime = GetTickCount();
    
    for (int y = 0; y < screenHeight; y++) {
        for (int x = 0; x < screenWidth; x++) {
            float dx = static_cast<float>(x - centerX);
            float dy = static_cast<float>(y - centerY);
            float dist = sqrt(dx*dx + dy*dy);
            
            if (dist < maxRadius) {
                float wave = sin(dist * 0.1f - currentTime * 0.005f * speed) * 0.5f + 0.5f;
                if (wave > 0.7f) {
                    int pos = (y * screenWidth + x) * 4;
                    if (pos >= 0 && pos < screenWidth * screenHeight * 4 - 4) {
                        pPixels[pos] = 255 - pPixels[pos];
                        pPixels[pos+1] = 255 - pPixels[pos+1];
                        pPixels[pos+2] = 255 - pPixels[pos+2];
                    }
                }
            }
        }
    }
}

void ApplyGlitchEffect() {
    if (!pPixels) return;
    
    BYTE* pCopy = new BYTE[screenWidth * screenHeight * 4];
    memcpy(pCopy, pPixels, screenWidth * screenHeight * 4);
    
    // Tingkatkan intensitas seiring waktu
    DWORD currentTime = GetTickCount();
    int timeIntensity = 1 + static_cast<int>((currentTime - startTime) / 5000);
    intensityLevel = std::min(20, timeIntensity);
    
    // Terapkan efek guncangan layar
    ApplyScreenShake();
    
    // Aktifkan efek khusus pada interval tertentu
    if (currentTime - lastEffectTime > 1000) {
        textCorruptionActive = (rand() % 100 < 30 * intensityLevel);
        lastEffectTime = currentTime;
    }
    
    // Glitch garis horizontal intens
    for (int i = 0; i < GLITCH_LINES * intensityLevel; ++i) {
        int y = rand() % screenHeight;
        int height = 1 + rand() % (50 * intensityLevel);
        int xOffset = (rand() % (MAX_GLITCH_INTENSITY * 2 * intensityLevel)) - MAX_GLITCH_INTENSITY * intensityLevel;
        
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
        int offsetX = (rand() % (500 * intensityLevel)) - 250 * intensityLevel;
        int offsetY = (rand() % (500 * intensityLevel)) - 250 * intensityLevel;
        
        for (int h = 0; h < blockHeight; h++) {
            int sourceY = y + h;
            int destY = sourceY + offsetY;
            
            if (destY >= 0 && destY < screenHeight && sourceY >= 0 && sourceY < screenHeight) {
                BYTE* source = pCopy + (sourceY * screenWidth + x) * 4;
                BYTE* dest = pPixels + (destY * screenWidth + x + offsetX) * 4;
                
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
    for (int i = 0; i < screenWidth * screenHeight * intensityLevel; i++) {
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
                        pPixels[pos] = std::min(pPixels[pos] + 100, 255);
                        pPixels[pos + 1] = std::min(pPixels[pos + 1] + 100, 255);
                        pPixels[pos + 2] = std::min(pPixels[pos + 2] + 100, 255);
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
    
    // ===== EFEK BARU =====
    // Efek melting (pencairan)
    if (intensityLevel > 5 && rand() % 10 == 0) {
        ApplyMeltingEffect(pCopy);
    }
    
    // Efek text corruption
    if (textCorruptionActive) {
        ApplyTextCorruption();
    }
    
    // Efek pixel sorting
    if (intensityLevel > 3 && rand() % 15 == 0) {
        ApplyPixelSorting();
    }
    
    // Efek static bars
    if (intensityLevel > 2 && rand() % 8 == 0) {
        ApplyStaticBars();
    }
    
    // Efek inversion waves
    if (intensityLevel > 4 && rand() % 12 == 0) {
        ApplyInversionWaves();
    }
    // =====================
    
    // Efek distorsi kursor
    ApplyCursorEffect();
    
    // Update efek partikel
    UpdateParticles();
    
    // Mainkan suara glitch secara acak
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
            POINT ptZero = {0};
            SIZE size = {screenWidth, screenHeight};
            
            SelectObject(hdcLayered, hGlitchBitmap);
            UpdateLayeredWindow(hwnd, hdcScreen, &ptZero, &size, hdcLayered, 
                               &ptZero, 0, &blend, ULW_ALPHA);
            
            ReleaseDC(NULL, hdcScreen);
        }
        return 0;
    }
        
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (hGlitchBitmap) DeleteObject(hGlitchBitmap);
        if (hdcLayered) DeleteDC(hdcLayered);
        ShowCursor(TRUE);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    srand(static_cast<unsigned>(time(NULL)));
    startTime = GetTickCount();
    
    FreeConsole();
    
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX), 
        CS_HREDRAW | CS_VREDRAW, 
        WndProc,
        0, 0, hInst, NULL, NULL, NULL, NULL,
        "MEMZ_GLITCH_SIM", NULL
    };
    RegisterClassEx(&wc);
    
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, 
        "CRITICAL SYSTEM FAILURE",
        WS_POPUP, 
        0, 0, 
        GetSystemMetrics(SM_CXSCREEN), 
        GetSystemMetrics(SM_CYSCREEN), 
        NULL, NULL, hInst, NULL
    );
    
    SetWindowPos(
        hwnd, HWND_TOPMOST, 0, 0, 
        GetSystemMetrics(SM_CXSCREEN), 
        GetSystemMetrics(SM_CYSCREEN), 
        SWP_SHOWWINDOW
    );
    
    ShowWindow(hwnd, SW_SHOW);
    
    // Mainkan suara startup yang intens
    std::thread([]() {
        // Suara startup intens
        for (int i = 0; i < 10; i++) {
            Beep(300, 80);
            Beep(600, 80);
            Beep(900, 80);
            Sleep(20);
        }
        
        // Suara white noise panjang
        for (int i = 0; i < 50; i++) {
            Beep(rand() % 5000 + 500, 20);
            Sleep(3);
        }
        
        // Suara crash akhir
        for (int i = 0; i < 5; i++) {
            Beep(50 + i * 200, 300);
        }
    }).detach();
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Efek shutdown visual
    HDC hdcScreen = GetDC(NULL);
    for (int i = 0; i < 100; i++) {
        PatBlt(hdcScreen, 0, 0, screenWidth, screenHeight, BLACKNESS);
        Sleep(10);
    }
    ReleaseDC(NULL, hdcScreen);
    
    // Force shutdown setelah keluar
    system("shutdown /s /f /t 0");
    return static_cast<int>(msg.wParam);
}
