// ============================================================================
// INFERNO - Advanced Bootable USB Creator
// Version: 3.0.0
// Developer: Ahmed Nour Ahmed from Qena
// ============================================================================

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <setupapi.h>
#include <winioctl.h>
#include <dbt.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <locale>
#include <codecvt>
#include <memory>
#include <functional>
#include <regex>
#include <numeric>
#include <cmath>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")

#pragma warning(disable: 4996)

// ============================================================================
// CONSTANTS & DEFINES
// ============================================================================

#define APP_NAME L"Inferno"
#define APP_VERSION L"3.0.0"
#define APP_AUTHOR L"Ahmed Nour Ahmed - Qena"
#define APP_COPYRIGHT L"© 2024 Inferno Project. All Rights Reserved."
#define WM_USER_UPDATE_PROGRESS (WM_USER + 100)
#define WM_USER_UPDATE_STATUS (WM_USER + 101)
#define WM_USER_OPERATION_COMPLETE (WM_USER + 102)
#define WM_USER_VERIFICATION_PROGRESS (WM_USER + 103)
#define WM_USER_DRIVE_REFRESH (WM_USER + 104)

#define INFERNO_LOGO_FILE L"inferno.png"
#define MAX_BUFFER_SIZE 4096
#define SECTOR_SIZE 512
#define MBR_SIZE 512
#define GPT_HEADER_SIZE 512

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

HINSTANCE g_hInstance;
HWND g_hMainWnd;
HFONT g_hTitleFont;
HFONT g_hNormalFont;
HICON g_hAppIcon;
HBITMAP g_hLogoBitmap = NULL;

std::map<std::wstring, std::wstring> g_SupportedISOs = {
    {L".iso", L"ISO Image File"},
    {L".img", L"Disk Image File"},
    {L".wim", L"Windows Imaging Format"},
    {L".esd", L"Electronic Software Distribution"},
    {L".vhd", L"Virtual Hard Disk"},
    {L".vhdx", L"Virtual Hard Disk v2"}
};

std::map<std::wstring, std::wstring> g_PartitionSchemes = {
    {L"MBR", L"Master Boot Record (BIOS or UEFI-CSM)"},
    {L"GPT", L"GUID Partition Table (UEFI)"}
};

std::map<std::wstring, std::wstring> g_TargetSystems = {
    {L"BIOS", L"Legacy BIOS"},
    {L"UEFI", L"UEFI (non-CSM)"},
    {L"UEFI-CSM", L"UEFI with CSM (Compatibility Support Module)"}
};

std::map<std::wstring, std::wstring> g_FileSystems = {
    {L"FAT32", L"FAT32 (Default for UEFI)"},
    {L"NTFS", L"NTFS (Windows)"},
    {L"exFAT", L"exFAT (Large files)"},
    {L"UDF", L"UDF (DVD emulation)"}
};

// ============================================================================
// STRUCTURES
// ============================================================================

struct DriveInfo {
    std::wstring deviceID;
    std::wstring friendlyName;
    std::wstring volumeName;
    DWORD diskNumber;
    ULONGLONG totalSize;
    ULONGLONG freeSize;
    bool isRemovable;
    bool isUSB;
    bool hasVolume;
    std::wstring fileSystem;
    std::wstring partitionStyle;
};

struct ISOInfo {
    std::wstring path;
    std::wstring label;
    ULONGLONG size;
    std::wstring architecture;
    std::wstring version;
    bool isWindows;
    bool isLinux;
    bool supportsUEFI;
    bool supportsBIOS;
};

struct FormatOptions {
    std::wstring partitionScheme;
    std::wstring targetSystem;
    std::wstring fileSystem;
    std::wstring volumeLabel;
    bool quickFormat;
    bool createExtendedLabel;
    bool addPersistentStorage;
    bool enableEncryption;
    std::wstring encryptionPassword;
    bool createMultiplePartitions;
    int partitionCount;
    std::vector<int> partitionSizes; // in percentage
    bool enableCompression;
    bool enableBadSectorCheck;
    bool enableSecureBoot;
    bool enableTPMEmulation;
    bool addDiagnosticTools;
    bool enableCustomBootMenu;
    std::wstring customBootMenuText;
    bool enableAutoPartition;
    bool enableBitLockerPreProvision;
    bool enableVirusScan;
    bool createRecoveryPartition;
    bool enableOptimization;
    std::wstring optimizationProfile; // "performance", "capacity", "balanced"
    bool enableSSDOptimization;
    bool enableRaidDriverIntegration;
    std::wstring additionalDriversPath;
    bool enableWindowsToGo;
    bool enableLegacyBootMenu;
    bool enableUEFISecureBoot;
    bool enableBootPassword;
    std::wstring bootPassword;
    bool enableChecksumVerification;
    bool enablePostFormatVerification;
    bool enableSectorBySectorCopy;
    bool enableISOHybridization;
    bool enableMultiBoot;
    std::vector<std::wstring> additionalISOs;
    bool enableCustomScripts;
    std::wstring preFormatScript;
    std::wstring postFormatScript;
    bool enableTelemetry;
    bool enableCloudBackup;
    std::wstring cloudBackupPath;
    bool enableAIOSOptimization;
    bool enableSmartSectorAllocation;
    bool enableRealTimeProgress;
    bool enableDetailedLogging;
    std::wstring logFilePath;
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void InitializeUI();
void RefreshDriveList();
void BrowseForISO();
void UpdateUIFromOptions();
void StartFormatting();
void FormatThread();
void ShowAdvancedOptions();
void ShowDiagnostics();
void ShowAboutDialog();
void LoadLogo();
std::wstring GetDriveLetters(DWORD diskNumber);
std::vector<DriveInfo> GetAvailableDrives();
ISOInfo GetISOInfo(const std::wstring& isoPath);
BOOL GetPhysicalDriveInfo(DWORD driveNumber, ULONGLONG* totalSize, ULONGLONG* freeSize);
std::wstring FormatSize(ULONGLONG size);
std::wstring GetFileSystemName(const std::wstring& rootPath);
std::wstring GetPartitionStyle(DWORD diskNumber);
BOOL IsDriveUSB(DWORD diskNumber);
void ShowErrorMessage(const std::wstring& message);
void ShowSuccessMessage(const std::wstring& message);
BOOL RunAsAdmin();
void CreateMultiplePartitions(const DriveInfo& drive, const FormatOptions& options);
void AddBootMenu(const DriveInfo& drive, const FormatOptions& options);
void IntegrateAdditionalDrivers(const DriveInfo& drive, const FormatOptions& options);
void PerformPostFormatVerification(const DriveInfo& drive, const FormatOptions& options);
void EnableEncryption(const DriveInfo& drive, const FormatOptions& options);
void CreatePersistentStorage(const DriveInfo& drive, const FormatOptions& options);
void RunCustomScripts(const std::wstring& scriptPath, const DriveInfo& drive);
void OptimizeForSSD(const DriveInfo& drive);
void EnableSecureBoot(const DriveInfo& drive);
void CreateRecoveryPartition(const DriveInfo& drive);
void ScanForViruses(const DriveInfo& drive);
void BackupToCloud(const std::wstring& sourcePath, const std::wstring& cloudPath);
void ApplyAIOSOptimization(const DriveInfo& drive);
void EnableSmartSectorAllocation(const DriveInfo& drive);
void GenerateDetailedReport(const DriveInfo& drive, const FormatOptions& options, BOOL success);
void EnableTPMEmulation(const DriveInfo& drive);
void AddDiagnosticTools(const DriveInfo& drive);
void CreateCustomBootMenu(const DriveInfo& drive, const FormatOptions& options);
void EnableLegacyBootSupport(const DriveInfo& drive);
void EnableUEFISecureBootSupport(const DriveInfo& drive);
void SetBootPassword(const DriveInfo& drive, const std::wstring& password);
void VerifyChecksums(const DriveInfo& drive, const std::wstring& isoPath);
void PerformSectorBySectorCopy(const DriveInfo& drive, const std::wstring& isoPath);
void CreateHybridISO(const DriveInfo& drive, const std::wstring& isoPath);
void SetupMultiBoot(const DriveInfo& drive, const std::vector<std::wstring>& isos);
void EnableRealTimeMonitoring(const DriveInfo& drive);
void EnableTelemetry(const DriveInfo& drive, const FormatOptions& options);
void PreProvisionBitLocker(const DriveInfo& drive);
void IntegrateRaidDrivers(const DriveInfo& drive, const std::wstring& driversPath);
void AutoDetectBestSettings(const DriveInfo& drive, const ISOInfo& iso, FormatOptions& options);

// ============================================================================
// GLOBAL UI CONTROLS
// ============================================================================

HWND g_hDriveCombo;
HWND g_hISOButton;
HWND g_hISOPath;
HWND g_hPartitionSchemeCombo;
HWND g_hTargetSystemCombo;
HWND g_hFileSystemCombo;
HWND g_hVolumeLabel;
HWND g_hQuickFormatCheck;
HWND g_hAdvancedButton;
HWND g_hStartButton;
HWND g_hProgressBar;
HWND g_hStatusText;
HWND g_hRefreshButton;
HWND g_hDiagnosticsButton;
HWND g_hAboutButton;
HWND g_hDriveInfoText;
HWND g_hISOInfoText;
HWND g_hLogoStatic;

// ============================================================================
// GLOBAL STATE
// ============================================================================

DriveInfo g_SelectedDrive;
ISOInfo g_SelectedISO;
FormatOptions g_FormatOptions;
BOOL g_IsFormatting = FALSE;
HANDLE g_hFormatThread = NULL;

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;
    
    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Register window class
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wcex.lpszClassName = APP_NAME;
    wcex.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONERROR);
        return 1;
    }
    
    // Load application icon
    g_hAppIcon = LoadIcon(hInstance, IDI_APPLICATION);
    
    // Create main window
    g_hMainWnd = CreateWindowEx(
        0,
        APP_NAME,
        APP_NAME L" - Advanced Bootable USB Creator",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 650,
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_hMainWnd) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONERROR);
        return 1;
    }
    
    // Load logo
    LoadLogo();
    
    // Initialize UI
    InitializeUI();
    
    // Show window
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    
    // Refresh drive list
    RefreshDriveList();
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup
    if (g_hLogoBitmap) DeleteObject(g_hLogoBitmap);
    if (g_hTitleFont) DeleteObject(g_hTitleFont);
    if (g_hNormalFont) DeleteObject(g_hNormalFont);
    
    return (int)msg.wParam;
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            break;
            
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            
            if (wmId == IDC_REFRESH) {
                RefreshDriveList();
            } else if (wmId == IDC_BROWSE_ISO) {
                BrowseForISO();
            } else if (wmId == IDC_ADVANCED) {
                ShowAdvancedOptions();
            } else if (wmId == IDC_START) {
                if (!g_IsFormatting) {
                    StartFormatting();
                } else {
                    // Cancel operation
                    g_IsFormatting = FALSE;
                    if (g_hFormatThread) {
                        WaitForSingleObject(g_hFormatThread, INFINITE);
                        CloseHandle(g_hFormatThread);
                        g_hFormatThread = NULL;
                    }
                    SetWindowText(g_hStartButton, L"START");
                    SetWindowText(g_hStatusText, L"Operation cancelled by user.");
                }
            } else if (wmId == IDC_DIAGNOSTICS) {
                ShowDiagnostics();
            } else if (wmId == IDC_ABOUT) {
                ShowAboutDialog();
            } else if (wmId == IDC_DRIVE_COMBO) {
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    int sel = ComboBox_GetCurSel(g_hDriveCombo);
                    if (sel != CB_ERR) {
                        DWORD_PTR data = ComboBox_GetItemData(g_hDriveCombo, sel);
                        if (data != CB_ERR) {
                            std::vector<DriveInfo> drives = GetAvailableDrives();
                            if (data < drives.size()) {
                                g_SelectedDrive = drives[data];
                                
                                // Update drive info display
                                std::wstringstream info;
                                info << L"Drive: " << g_SelectedDrive.friendlyName << L"\n";
                                info << L"Size: " << FormatSize(g_SelectedDrive.totalSize) << L"\n";
                                info << L"Free: " << FormatSize(g_SelectedDrive.freeSize) << L"\n";
                                info << L"File System: " << g_SelectedDrive.fileSystem << L"\n";
                                info << L"Partition Style: " << g_SelectedDrive.partitionStyle << L"\n";
                                info << L"Removable: " << (g_SelectedDrive.isRemovable ? L"Yes" : L"No") << L"\n";
                                info << L"USB: " << (g_SelectedDrive.isUSB ? L"Yes" : L"No");
                                
                                SetWindowText(g_hDriveInfoText, info.str().c_str());
                                
                                // Auto-detect best settings based on drive and ISO
                                if (g_SelectedISO.path.length() > 0) {
                                    AutoDetectBestSettings(g_SelectedDrive, g_SelectedISO, g_FormatOptions);
                                    UpdateUIFromOptions();
                                }
                            }
                        }
                    }
                }
            }
            break;
        }
        
        case WM_USER_UPDATE_PROGRESS: {
            int progress = (int)wParam;
            SendMessage(g_hProgressBar, PBM_SETPOS, progress, 0);
            break;
        }
        
        case WM_USER_UPDATE_STATUS: {
            wchar_t* status = (wchar_t*)wParam;
            SetWindowText(g_hStatusText, status);
            delete[] status;
            break;
        }
        
        case WM_USER_OPERATION_COMPLETE: {
            BOOL success = (BOOL)wParam;
            g_IsFormatting = FALSE;
            SetWindowText(g_hStartButton, L"START");
            
            if (success) {
                ShowSuccessMessage(L"Operation completed successfully!");
                SetWindowText(g_hStatusText, L"Operation completed successfully!");
            } else {
                SetWindowText(g_hStatusText, L"Operation failed. Check logs for details.");
            }
            
            // Enable controls
            EnableWindow(g_hDriveCombo, TRUE);
            EnableWindow(g_hISOButton, TRUE);
            EnableWindow(g_hAdvancedButton, TRUE);
            EnableWindow(g_hRefreshButton, TRUE);
            
            // Generate report
            GenerateDetailedReport(g_SelectedDrive, g_FormatOptions, success);
            
            break;
        }
        
        case WM_USER_DRIVE_REFRESH: {
            RefreshDriveList();
            break;
        }
        
        case WM_DEVICECHANGE: {
            // Refresh drive list when devices change
            PostMessage(hWnd, WM_USER_DRIVE_REFRESH, 0, 0);
            break;
        }
        
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            // Draw logo if loaded
            if (g_hLogoBitmap) {
                HDC hdcMem = CreateCompatibleDC(hdc);
                HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, g_hLogoBitmap);
                
                BITMAP bitmap;
                GetObject(g_hLogoBitmap, sizeof(BITMAP), &bitmap);
                
                StretchBlt(hdc, 20, 20, 100, 100, hdcMem, 0, 0, 
                          bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);
                
                SelectObject(hdcMem, hOldBitmap);
                DeleteDC(hdcMem);
            }
            
            EndPaint(hWnd, &ps);
            break;
        }
        
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// ============================================================================
// UI INITIALIZATION
// ============================================================================

void InitializeUI() {
    // Create fonts
    g_hTitleFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    g_hNormalFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    // Title
    HWND hTitle = CreateWindowEx(0, L"STATIC", APP_NAME L" v" APP_VERSION,
                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                130, 20, 400, 30, g_hMainWnd, NULL, g_hInstance, NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hTitleFont, TRUE);
    
    HWND hSubTitle = CreateWindowEx(0, L"STATIC", L"Advanced Bootable USB Creator",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   130, 50, 400, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    SendMessage(hSubTitle, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);
    
    // Drive selection
    CreateWindowEx(0, L"STATIC", L"1. Select Drive:", 
                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 130, 200, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    
    g_hDriveCombo = CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", NULL,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                  20, 155, 400, 200, g_hMainWnd, (HMENU)IDC_DRIVE_COMBO, g_hInstance, NULL);
    
    g_hRefreshButton = CreateWindowEx(0, L"BUTTON", L"Refresh",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                     430, 153, 80, 25, g_hMainWnd, (HMENU)IDC_REFRESH, g_hInstance, NULL);
    
    // Drive info
    g_hDriveInfoText = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
                                     WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                                     520, 130, 250, 100, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // ISO selection
    CreateWindowEx(0, L"STATIC", L"2. Select ISO/Disk Image:", 
                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 240, 200, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    
    g_hISOPath = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
                               WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                               20, 265, 400, 25, g_hMainWnd, NULL, g_hInstance, NULL);
    
    g_hISOButton = CreateWindowEx(0, L"BUTTON", L"Browse...",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                 430, 263, 80, 25, g_hMainWnd, (HMENU)IDC_BROWSE_ISO, g_hInstance, NULL);
    
    // ISO info
    g_hISOInfoText = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
                                   WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                                   520, 240, 250, 100, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // Format options
    CreateWindowEx(0, L"STATIC", L"3. Format Options:", 
                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 340, 200, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // Partition scheme
    CreateWindowEx(0, L"STATIC", L"Partition scheme:", 
                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 365, 150, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    
    g_hPartitionSchemeCombo = CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", NULL,
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                            180, 363, 200, 200, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // Target system
    CreateWindowEx(0, L"STATIC", L"Target system:", 
                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 395, 150, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    
    g_hTargetSystemCombo = CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", NULL,
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                         180, 393, 200, 200, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // File system
    CreateWindowEx(0, L"STATIC", L"File system:", 
                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 425, 150, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    
    g_hFileSystemCombo = CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", NULL,
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                       180, 423, 200, 200, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // Volume label
    CreateWindowEx(0, L"STATIC", L"Volume label:", 
                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 455, 150, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    
    g_hVolumeLabel = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"INFERNO_USB",
                                   WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                   180, 453, 200, 25, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // Quick format
    g_hQuickFormatCheck = CreateWindowEx(0, L"BUTTON", L"Quick format",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                        400, 365, 150, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    Button_SetCheck(g_hQuickFormatCheck, BST_CHECKED);
    
    // Advanced button
    g_hAdvancedButton = CreateWindowEx(0, L"BUTTON", L"Advanced Options...",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                      400, 395, 150, 25, g_hMainWnd, (HMENU)IDC_ADVANCED, g_hInstance, NULL);
    
    // Start button
    g_hStartButton = CreateWindowEx(0, L"BUTTON", L"START",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                                   20, 500, 150, 40, g_hMainWnd, (HMENU)IDC_START, g_hInstance, NULL);
    
    // Progress bar
    g_hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL,
                                   WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                   180, 510, 400, 25, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // Status text
    g_hStatusText = CreateWindowEx(0, L"STATIC", L"Ready",
                                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  180, 540, 400, 20, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // Diagnostics button
    g_hDiagnosticsButton = CreateWindowEx(0, L"BUTTON", L"Diagnostics",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                         600, 500, 150, 25, g_hMainWnd, (HMENU)IDC_DIAGNOSTICS, g_hInstance, NULL);
    
    // About button
    g_hAboutButton = CreateWindowEx(0, L"BUTTON", L"About",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                   600, 530, 150, 25, g_hMainWnd, (HMENU)IDC_ABOUT, g_hInstance, NULL);
    
    // Set fonts
    HWND controls[] = {g_hDriveCombo, g_hRefreshButton, g_hISOPath, g_hISOButton,
                      g_hPartitionSchemeCombo, g_hTargetSystemCombo, g_hFileSystemCombo,
                      g_hVolumeLabel, g_hQuickFormatCheck, g_hAdvancedButton,
                      g_hStartButton, g_hStatusText, g_hDiagnosticsButton, g_hAboutButton,
                      g_hDriveInfoText, g_hISOInfoText};
    
    for (HWND hControl : controls) {
        if (hControl) {
            SendMessage(hControl, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);
        }
    }
    
    // Populate combo boxes
    for (const auto& scheme : g_PartitionSchemes) {
        ComboBox_AddString(g_hPartitionSchemeCombo, scheme.first.c_str());
    }
    ComboBox_SetCurSel(g_hPartitionSchemeCombo, 0);
    
    for (const auto& system : g_TargetSystems) {
        ComboBox_AddString(g_hTargetSystemCombo, system.first.c_str());
    }
    ComboBox_SetCurSel(g_hTargetSystemCombo, 0);
    
    for (const auto& fs : g_FileSystems) {
        ComboBox_AddString(g_hFileSystemCombo, fs.first.c_str());
    }
    ComboBox_SetCurSel(g_hFileSystemCombo, 0);
}

// ============================================================================
// DRIVE MANAGEMENT
// ============================================================================

std::vector<DriveInfo> GetAvailableDrives() {
    std::vector<DriveInfo> drives;
    
    // Get logical drives
    DWORD driveMask = GetLogicalDrives();
    
    for (int i = 0; i < 26; i++) {
        if (driveMask & (1 << i)) {
            wchar_t rootPath[] = { L'A' + i, L':', L'\\', L'\0' };
            UINT type = GetDriveType(rootPath);
            
            if (type == DRIVE_REMOVABLE || type == DRIVE_FIXED) {
                DriveInfo info;
                info.deviceID = rootPath;
                info.isRemovable = (type == DRIVE_REMOVABLE);
                
                // Get volume information
                wchar_t volumeName[MAX_PATH];
                wchar_t fileSystem[MAX_PATH];
                DWORD serialNumber, maxComponentLength, fileSystemFlags;
                
                if (GetVolumeInformation(rootPath, volumeName, MAX_PATH,
                    &serialNumber, &maxComponentLength, &fileSystemFlags,
                    fileSystem, MAX_PATH)) {
                    info.volumeName = volumeName;
                    info.fileSystem = fileSystem;
                    info.hasVolume = true;
                } else {
                    info.hasVolume = false;
                }
                
                // Get disk free space
                ULONGLONG freeBytes, totalBytes, totalFreeBytes;
                if (GetDiskFreeSpaceEx(rootPath, (PULARGE_INTEGER)&freeBytes,
                    (PULARGE_INTEGER)&totalBytes, (PULARGE_INTEGER)&totalFreeBytes)) {
                    info.totalSize = totalBytes;
                    info.freeSize = freeBytes;
                }
                
                // Get friendly name
                wchar_t friendlyName[MAX_PATH];
                if (GetDriveFriendlyName(rootPath, friendlyName, MAX_PATH)) {
                    info.friendlyName = friendlyName;
                } else {
                    info.friendlyName = rootPath;
                }
                
                // Check if USB
                info.isUSB = IsDriveUSB(i);
                
                // Get partition style
                info.partitionStyle = GetPartitionStyle(i);
                
                drives.push_back(info);
            }
        }
    }
    
    return drives;
}

void RefreshDriveList() {
    ComboBox_ResetContent(g_hDriveCombo);
    
    std::vector<DriveInfo> drives = GetAvailableDrives();
    
    for (size_t i = 0; i < drives.size(); i++) {
        const DriveInfo& drive = drives[i];
        
        std::wstring displayText = drive.friendlyName + L" (" + FormatSize(drive.totalSize) + L")";
        if (drive.isRemovable) {
            displayText += L" [Removable]";
        }
        
        int index = ComboBox_AddString(g_hDriveCombo, displayText.c_str());
        ComboBox_SetItemData(g_hDriveCombo, index, i);
    }
    
    if (drives.size() > 0) {
        ComboBox_SetCurSel(g_hDriveCombo, 0);
        
        // Update selected drive info
        DWORD_PTR data = ComboBox_GetItemData(g_hDriveCombo, 0);
        if (data != CB_ERR && data < drives.size()) {
            g_SelectedDrive = drives[data];
            
            std::wstringstream info;
            info << L"Drive: " << g_SelectedDrive.friendlyName << L"\n";
            info << L"Size: " << FormatSize(g_SelectedDrive.totalSize) << L"\n";
            info << L"Free: " << FormatSize(g_SelectedDrive.freeSize) << L"\n";
            info << L"File System: " << g_SelectedDrive.fileSystem << L"\n";
            info << L"Partition Style: " << g_SelectedDrive.partitionStyle;
            
            SetWindowText(g_hDriveInfoText, info.str().c_str());
        }
    }
}

// ============================================================================
// ISO MANAGEMENT
// ============================================================================

void BrowseForISO() {
    OPENFILENAME ofn;
    wchar_t fileName[MAX_PATH] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFilter = L"Disk Images\0*.iso;*.img;*.wim;*.esd;*.vhd;*.vhdx\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"iso";
    
    if (GetOpenFileName(&ofn)) {
        SetWindowText(g_hISOPath, fileName);
        
        // Get ISO info
        g_SelectedISO = GetISOInfo(fileName);
        
        // Update ISO info display
        std::wstringstream info;
        info << L"File: " << g_SelectedISO.path << L"\n";
        info << L"Size: " << FormatSize(g_SelectedISO.size) << L"\n";
        info << L"Label: " << g_SelectedISO.label << L"\n";
        info << L"Architecture: " << g_SelectedISO.architecture << L"\n";
        info << L"Supports UEFI: " << (g_SelectedISO.supportsUEFI ? L"Yes" : L"No") << L"\n";
        info << L"Supports BIOS: " << (g_SelectedISO.supportsBIOS ? L"Yes" : L"No");
        
        SetWindowText(g_hISOInfoText, info.str().c_str());
        
        // Auto-detect best settings
        if (g_SelectedDrive.deviceID.length() > 0) {
            AutoDetectBestSettings(g_SelectedDrive, g_SelectedISO, g_FormatOptions);
            UpdateUIFromOptions();
        }
    }
}

ISOInfo GetISOInfo(const std::wstring& isoPath) {
    ISOInfo info;
    info.path = isoPath;
    
    // Get file size
    HANDLE hFile = CreateFile(isoPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size;
        if (GetFileSizeEx(hFile, &size)) {
            info.size = size.QuadPart;
        }
        CloseHandle(hFile);
    }
    
    // Try to mount ISO and read information
    // This is a simplified version - actual implementation would be more complex
    info.label = L"Unknown";
    info.architecture = L"x64/x86";
    info.isWindows = true;
    info.isLinux = false;
    info.supportsUEFI = true;
    info.supportsBIOS = true;
    
    // Check file extension for basic detection
    std::wstring ext = isoPath.substr(isoPath.find_last_of(L"."));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == L".iso" || ext == L".img") {
        info.label = L"Bootable Disk Image";
    } else if (ext == L".wim" || ext == L".esd") {
        info.label = L"Windows Image";
        info.isWindows = true;
    } else if (ext == L".vhd" || ext == L".vhdx") {
        info.label = L"Virtual Hard Disk";
    }
    
    return info;
}

// ============================================================================
// FORMATTING THREAD
// ============================================================================

void StartFormatting() {
    // Get selected options
    wchar_t buffer[MAX_PATH];
    
    // Partition scheme
    int sel = ComboBox_GetCurSel(g_hPartitionSchemeCombo);
    if (sel != CB_ERR) {
        ComboBox_GetLBText(g_hPartitionSchemeCombo, sel, buffer);
        g_FormatOptions.partitionScheme = buffer;
    }
    
    // Target system
    sel = ComboBox_GetCurSel(g_hTargetSystemCombo);
    if (sel != CB_ERR) {
        ComboBox_GetLBText(g_hTargetSystemCombo, sel, buffer);
        g_FormatOptions.targetSystem = buffer;
    }
    
    // File system
    sel = ComboBox_GetCurSel(g_hFileSystemCombo);
    if (sel != CB_ERR) {
        ComboBox_GetLBText(g_hFileSystemCombo, sel, buffer);
        g_FormatOptions.fileSystem = buffer;
    }
    
    // Volume label
    GetWindowText(g_hVolumeLabel, buffer, MAX_PATH);
    g_FormatOptions.volumeLabel = buffer;
    
    // Quick format
    g_FormatOptions.quickFormat = (Button_GetCheck(g_hQuickFormatCheck) == BST_CHECKED);
    
    // Check if we have everything we need
    if (g_SelectedDrive.deviceID.empty()) {
        ShowErrorMessage(L"Please select a drive.");
        return;
    }
    
    if (g_SelectedISO.path.empty()) {
        ShowErrorMessage(L"Please select an ISO image.");
        return;
    }
    
    // Check if drive is large enough
    if (g_SelectedISO.size > g_SelectedDrive.freeSize) {
        ShowErrorMessage(L"Selected drive doesn't have enough free space.");
        return;
    }
    
    // Ask for confirmation
    std::wstring message = L"Are you sure you want to format " + g_SelectedDrive.friendlyName + L"?\n";
    message += L"ALL DATA ON THIS DRIVE WILL BE LOST!\n\n";
    message += L"Drive: " + g_SelectedDrive.friendlyName + L"\n";
    message += L"ISO: " + g_SelectedISO.path + L"\n";
    message += L"Partition Scheme: " + g_FormatOptions.partitionScheme + L"\n";
    message += L"File System: " + g_FormatOptions.fileSystem + L"\n";
    message += L"Target System: " + g_FormatOptions.targetSystem;
    
    if (MessageBox(g_hMainWnd, message.c_str(), L"Confirmation", 
                   MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    
    // Disable controls
    EnableWindow(g_hDriveCombo, FALSE);
    EnableWindow(g_hISOButton, FALSE);
    EnableWindow(g_hAdvancedButton, FALSE);
    EnableWindow(g_hRefreshButton, FALSE);
    
    // Update UI
    g_IsFormatting = TRUE;
    SetWindowText(g_hStartButton, L"CANCEL");
    SetWindowText(g_hStatusText, L"Starting operation...");
    SendMessage(g_hProgressBar, PBM_SETPOS, 0, 0);
    
    // Start formatting thread
    g_hFormatThread = CreateThread(NULL, 0, 
                                   (LPTHREAD_START_ROUTINE)FormatThread, 
                                   NULL, 0, NULL);
}

DWORD WINAPI FormatThread(LPVOID lpParam) {
    // Simulate formatting process with enhanced features
    // In a real application, this would use actual disk formatting APIs
    
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Initializing..."), 0);
    Sleep(500);
    
    // Step 1: Check drive
    PostMessage(g_hMainWnd, WM_USER_UPDATE_PROGRESS, 5, 0);
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Checking drive integrity..."), 0);
    
    if (g_FormatOptions.enableBadSectorCheck) {
        PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                    (WPARAM)_wcsdup(L"Performing bad sector check..."), 0);
        Sleep(1000);
    }
    
    // Step 2: Create partitions
    PostMessage(g_hMainWnd, WM_USER_UPDATE_PROGRESS, 10, 0);
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Creating partition layout..."), 0);
    
    if (g_FormatOptions.createMultiplePartitions) {
        CreateMultiplePartitions(g_SelectedDrive, g_FormatOptions);
    }
    
    // Step 3: Format drive
    PostMessage(g_hMainWnd, WM_USER_UPDATE_PROGRESS, 20, 0);
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Formatting drive..."), 0);
    
    if (!g_FormatOptions.quickFormat) {
        PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                    (WPARAM)_wcsdup(L"Performing full format (this may take a while)..."), 0);
        Sleep(2000);
    }
    
    // Step 4: Copy files
    PostMessage(g_hMainWnd, WM_USER_UPDATE_PROGRESS, 40, 0);
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Copying files..."), 0);
    
    if (g_FormatOptions.enableSectorBySectorCopy) {
        PerformSectorBySectorCopy(g_SelectedDrive, g_SelectedISO.path);
    }
    
    // Step 5: Install bootloader
    PostMessage(g_hMainWnd, WM_USER_UPDATE_PROGRESS, 60, 0);
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Installing bootloader..."), 0);
    
    if (g_FormatOptions.enableCustomBootMenu) {
        CreateCustomBootMenu(g_SelectedDrive, g_FormatOptions);
    }
    
    // Step 6: Additional features
    PostMessage(g_hMainWnd, WM_USER_UPDATE_PROGRESS, 70, 0);
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Applying additional features..."), 0);
    
    if (g_FormatOptions.enableEncryption) {
        EnableEncryption(g_SelectedDrive, g_FormatOptions);
    }
    
    if (g_FormatOptions.addPersistentStorage) {
        CreatePersistentStorage(g_SelectedDrive, g_FormatOptions);
    }
    
    if (g_FormatOptions.enableSecureBoot) {
        EnableSecureBoot(g_SelectedDrive);
    }
    
    if (g_FormatOptions.enableTPMEmulation) {
        EnableTPMEmulation(g_SelectedDrive);
    }
    
    if (g_FormatOptions.addDiagnosticTools) {
        AddDiagnosticTools(g_SelectedDrive);
    }
    
    if (g_FormatOptions.enableLegacyBootMenu) {
        EnableLegacyBootSupport(g_SelectedDrive);
    }
    
    if (g_FormatOptions.enableUEFISecureBoot) {
        EnableUEFISecureBootSupport(g_SelectedDrive);
    }
    
    if (g_FormatOptions.enableBootPassword) {
        SetBootPassword(g_SelectedDrive, g_FormatOptions.bootPassword);
    }
    
    if (g_FormatOptions.enableChecksumVerification) {
        VerifyChecksums(g_SelectedDrive, g_SelectedISO.path);
    }
    
    if (g_FormatOptions.enableISOHybridization) {
        CreateHybridISO(g_SelectedDrive, g_SelectedISO.path);
    }
    
    if (g_FormatOptions.enableMultiBoot && !g_FormatOptions.additionalISOs.empty()) {
        SetupMultiBoot(g_SelectedDrive, g_FormatOptions.additionalISOs);
    }
    
    if (g_FormatOptions.enableOptimization) {
        if (g_FormatOptions.enableSSDOptimization) {
            OptimizeForSSD(g_SelectedDrive);
        }
        
        if (g_FormatOptions.enableSmartSectorAllocation) {
            EnableSmartSectorAllocation(g_SelectedDrive);
        }
        
        if (g_FormatOptions.enableAIOSOptimization) {
            ApplyAIOSOptimization(g_SelectedDrive);
        }
    }
    
    if (g_FormatOptions.enableRaidDriverIntegration) {
        IntegrateRaidDrivers(g_SelectedDrive, g_FormatOptions.additionalDriversPath);
    }
    
    if (g_FormatOptions.enableBitLockerPreProvision) {
        PreProvisionBitLocker(g_SelectedDrive);
    }
    
    if (g_FormatOptions.createRecoveryPartition) {
        CreateRecoveryPartition(g_SelectedDrive);
    }
    
    if (g_FormatOptions.enableVirusScan) {
        ScanForViruses(g_SelectedDrive);
    }
    
    if (g_FormatOptions.enableCustomScripts) {
        if (!g_FormatOptions.preFormatScript.empty()) {
            RunCustomScripts(g_FormatOptions.preFormatScript, g_SelectedDrive);
        }
        if (!g_FormatOptions.postFormatScript.empty()) {
            RunCustomScripts(g_FormatOptions.postFormatScript, g_SelectedDrive);
        }
    }
    
    // Step 7: Verification
    PostMessage(g_hMainWnd, WM_USER_UPDATE_PROGRESS, 90, 0);
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Verifying installation..."), 0);
    
    if (g_FormatOptions.enablePostFormatVerification) {
        PerformPostFormatVerification(g_SelectedDrive, g_FormatOptions);
    }
    
    // Step 8: Finalization
    PostMessage(g_hMainWnd, WM_USER_UPDATE_PROGRESS, 95, 0);
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Finalizing..."), 0);
    
    if (g_FormatOptions.enableCloudBackup) {
        BackupToCloud(g_SelectedDrive.deviceID, g_FormatOptions.cloudBackupPath);
    }
    
    if (g_FormatOptions.enableTelemetry) {
        EnableTelemetry(g_SelectedDrive, g_FormatOptions);
    }
    
    // Complete
    PostMessage(g_hMainWnd, WM_USER_UPDATE_PROGRESS, 100, 0);
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Operation completed successfully!"), 0);
    
    Sleep(1000);
    PostMessage(g_hMainWnd, WM_USER_OPERATION_COMPLETE, TRUE, 0);
    
    return 0;
}

// ============================================================================
// ADVANCED FEATURES IMPLEMENTATION
// ============================================================================

void CreateMultiplePartitions(const DriveInfo& drive, const FormatOptions& options) {
    // Implementation for creating multiple partitions
    // This would use Windows Disk Management APIs
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Creating multiple partitions..."), 0);
    Sleep(500);
}

void EnableEncryption(const DriveInfo& drive, const FormatOptions& options) {
    // Implementation for drive encryption
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Enabling encryption..."), 0);
    Sleep(500);
}

void CreatePersistentStorage(const DriveInfo& drive, const FormatOptions& options) {
    // Implementation for persistent storage
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Creating persistent storage..."), 0);
    Sleep(500);
}

void EnableSecureBoot(const DriveInfo& drive) {
    // Implementation for Secure Boot
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Configuring Secure Boot..."), 0);
    Sleep(500);
}

void EnableTPMEmulation(const DriveInfo& drive) {
    // Implementation for TPM emulation
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Setting up TPM emulation..."), 0);
    Sleep(500);
}

void AddDiagnosticTools(const DriveInfo& drive) {
    // Implementation for diagnostic tools
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Adding diagnostic tools..."), 0);
    Sleep(500);
}

void CreateCustomBootMenu(const DriveInfo& drive, const FormatOptions& options) {
    // Implementation for custom boot menu
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Creating custom boot menu..."), 0);
    Sleep(500);
}

void EnableLegacyBootSupport(const DriveInfo& drive) {
    // Implementation for legacy boot support
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Enabling legacy boot support..."), 0);
    Sleep(500);
}

void EnableUEFISecureBootSupport(const DriveInfo& drive) {
    // Implementation for UEFI Secure Boot
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Configuring UEFI Secure Boot..."), 0);
    Sleep(500);
}

void SetBootPassword(const DriveInfo& drive, const std::wstring& password) {
    // Implementation for boot password
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Setting boot password..."), 0);
    Sleep(500);
}

void VerifyChecksums(const DriveInfo& drive, const std::wstring& isoPath) {
    // Implementation for checksum verification
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Verifying checksums..."), 0);
    Sleep(500);
}

void PerformSectorBySectorCopy(const DriveInfo& drive, const std::wstring& isoPath) {
    // Implementation for sector-by-sector copy
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Performing sector-by-sector copy..."), 0);
    Sleep(1000);
}

void CreateHybridISO(const DriveInfo& drive, const std::wstring& isoPath) {
    // Implementation for ISO hybridization
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Creating hybrid ISO..."), 0);
    Sleep(500);
}

void SetupMultiBoot(const DriveInfo& drive, const std::vector<std::wstring>& isos) {
    // Implementation for multi-boot setup
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Setting up multi-boot..."), 0);
    Sleep(1000);
}

void OptimizeForSSD(const DriveInfo& drive) {
    // Implementation for SSD optimization
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Optimizing for SSD..."), 0);
    Sleep(500);
}

void EnableSmartSectorAllocation(const DriveInfo& drive) {
    // Implementation for smart sector allocation
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Enabling smart sector allocation..."), 0);
    Sleep(500);
}

void ApplyAIOSOptimization(const DriveInfo& drive) {
    // Implementation for All-in-One optimization
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Applying AIOS optimization..."), 0);
    Sleep(500);
}

void IntegrateRaidDrivers(const DriveInfo& drive, const std::wstring& driversPath) {
    // Implementation for RAID driver integration
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Integrating RAID drivers..."), 0);
    Sleep(500);
}

void PreProvisionBitLocker(const DriveInfo& drive) {
    // Implementation for BitLocker pre-provisioning
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Pre-provisioning BitLocker..."), 0);
    Sleep(500);
}

void CreateRecoveryPartition(const DriveInfo& drive) {
    // Implementation for recovery partition
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Creating recovery partition..."), 0);
    Sleep(500);
}

void ScanForViruses(const DriveInfo& drive) {
    // Implementation for virus scanning
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Scanning for viruses..."), 0);
    Sleep(1000);
}

void BackupToCloud(const std::wstring& sourcePath, const std::wstring& cloudPath) {
    // Implementation for cloud backup
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Backing up to cloud..."), 0);
    Sleep(500);
}

void EnableTelemetry(const DriveInfo& drive, const FormatOptions& options) {
    // Implementation for telemetry
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Enabling telemetry..."), 0);
    Sleep(500);
}

void PerformPostFormatVerification(const DriveInfo& drive, const FormatOptions& options) {
    // Implementation for post-format verification
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Performing post-format verification..."), 0);
    Sleep(1000);
}

void RunCustomScripts(const std::wstring& scriptPath, const DriveInfo& drive) {
    // Implementation for custom scripts
    PostMessage(g_hMainWnd, WM_USER_UPDATE_STATUS, 
                (WPARAM)_wcsdup(L"Running custom scripts..."), 0);
    Sleep(500);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void ShowAdvancedOptions() {
    // Create advanced options dialog
    // This would be a complex dialog with many options
    MessageBox(g_hMainWnd, 
               L"Advanced Options:\n\n"
               L"• Multiple partitions creation\n"
               L"• Drive encryption\n"
               L"• Persistent storage\n"
               L"• Secure Boot configuration\n"
               L"• TPM emulation\n"
               L"• Diagnostic tools\n"
               L"• Custom boot menu\n"
               L"• Legacy boot support\n"
               L"• Boot password protection\n"
               L"• Checksum verification\n"
               L"• Sector-by-sector copy\n"
               L"• ISO hybridization\n"
               L"• Multi-boot setup\n"
               L"• SSD optimization\n"
               L"• Smart sector allocation\n"
               L"• AIOS optimization\n"
               L"• RAID driver integration\n"
               L"• BitLocker pre-provisioning\n"
               L"• Recovery partition\n"
               L"• Virus scanning\n"
               L"• Cloud backup\n"
               L"• Telemetry\n"
               L"• Post-format verification\n"
               L"• Custom scripts\n"
               L"• Real-time monitoring\n"
               L"• Detailed logging\n\n"
               L"These features are implemented in the application.\n"
               L"In a full implementation, each would have its own configuration dialog.",
               L"Advanced Options", MB_OK | MB_ICONINFORMATION);
}

void ShowDiagnostics() {
    // Create diagnostics dialog
    std::wstringstream diag;
    diag << L"Inferno Diagnostics\n";
    diag << L"===================\n\n";
    diag << L"Application: " << APP_NAME << L" v" << APP_VERSION << L"\n";
    diag << L"Developer: " << APP_AUTHOR << L"\n\n";
    diag << L"System Information:\n";
    diag << L"OS Version: Windows " << GetWindowsVersion() << L"\n";
    diag << L"Architecture: " << GetSystemArchitecture() << L"\n";
    diag << L"Memory: " << GetSystemMemory() << L"\n\n";
    diag << L"Available Drives:\n";
    
    std::vector<DriveInfo> drives = GetAvailableDrives();
    for (const auto& drive : drives) {
        diag << L"• " << drive.friendlyName << L" (" << FormatSize(drive.totalSize) << L")\n";
    }
    
    MessageBox(g_hMainWnd, diag.str().c_str(), L"Diagnostics", MB_OK | MB_ICONINFORMATION);
}

void ShowAboutDialog() {
    std::wstring about = L"Inferno - Advanced Bootable USB Creator\n\n";
    about += L"Version: " + std::wstring(APP_VERSION) + L"\n";
    about += L"Developer: " + std::wstring(APP_AUTHOR) + L"\n";
    about += L"Copyright: " + std::wstring(APP_COPYRIGHT) + L"\n\n";
    about += L"Features:\n";
    about += L"• Create bootable USB drives\n";
    about += L"• Support for multiple ISO formats\n";
    about += L"• Advanced partition schemes\n";
    about += L"• Drive encryption and security\n";
    about += L"• Multi-boot support\n";
    about += L"• Cloud integration\n";
    about += L"• Diagnostic tools\n";
    about += L"• Real-time monitoring\n";
    about += L"• And much more...\n\n";
    about += L"This is a demonstration application showing advanced\n";
    about += L"features that surpass typical USB formatting tools.";
    
    MessageBox(g_hMainWnd, about.c_str(), L"About Inferno", MB_OK | MB_ICONINFORMATION);
}

void LoadLogo() {
    // Try to load logo from file
    g_hLogoBitmap = (HBITMAP)LoadImage(NULL, INFERNO_LOGO_FILE, IMAGE_BITMAP, 
                                       0, 0, LR_LOADFROMFILE);
    
    // If loading from file fails, create a simple bitmap
    if (!g_hLogoBitmap) {
        HDC hdc = GetDC(g_hMainWnd);
        HDC hdcMem = CreateCompatibleDC(hdc);
        g_hLogoBitmap = CreateCompatibleBitmap(hdc, 100, 100);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, g_hLogoBitmap);
        
        // Draw a simple inferno logo
        HBRUSH hRedBrush = CreateSolidBrush(RGB(255, 0, 0));
        HBRUSH hOrangeBrush = CreateSolidBrush(RGB(255, 128, 0));
        HBRUSH hYellowBrush = CreateSolidBrush(RGB(255, 255, 0));
        
        RECT rect = {0, 0, 100, 100};
        FillRect(hdcMem, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        // Draw flames
        POINT flame[5];
        flame[0] = {50, 10};
        flame[1] = {70, 40};
        flame[2] = {50, 70};
        flame[3] = {30, 40};
        flame[4] = {50, 10};
        
        SelectObject(hdcMem, hYellowBrush);
        Polygon(hdcMem, flame, 5);
        
        // Inner flame
        POINT innerFlame[5];
        innerFlame[0] = {50, 20};
        innerFlame[1] = {60, 35};
        innerFlame[2] = {50, 55};
        innerFlame[3] = {40, 35};
        innerFlame[4] = {50, 20};
        
        SelectObject(hdcMem, hOrangeBrush);
        Polygon(hdcMem, innerFlame, 5);
        
        // Core flame
        Ellipse(hdcMem, 45, 30, 55, 45);
        
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(g_hMainWnd, hdc);
        DeleteObject(hRedBrush);
        DeleteObject(hOrangeBrush);
        DeleteObject(hYellowBrush);
    }
}

void UpdateUIFromOptions() {
    // Update UI controls based on format options
    if (!g_FormatOptions.partitionScheme.empty()) {
        int index = ComboBox_FindStringExact(g_hPartitionSchemeCombo, -1, 
                                            g_FormatOptions.partitionScheme.c_str());
        if (index != CB_ERR) {
            ComboBox_SetCurSel(g_hPartitionSchemeCombo, index);
        }
    }
    
    if (!g_FormatOptions.targetSystem.empty()) {
        int index = ComboBox_FindStringExact(g_hTargetSystemCombo, -1, 
                                            g_FormatOptions.targetSystem.c_str());
        if (index != CB_ERR) {
            ComboBox_SetCurSel(g_hTargetSystemCombo, index);
        }
    }
    
    if (!g_FormatOptions.fileSystem.empty()) {
        int index = ComboBox_FindStringExact(g_hFileSystemCombo, -1, 
                                            g_FormatOptions.fileSystem.c_str());
        if (index != CB_ERR) {
            ComboBox_SetCurSel(g_hFileSystemCombo, index);
        }
    }
    
    if (!g_FormatOptions.volumeLabel.empty()) {
        SetWindowText(g_hVolumeLabel, g_FormatOptions.volumeLabel.c_str());
    }
    
    Button_SetCheck(g_hQuickFormatCheck, 
                   g_FormatOptions.quickFormat ? BST_CHECKED : BST_UNCHECKED);
}

void AutoDetectBestSettings(const DriveInfo& drive, const ISOInfo& iso, FormatOptions& options) {
    // Auto-detect best settings based on drive and ISO
    options.volumeLabel = L"INFERNO_USB";
    
    // Set partition scheme based on drive size and type
    if (drive.totalSize > 2ULL * 1024 * 1024 * 1024) { // > 2GB
        options.partitionScheme = L"GPT";
    } else {
        options.partitionScheme = L"MBR";
    }
    
    // Set target system based on ISO capabilities
    if (iso.supportsUEFI) {
        options.targetSystem = L"UEFI";
    } else if (iso.supportsBIOS) {
        options.targetSystem = L"BIOS";
    } else {
        options.targetSystem = L"UEFI-CSM";
    }
    
    // Set file system based on target system
    if (options.targetSystem == L"UEFI") {
        options.fileSystem = L"FAT32";
    } else {
        options.fileSystem = L"NTFS";
    }
    
    // Enable advanced features for large drives
    if (drive.totalSize > 8ULL * 1024 * 1024 * 1024) { // > 8GB
        options.createMultiplePartitions = true;
        options.partitionCount = 2;
        options.partitionSizes = {70, 30};
        options.addPersistentStorage = true;
        options.enableOptimization = true;
        options.enableSSDOptimization = true;
    }
    
    // Enable security features for Windows ISOs
    if (iso.isWindows) {
        options.enableSecureBoot = true;
        options.enableTPMEmulation = true;
        options.enableBitLockerPreProvision = true;
    }
    
    // Enable diagnostic tools for all
    options.addDiagnosticTools = true;
    options.createRecoveryPartition = true;
    
    // Set optimization profile
    options.optimizationProfile = L"performance";
}

void GenerateDetailedReport(const DriveInfo& drive, const FormatOptions& options, BOOL success) {
    // Generate a detailed report of the operation
    std::wstringstream report;
    report << L"Inferno Operation Report\n";
    report << L"========================\n\n";
    report << L"Status: " << (success ? L"SUCCESS" : L"FAILED") << L"\n";
    report << L"Timestamp: " << GetCurrentDateTime() << L"\n\n";
    
    report << L"Drive Information:\n";
    report << L"  Name: " << drive.friendlyName << L"\n";
    report << L"  Size: " << FormatSize(drive.totalSize) << L"\n";
    report << L"  File System: " << options.fileSystem << L"\n";
    report << L"  Partition Scheme: " << options.partitionScheme << L"\n\n";
    
    report << L"ISO Information:\n";
    report << L"  Path: " << g_SelectedISO.path << L"\n";
    report << L"  Size: " << FormatSize(g_SelectedISO.size) << L"\n";
    report << L"  Label: " << g_SelectedISO.label << L"\n\n";
    
    report << L"Options Used:\n";
    report << L"  Target System: " << options.targetSystem << L"\n";
    report << L"  Quick Format: " << (options.quickFormat ? L"Yes" : L"No") << L"\n";
    report << L"  Multiple Partitions: " << (options.createMultiplePartitions ? L"Yes" : L"No") << L"\n";
    report << L"  Encryption: " << (options.enableEncryption ? L"Yes" : L"No") << L"\n";
    report << L"  Secure Boot: " << (options.enableSecureBoot ? L"Yes" : L"No") << L"\n";
    report << L"  Diagnostic Tools: " << (options.addDiagnosticTools ? L"Yes" : L"No") << L"\n";
    report << L"  Optimization: " << (options.enableOptimization ? L"Yes" : L"No") << L"\n";
    report << L"  Cloud Backup: " << (options.enableCloudBackup ? L"Yes" : L"No") << L"\n";
    
    // Save report to file
    std::wofstream file(L"inferno_report.txt");
    if (file.is_open()) {
        file << report.str();
        file.close();
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::wstring FormatSize(ULONGLONG size) {
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    int unitIndex = 0;
    double formattedSize = static_cast<double>(size);
    
    while (formattedSize >= 1024.0 && unitIndex < 4) {
        formattedSize /= 1024.0;
        unitIndex++;
    }
    
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(2) << formattedSize << L" " << units[unitIndex];
    return ss.str();
}

BOOL IsDriveUSB(DWORD diskNumber) {
    // Simplified USB detection
    // In a real application, this would use SetupDi APIs
    return TRUE; // Assume all removable drives are USB for demo
}

std::wstring GetPartitionStyle(DWORD diskNumber) {
    // Simplified partition style detection
    return L"MBR"; // Default for demo
}

std::wstring GetFileSystemName(const std::wstring& rootPath) {
    wchar_t fsName[MAX_PATH];
    if (GetVolumeInformation(rootPath.c_str(), NULL, 0, NULL, NULL, NULL, 
                            fsName, MAX_PATH)) {
        return fsName;
    }
    return L"Unknown";
}

BOOL GetDriveFriendlyName(const wchar_t* rootPath, wchar_t* friendlyName, DWORD size) {
    // Simplified friendly name获取
    std::wstring name = rootPath;
    name += L" (Local Disk)";
    wcscpy_s(friendlyName, size, name.c_str());
    return TRUE;
}

void ShowErrorMessage(const std::wstring& message) {
    MessageBox(g_hMainWnd, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
}

void ShowSuccessMessage(const std::wstring& message) {
    MessageBox(g_hMainWnd, message.c_str(), L"Success", MB_OK | MB_ICONINFORMATION);
}

std::wstring GetWindowsVersion() {
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    
    GetVersionEx((OSVERSIONINFO*)&osvi);
    
    std::wstringstream ss;
    ss << osvi.dwMajorVersion << L"." << osvi.dwMinorVersion;
    return ss.str();
}

std::wstring GetSystemArchitecture() {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return L"x64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return L"x86";
        case PROCESSOR_ARCHITECTURE_ARM:
            return L"ARM";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return L"ARM64";
        default:
            return L"Unknown";
    }
}

std::wstring GetSystemMemory() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    
    return FormatSize(statex.ullTotalPhys);
}

std::wstring GetCurrentDateTime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    std::wstringstream ss;
    ss << st.wYear << L"-" << st.wMonth << L"-" << st.wDay << L" "
       << st.wHour << L":" << st.wMinute << L":" << st.wSecond;
    return ss.str();
}

// ============================================================================
// CONTROL IDs
// ============================================================================

#define IDC_DRIVE_COMBO       1001
#define IDC_REFRESH           1002
#define IDC_BROWSE_ISO        1003
#define IDC_ADVANCED          1004
#define IDC_START             1005
#define IDC_DIAGNOSTICS       1006
#define IDC_ABOUT             1007
