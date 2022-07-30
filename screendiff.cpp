// Date: Dec 8, 2021
// screendiff.cpp : Coding Exercise as per assignment
// Start time: 17:30
// End time: 18:15

#include <iostream>
#include <thread>
#include <windows.h>
using namespace std;


// Store screen resolution
struct ScreenRes {
    int x0;
    int x1;
    int y0;
    int y1;
    int width;
    int height;
    void adjustWidth() {
        width = x1 - x0;
    }
    void adjustHeight() {
        height = y1 - y0;
    }
};

//Get current [+virtual span] screen resolution
void GetScreenRes(ScreenRes& sr) {
    sr.x0 = GetSystemMetrics(SM_XVIRTUALSCREEN);
    sr.y0 = GetSystemMetrics(SM_YVIRTUALSCREEN);
    sr.x1 = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    sr.y1 = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    sr.adjustWidth(); //calculate factual screen width [+span across multiple srcreens]
    sr.adjustHeight(); //calculate factual screen height [+span accros multiple screens]
}

//Capture screen [+multiscreen span]
//Note: returned HBITMPA must be freed to prevent memory leaks
HBITMAP GetScreenShot(const ScreenRes& sr) {
    HDC hScreenDeviceContext = CreateDC(L"DISPLAY", NULL, NULL, NULL);
    HDC     hScreen = GetDC(NULL);
    HDC     hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, sr.width, sr.height);
    HGDIOBJ GDIObj = SelectObject(hDC, hBitmap); 
    BOOL    bRet = BitBlt(hDC, 0, 0, sr.width, sr.height, hScreen, sr.x0, sr.y0, SRCCOPY);
    hBitmap = (HBITMAP)SelectObject(hDC, hBitmap);
    SelectObject(hDC, GDIObj);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);
    return hBitmap;
}

//Quickly compare the two bitmap objects
//Returns: a new bitmap if the two are different
HBITMAP CompareBitmapObjects(const ScreenRes& sr, HBITMAP hBitmapOld, HBITMAP hBitmapNew) {
    HBITMAP HBitmapDiff = NULL;
    // Quick sanity check
    if (hBitmapOld == hBitmapNew || (hBitmapOld == NULL || hBitmapNew == NULL)) {
        return NULL;
    }

    bool bNoDiff = false;

    HDC hdc = GetDC(NULL);
    BITMAPINFO bmInfoOld = { 0 };
    BITMAPINFO bmInfoNew = { 0 };

    bmInfoOld.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmInfoNew.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    if (0 != GetDIBits(hdc, hBitmapOld, 0, 0, NULL, &bmInfoOld, DIB_RGB_COLORS) &&
        0 != GetDIBits(hdc, hBitmapNew, 0, 0, NULL, &bmInfoNew, DIB_RGB_COLORS)) {
        if (0 == memcmp(&bmInfoOld.bmiHeader, &bmInfoNew.bmiHeader, sizeof(BITMAPINFOHEADER))) {
            BYTE* pOldBmBits = new BYTE[bmInfoOld.bmiHeader.biSizeImage];
            BYTE* pNewBmBits = new BYTE[bmInfoOld.bmiHeader.biSizeImage];
            BYTE* pByteOld = NULL;
            BYTE* pByteNew = NULL;

            PBITMAPINFO pBmInfoOld = &bmInfoOld;
            PBITMAPINFO pBmInfoNew = &bmInfoNew;
            int ResourceAlloc = 0;
            switch (bmInfoOld.bmiHeader.biBitCount) {
            case 1:
                ResourceAlloc = 1 * sizeof(RGBQUAD);
                break;
            case 4:
                ResourceAlloc = 15 * sizeof(RGBQUAD);
                break;
            case 8:
                ResourceAlloc = 255 * sizeof(RGBQUAD);
                break;
            case 16:
            case 32:
                ResourceAlloc = 2 * sizeof(RGBQUAD);
            }

            if (ResourceAlloc) {
                pByteOld = new BYTE[sizeof(BITMAPINFO) + ResourceAlloc];
                if (pByteOld) {
                    memset(pByteOld, 0, sizeof(BITMAPINFO) + ResourceAlloc);
                    memcpy(pByteOld, pBmInfoOld, sizeof(BITMAPINFO));
                    pBmInfoOld = (PBITMAPINFO)pByteOld;
                }

                pByteNew = new BYTE[sizeof(BITMAPINFO) + ResourceAlloc];
                if (pByteNew) {
                    memset(pByteNew, 0, sizeof(BITMAPINFO) + ResourceAlloc);
                    memcpy(pByteNew, pBmInfoNew, sizeof(BITMAPINFO));
                    pBmInfoNew = (PBITMAPINFO)pByteNew;
                }
            }

            if (
                pOldBmBits && 
                pNewBmBits && 
                pBmInfoOld && 
                pBmInfoNew) {

                memset(pNewBmBits, 0, bmInfoNew.bmiHeader.biSizeImage);
                memset(pOldBmBits, 0, bmInfoOld.bmiHeader.biSizeImage);

                if (0 != GetDIBits(hdc, hBitmapOld, 0,
                    pBmInfoOld->bmiHeader.biHeight, pOldBmBits, pBmInfoOld,
                    DIB_RGB_COLORS) && 0 != GetDIBits(hdc, hBitmapNew, 0,
                        pBmInfoNew->bmiHeader.biHeight, pNewBmBits, pBmInfoNew,
                        DIB_RGB_COLORS))
                {
                    // Let's compare the two bitmaps
                    bNoDiff = 0 == memcmp(pOldBmBits, pNewBmBits,
                        pBmInfoOld->bmiHeader.biSizeImage);
                    if (!bNoDiff) {
                        //Ok, the bitmaps are different, let's create a copy of the first image
                        HBitmapDiff = (HBITMAP)CopyImage(hBitmapOld, IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
                        //Identify which bits are different and set them to green
                        //Compare the diff bitmap with the original map 
                        BYTE* pDiffBits = new BYTE[bmInfoOld.bmiHeader.biSizeImage];
                        memset(pDiffBits, 0, bmInfoOld.bmiHeader.biSizeImage);
                        // Do channel by channel byte comparison to spot any differences
                        for (int idx = 0; idx < sr.width * sr.height * 4; idx += 4)
                        {
                            // If a difference in any of the channels is found - mark this screen pixel green
                            if (pOldBmBits[idx] != pNewBmBits[idx] ||
                                pOldBmBits[idx + 1] != pNewBmBits[idx + 1] ||
                                pOldBmBits[idx + 2] != pNewBmBits[idx + 2] ||
                                pOldBmBits[idx + 3] != pNewBmBits[idx + 3]) {
                                pDiffBits[idx] = 0;
                                pDiffBits[idx + 1] = 255; //<= Set to Green
                                pDiffBits[idx + 2] = 0;
                                pDiffBits[idx + 3] = 0;
                            }
                            else {
                                //Copy pixel as per original 
                                pDiffBits[idx] = pOldBmBits[idx];
                                pDiffBits[idx + 1] = pOldBmBits[idx + 1];
                                pDiffBits[idx + 2] = pOldBmBits[idx + 2];
                                pDiffBits[idx + 3] = pOldBmBits[idx + 3];
                            }
                        }
                        // Set the modified bits back and return the copy
                        SetDIBits(hdc, HBitmapDiff, 0,
                            pBmInfoOld->bmiHeader.biHeight, pDiffBits, pBmInfoOld,
                            DIB_RGB_COLORS);
                    }                   
                }
            }

            delete[] pOldBmBits;
            delete[] pNewBmBits;
            delete[] pByteOld;
            delete[] pByteNew;
        }
    }
    ReleaseDC(NULL, hdc);
    // return new image back
    return HBitmapDiff;
}

//Save bitmap to clipboard
void ClipboardBitmapDiff(HBITMAP hbm) {
    // save bitmap to clipboard
    OpenClipboard(NULL);
    EmptyClipboard();
    SetClipboardData(CF_BITMAP, hbm);
    CloseClipboard();
}

int main(int argc, char* argv[], char* env[])
{
    ScreenRes sr;
    GetScreenRes(sr);
    //Loop through until a difference is found, generate a new bitmap and store in the clipboard
    while (true) {
        //Screen stapshot 1
        HBITMAP hbm1 = GetScreenShot(sr);
        this_thread::sleep_for(10s);
        //Screen snapshot 2
        HBITMAP hbm2 = GetScreenShot(sr);
        //Compare the snapshots
        HBITMAP hbm3 = CompareBitmapObjects(sr, hbm1, hbm2);
        DeleteObject(hbm1);
        DeleteObject(hbm2);
        //If difference is spotted, a new bitmap is returned with the changes highlighted
        if (hbm3) {
            ClipboardBitmapDiff(hbm3);
            DeleteObject(hbm3);
            break;
        }
        this_thread::sleep_for(10s);
    }
    return EXIT_SUCCESS;
}
