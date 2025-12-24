// MusicProject.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "MusicProject.h"
#include <ShellScalingApi.h>
#include <CommCtrl.h>
#include <mmsystem.h>
#include <uxtheme.h>  // Для тем Windows
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>
#include <vector>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "Ws2_32.lib") // Link with Ws2_32.lib
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shcore.lib") // Додати цей pragma
#pragma comment(lib, "uxtheme.lib") 

using namespace std;

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define MAX_LOADSTRING 100

// Source - https://stackoverflow.com/a
// Posted by Brian R. Bondy, modified by community. See post 'Timeline' for change history
// Retrieved 2025-12-24, License - CC BY-SA 2.5

//Example: b1 == 192, b2 == 168, b3 == 0, b4 == 100
struct IPv4
{
    unsigned char b1, b2, b3, b4;
};
wstring UserIP;

void getMyIP() {
    WSADATA wsaData;

    // Ініціалізація Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return;
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        
        WSACleanup();
        return;
    }


    // Використання getaddrinfo замість gethostbyname
    struct addrinfo hints = { 0 };
    struct addrinfo* result = NULL;

    hints.ai_family = AF_UNSPEC;    // І IPv4, і IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    // Для отримання IP

    int ret = getaddrinfo(hostname, NULL, &hints, &result);
    if (ret != 0) {
        WSACleanup();
        return;
    }

    // Перебираємо всі результати
    std::vector<std::string> ipAddresses;

    for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        char ipStr[INET6_ADDRSTRLEN];
        void* addr;

        if (ptr->ai_family == AF_INET) { // IPv4
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)ptr->ai_addr;
            addr = &(ipv4->sin_addr);
        }
        else { // IPv6
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)ptr->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        // Конвертуємо в строку
        inet_ntop(ptr->ai_family, addr, ipStr, sizeof(ipStr));

        // Пропускаємо localhost
        if (strcmp(ipStr, "127.0.0.1") != 0 && strcmp(ipStr, "::1") != 0) {
            ipAddresses.push_back(ipStr);
        }
    }

    freeaddrinfo(result);

    // Вибір основної IP
    if (!ipAddresses.empty()) {
        // Вибір першої IPv4 адреси
        for (const auto& ip : ipAddresses) {
            if (ip.find(':') == std::string::npos) { // IPv4 (не містить ':')
                UserIP = std::wstring(ip.begin(), ip.end());
                break;
            }
        }

        // Якщо IPv4 не знайдено, беремо першу
        if (UserIP.empty()) {
            UserIP = std::wstring(ipAddresses[0].begin(), ipAddresses[0].end());
        }

        // Вивід всіх IP
        for (size_t i = 0; i < ipAddresses.size(); i++) {
            std::wstring wip(ipAddresses[i].begin(), ipAddresses[i].end());
        }
    }
    else {
        UserIP = L"IP не знайдено";
    }

    WSACleanup();
}

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void PlayMP3FromResource(int resourceID) {

    // 1. Знаходимо ресурс MP3
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceID), _T("MUSIC"));
    if (!hRes) {
        // Спробуємо знайти як "MP3" тип
        hRes = FindResource(NULL, MAKEINTRESOURCE(resourceID), _T("MP3"));
        if (!hRes) {
            return;
        }
    }

    // 2. Отримуємо розмір ресурсу
    DWORD resourceSize = SizeofResource(NULL, hRes);

    // 3. Завантажуємо ресурс
    HGLOBAL hGlobal = LoadResource(NULL, hRes);
    if (!hGlobal) {
        return;
    }

    // 4. Блокуємо ресурс для отримання вказівника
    LPVOID resourceData = LockResource(hGlobal);
    if (!resourceData) {
        return;
    }

    // 5. Створюємо тимчасовий файл
    TCHAR tempPath[MAX_PATH];
    TCHAR tempFile[MAX_PATH];

    if (!GetTempPath(MAX_PATH, tempPath)) {
        return;
    }

    if (!GetTempFileName(tempPath, _T("mp3"), 0, tempFile)) {
        return;
    }


    // 6. Записуємо ресурс у тимчасовий файл
    HANDLE hFile = CreateFile(tempFile, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD bytesWritten;
    if (!WriteFile(hFile, resourceData, resourceSize, &bytesWritten, NULL)) {
        CloseHandle(hFile);
        DeleteFile(tempFile);
        return;
    }

    CloseHandle(hFile);

    // 7. Відтворюємо MP3 через MCI
    TCHAR mciCommand[512];

    // Формуємо команду для відкриття файлу
    _stprintf_s(mciCommand, 512, _T("open \"%s\" type mpegvideo alias mymusic"), tempFile);

    MCIERROR mciError = mciSendString(mciCommand, NULL, 0, NULL);
    if (mciError != 0) {
        // Спробуємо інший спосіб
        _stprintf_s(mciCommand, 512, _T("open \"%s\" alias mymusic"), tempFile);
        mciError = mciSendString(mciCommand, NULL, 0, NULL);

        if (mciError != 0) {
            TCHAR errorMsg[256];
            mciGetErrorString(mciError, errorMsg, 256);
            DeleteFile(tempFile);
            return;
        }
    }


    // 8. Відтворюємо музику
    mciError = mciSendString(_T("play mymusic"), NULL, 0, NULL);
    if (mciError == 0) {
    }
    else {
        TCHAR errorMsg[256];
        mciGetErrorString(mciError, errorMsg, 256);
    }


}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    
    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MUSICPROJECT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MUSICPROJECT));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MUSICPROJECT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_MUSICPROJECT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE: {

        break;
    }
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
                
            // ОБЕРЕЖНО, МР3 МУЗИКА!!!

            case ID_XX1: {
                PlayMP3FromResource(IDR_MUSICREBELIAMYP);
                return (INT_PTR)TRUE;
            }
            case ID_Samduvav1: {
                PlayMP3FromResource(IDR_MUSICSAMDUVAVMYP);
                return (INT_PTR)TRUE;
            }
            case ID_Acute2: {
                PlayMP3FromResource(IDR_MUSICACUTEDZEN);
                return (INT_PTR)TRUE;
            }
            case ID_Trouble2: {
                PlayMP3FromResource(IDR_MUSICHALEPADZEN);
                return (INT_PTR)TRUE;
            }

                            //STEPLER *

            case ID_LOLITREK_HAZAII: {

                PlayMP3FromResource(IDR_MUSICHAZA2_8D);
                return (INT_PTR)TRUE;
            }
            case ID_LOLITREK_UdieNnx_HXVSAGE_DJ_PORTU: {

                PlayMP3FromResource(IDR_UdieNnx_HXVSAGE_DJ_PORTU);
                return (INT_PTR)TRUE;
            }

            case ID_LOLITREK_PAISONADO: {
                PlayMP3FromResource(IDR_MUSICPAISONADO3);
                return (INT_PTR)TRUE;
            }

            // ОБЕРЕЖНО, МР3 МУЗИКА!!!


            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
    {

        HBITMAP h67Bitmap = LoadBitmap(GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDC_MUSICPROJECT));

        HWND h67Button = GetDlgItem(hDlg, IDC_67BUTTON);

        if (h67Bitmap) {
            // Встановлюємо бітмап на кнопку
            SendMessage(h67Button, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)h67Bitmap);
        }

        /*
        HWND h67Btn = GetDlgItem(hDlg, IDC_67BUTTON);

        SetWindowLong(h67Btn, GWL_EXSTYLE,
            GetWindowLong(h67Btn, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
        SetLayeredWindowAttributes(h67Btn, 0, 0, LWA_ALPHA);
        */
        HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS);
        if (hProgressBar)
        {
            // Встановлюємо діапазон
            SendMessage(hProgressBar, PBM_SETRANGE32, 0, 100);

            // Початкова позиція
            SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

            // Крок для PBM_STEPIT
            SendMessage(hProgressBar, PBM_SETSTEP, 10, 0);

            // Стиль smooth (гладкий)
            DWORD style = GetWindowLong(hProgressBar, GWL_STYLE);
            SetWindowLong(hProgressBar, GWL_STYLE, style | PBS_SMOOTH);

            // Для вертикального прогрес-бару (опціонально)
            // SetWindowLong(hProgressBar, GWL_STYLE, style | PBS_VERTICAL);
        }

    }
    return (INT_PTR)TRUE;
    case WM_COMMAND:

        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDOK: {

            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        //@ts-ignore
        case IDC_DELETEBRAIN: {
            system("start https://youtu.be/dQw4w9WgXcQ?si=gVtwyQRlzOrrdkmb");

            return (INT_PTR)TRUE;

        }
        //case IDC_67BUTTON: {
        //    return (INT_PTR)TRUE;
        //}
        case IDC_IPREADER: {
            DWORD dwIPAddress = 0;
            HWND IpAdressControll = GetDlgItem(hDlg, IDC_IPADDRESS);

            SendMessage(IpAdressControll, IPM_GETADDRESS, 0, (LPARAM)&dwIPAddress);

            // Розпакувати октети
            BYTE b1 = FIRST_IPADDRESS(dwIPAddress);
            BYTE b2 = SECOND_IPADDRESS(dwIPAddress);
            BYTE b3 = THIRD_IPADDRESS(dwIPAddress);
            BYTE b4 = FOURTH_IPADDRESS(dwIPAddress);

            wstring IpAdressEnteredByUsr;

            IpAdressEnteredByUsr = std::to_wstring(b1) + L"." +
                std::to_wstring(b2) + L"." +
                std::to_wstring(b3) + L"." +
                std::to_wstring(b4);

            getMyIP();

            wstring FuckingMessage = L"Той ІР адресс який ти ввів: " + IpAdressEnteredByUsr + L" А той який ти мав ввести: ", to_wstring(UserIP);

            MessageBoxW(hDlg, FuckingMessage.c_str(), L"Твій ІР адресс", MB_YESNOCANCEL);
            PlayMP3FromResource(IDR_MUSIC6);
            return (INT_PTR)TRUE;
        }
        case IDC_PLEASENO: {

            MessageBoxW(NULL, L"Тебя нихто не спрашывал", L"67", MB_OK);
            return (INT_PTR)TRUE;
        }
        }
        
    }
    return (INT_PTR)FALSE;
}
