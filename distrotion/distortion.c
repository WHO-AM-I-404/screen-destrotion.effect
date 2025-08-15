
#include <windows.h>
#include <math.h>

#define PI 3.1415926535
#define ITERATIONS 150
#define ITERATIONSM 40
#define DISTANCE 4
#define SIZE 10

int main(void) {
    HDC desktopDC = GetDC(NULL);

    const int scrx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int scry = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int scrw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int scrh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC screenshotDC = CreateCompatibleDC(desktopDC);
    HBITMAP screenshotBmp = CreateCompatibleBitmap(desktopDC, scrw, scrh);
    SelectObject(screenshotDC, screenshotBmp);

    for (;;) {
        BitBlt(screenshotDC, scrx, scry, scrw, scrh, desktopDC, 0, 0, SRCCOPY);

        POINT curpos;
        GetCursorPos(&curpos);
        curpos.x += scrx;
        curpos.y += scry;

        for (float angle = 0; angle < 2*PI; angle += PI/ITERATIONS*2) {
            float normX = sinf(angle);
            float normY = cosf(angle);

            for (int d = 0; d < ITERATIONSM*DISTANCE; d+=DISTANCE) {
                float dfac = 1-d / ((float)(ITERATIONSM*DISTANCE));
                int srcX = normX * (d-1) + curpos.x;
                int srcY = normY * (d-1) + curpos.y;

                int targetX = normX * d + curpos.x;
                int targetY = normY * d + curpos.y;
                
                BitBlt(screenshotDC, targetX, targetY, SIZE, SIZE, screenshotDC, srcX, srcY, SRCCOPY);
            }
        }

        BitBlt(desktopDC, 0, 0, scrw, scrh, screenshotDC, scrx, scry, SRCCOPY);

        Sleep(50);
    }
}

