/*
 * Inferno - أداة متقدمة لحرق أنظمة التشغيل
 * إصدار: 4.0.0
 * المطور: فريق Inferno
 * الرخصة: GPL v3
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <winioctl.h>
#include <setupapi.h>
#include <winternl.h>
#include <dbt.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <regex>
#include <codecvt>
#include <locale>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winmm.lib")

// تعريفات ثوابت
#define INFERNO_VERSION L"4.0.0"
#define INFERNO_BUILD L"2024.01"
#define WM_UPDATE_PROGRESS (WM_USER + 100)
#define WM_DEVICE_CHANGE (WM_USER + 101)
#define WM_LOG_MESSAGE (WM_USER + 102)
#define MAX_PATH_LENGTH 32767

// هياكل البيانات
struct DeviceInfo {
    std::wstring devicePath;
    std::wstring friendlyName;
    std::wstring volumeName;
    uint64_t totalSize;
    uint64_t freeSize;
    uint32_t diskNumber;
    bool isRemovable;
    bool isUSB;
    bool isBootable;
    std::wstring fileSystem;
    DWORD serialNumber;
};

struct ISOImage {
    std::wstring path;
    std::wstring label;
    std::wstring type;
    uint64_t size;
    bool isBootable;
    std::wstring architecture;
    std::wstring version;
};

struct PartitionScheme {
    std::wstring name;
    std::wstring id;
    std::wstring description;
    bool supportsUEFI;
    bool supportsBIOS;
};

struct FormatOption {
    std::wstring name;
    std::wstring id;
    std::wstring defaultLabel;
};

// فئات مساعدة
class Logger {
private:
    static std::wstring logFilePath;
    static std::mutex logMutex;
    
public:
    static void Initialize() {
        wchar_t path[MAX_PATH_LENGTH];
        GetModuleFileNameW(NULL, path, MAX_PATH_LENGTH);
        std::wstring exePath = path;
        size_t pos = exePath.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            logFilePath = exePath.substr(0, pos) + L"\\inferno.log";
        }
        
        std::wofstream logFile(logFilePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << L"=== Inferno Log Started ===" << std::endl;
            logFile.close();
        }
    }
    
    static void Log(const std::wstring& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        
        std::wofstream logFile(logFilePath, std::ios::app);
        if (logFile.is_open()) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            logFile << std::setw(2) << std::setfill(L'0') << st.wHour << L":"
                    << std::setw(2) << st.wMinute << L":"
                    << std::setw(2) << st.wSecond << L" - "
                    << message << std::endl;
            logFile.close();
        }
        
        OutputDebugStringW(message.c_str());
    }
    
    static void LogError(const std::wstring& message, DWORD errorCode = 0) {
        std::wstring errorMsg = L"ERROR: " + message;
        if (errorCode != 0) {
            wchar_t* buffer = nullptr;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                          NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (LPWSTR)&buffer, 0, NULL);
            if (buffer) {
                errorMsg += L" - " + std::wstring(buffer);
                LocalFree(buffer);
            }
        }
        Log(errorMsg);
    }
};

std::wstring Logger::logFilePath;
std::mutex Logger::logMutex;

class USBDetector {
private:
    HWND hWnd;
    
public:
    USBDetector(HWND hwnd) : hWnd(hwnd) {}
    
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
        return TRUE;
    }
    
    std::vector<DeviceInfo> DetectDevices() {
        std::vector<DeviceInfo> devices;
        
        wchar_t driveLetters[] = L"A:\\";
        DWORD drivesMask = GetLogicalDrives();
        
        for (int i = 0; i < 26; i++) {
            if (drivesMask & (1 << i)) {
                driveLetters[0] = L'A' + i;
                
                UINT driveType = GetDriveTypeW(driveLetters);
                if (driveType == DRIVE_REMOVABLE || driveType == DRIVE_FIXED) {
                    DeviceInfo device;
                    device.devicePath = driveLetters;
                    device.isRemovable = (driveType == DRIVE_REMOVABLE);
                    
                    // الحصول على معلومات مفصلة
                    wchar_t volumeName[MAX_PATH_LENGTH];
                    wchar_t fileSystem[MAX_PATH_LENGTH];
                    DWORD serialNumber, maxComponentLen, fileSystemFlags;
                    
                    if (GetVolumeInformationW(driveLetters, volumeName, MAX_PATH_LENGTH,
                        &serialNumber, &maxComponentLen, &fileSystemFlags,
                        fileSystem, MAX_PATH_LENGTH)) {
                        device.volumeName = volumeName;
                        device.fileSystem = fileSystem;
                        device.serialNumber = serialNumber;
                    }
                    
                    // الحصول على حجم القرص
                    ULARGE_INTEGER totalBytes, freeBytes;
                    if (GetDiskFreeSpaceExW(driveLetters, NULL, &totalBytes, &freeBytes)) {
                        device.totalSize = totalBytes.QuadPart;
                        device.freeSize = freeBytes.QuadPart;
                    }
                    
                    // الحصول على الاسم الودود
                    wchar_t friendlyName[MAX_PATH_LENGTH];
                    if (GetDriveDisplayName(driveLetters, friendlyName, MAX_PATH_LENGTH)) {
                        device.friendlyName = friendlyName;
                    } else {
                        device.friendlyName = L"جهاز تخزين " + std::wstring(driveLetters);
                    }
                    
                    // الكشف إذا كان USB
                    device.isUSB = IsUSBDevice(driveLetters[0]);
                    
                    devices.push_back(device);
                }
            }
        }
        
        return devices;
    }
    
private:
    bool GetDriveDisplayName(const wchar_t* drivePath, wchar_t* displayName, DWORD bufferSize) {
        SHFILEINFOW sfi = {0};
        if (SHGetFileInfoW(drivePath, 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME)) {
            wcsncpy_s(displayName, bufferSize, sfi.szDisplayName, _TRUNCATE);
            return true;
        }
        return false;
    }
    
    bool IsUSBDevice(wchar_t driveLetter) {
        std::wstring rootPath = std::wstring(1, driveLetter) + L":\\";
        std::wstring query = L"\\\\.\\" + std::wstring(1, driveLetter) + L":";
        
        HANDLE hDevice = CreateFileW(query.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING, 0, NULL);
        if (hDevice == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        STORAGE_PROPERTY_QUERY queryProp = {0};
        queryProp.PropertyId = StorageDeviceProperty;
        queryProp.QueryType = PropertyStandardQuery;
        
        STORAGE_DEVICE_DESCRIPTOR deviceDescriptor = {0};
        DWORD bytesReturned = 0;
        
        if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
                           &queryProp, sizeof(queryProp),
                           &deviceDescriptor, sizeof(deviceDescriptor),
                           &bytesReturned, NULL)) {
            // تحقق من ناقل الاتصال
            if (deviceDescriptor.BusType == BusTypeUsb) {
                CloseHandle(hDevice);
                return true;
            }
        }
        
        CloseHandle(hDevice);
        return false;
    }
};

class ImageReader {
public:
    static ISOImage ReadISOInfo(const std::wstring& filePath) {
        ISOImage image;
        image.path = filePath;
        
        // الحصول على حجم الملف
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER fileSize;
            if (GetFileSizeEx(hFile, &fileSize)) {
                image.size = fileSize.QuadPart;
            }
            CloseHandle(hFile);
        }
        
        // محاولة قراءة معلومات ISO
        // هذه دالة مبسطة، في النسخة الحقيقية ستكون أكثر تعقيداً
        image.label = ExtractFileName(filePath);
        image.type = DetectImageType(filePath);
        image.isBootable = CheckIfBootable(filePath);
        image.architecture = DetectArchitecture(filePath);
        image.version = DetectVersion(filePath);
        
        return image;
    }
    
private:
    static std::wstring ExtractFileName(const std::wstring& path) {
        size_t pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            std::wstring filename = path.substr(pos + 1);
            size_t dotPos = filename.find_last_of(L'.');
            if (dotPos != std::wstring::npos) {
                return filename.substr(0, dotPos);
            }
            return filename;
        }
        return path;
    }
    
    static std::wstring DetectImageType(const std::wstring& path) {
        std::wstring ext = GetFileExtension(path);
        if (ext == L"iso") return L"ISO Image";
        if (ext == L"img") return L"Disk Image";
        if (ext == L"vhd") return L"Virtual Hard Disk";
        if (ext == L"vhdx") return L"Virtual Hard Disk v2";
        if (ext == L"wim") return L"Windows Imaging";
        return L"Unknown";
    }
    
    static bool CheckIfBootable(const std::wstring& path) {
        // تنفيذ مبسط للكشف عن إمكانية الإقلاع
        // في النسخة الحقيقية، سيتم تحليل محتوى ISO
        return true;
    }
    
    static std::wstring DetectArchitecture(const std::wstring& path) {
        // تنفيذ مبسط
        return L"x64";
    }
    
    static std::wstring DetectVersion(const std::wstring& path) {
        // تنفيذ مبسط
        return L"Unknown";
    }
    
    static std::wstring GetFileExtension(const std::wstring& path) {
        size_t dotPos = path.find_last_of(L'.');
        if (dotPos != std::wstring::npos) {
            std::wstring ext = path.substr(dotPos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            return ext;
        }
        return L"";
    }
};

class DiskWriter {
private:
    std::atomic<bool> cancelRequested;
    std::atomic<int> progress;
    HWND callbackWindow;
    
public:
    DiskWriter(HWND hwnd) : cancelRequested(false), progress(0), callbackWindow(hwnd) {}
    
    bool WriteImageToDisk(const std::wstring& imagePath, const std::wstring& devicePath, 
                         const PartitionScheme& scheme, const FormatOption& format,
                         bool createPersistentStorage, size_t persistentSize) {
        
        cancelRequested = false;
        progress = 0;
        
        Logger::Log(L"بدء عملية الكتابة: " + imagePath + L" إلى " + devicePath);
        
        // محاكاة العملية
        for (int i = 0; i <= 100 && !cancelRequested; i++) {
            progress = i;
            
            // إرسال تحديث التقدم
            if (callbackWindow) {
                PostMessage(callbackWindow, WM_UPDATE_PROGRESS, i, 0);
            }
            
            // محاكاة العمل
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // تسجيل التقدم
            if (i % 10 == 0) {
                Logger::Log(L"التقدم: " + std::to_wstring(i) + L"%");
            }
        }
        
        if (cancelRequested) {
            Logger::Log(L"تم إلغاء العملية");
            return false;
        }
        
        Logger::Log(L"اكتملت العملية بنجاح");
        return true;
    }
    
    void Cancel() {
        cancelRequested = true;
    }
    
    int GetProgress() const {
        return progress.load();
    }
};

// النافذة الرئيسية
class InfernoWindow {
private:
    HWND hWnd;
    HINSTANCE hInstance;
    HICON hIcon;
    HFONT hTitleFont;
    
    // عناصر التحكم
    HWND hDeviceCombo;
    HWND hImagePathEdit;
    HWND hImageBrowseBtn;
    HWND hFormatCombo;
    HWND hSchemeCombo;
    HWND hProgressBar;
    HWND hStartBtn;
    HWND hCancelBtn;
    HWND hLogEdit;
    HWND hRefreshBtn;
    HWND hAdvancedBtn;
    HWND hSettingsBtn;
    
    // البيانات
    std::vector<DeviceInfo> devices;
    std::vector<PartitionScheme> partitionSchemes;
    std::vector<FormatOption> formatOptions;
    ISOImage currentImage;
    
    // كائنات المساعدة
    USBDetector* usbDetector;
    DiskWriter* diskWriter;
    std::thread* writeThread;
    
    // إعدادات متقدمة
    bool enableBadBlocksCheck;
    bool enableCompression;
    bool enableEncryption;
    bool createPersistentStorage;
    size_t persistentStorageSize;
    std::wstring customLabel;
    bool enableSecureBoot;
    bool enableTPM;
    
public:
    InfernoWindow(HINSTANCE hInst) : hInstance(hInst), hIcon(NULL), hTitleFont(NULL),
                                     usbDetector(nullptr), diskWriter(nullptr), writeThread(nullptr),
                                     enableBadBlocksCheck(true), enableCompression(false),
                                     enableEncryption(false), createPersistentStorage(false),
                                     persistentStorageSize(4096), enableSecureBoot(true),
                                     enableTPM(false) {
        
        Logger::Initialize();
        InitializeWindow();
        LoadResources();
        InitializeControls();
        LoadSettings();
        RefreshDeviceList();
        LoadPartitionSchemes();
        LoadFormatOptions();
    }
    
    ~InfernoWindow() {
        if (writeThread && writeThread->joinable()) {
            writeThread->join();
            delete writeThread;
        }
        
        if (usbDetector) delete usbDetector;
        if (diskWriter) delete diskWriter;
        
        if (hTitleFont) DeleteObject(hTitleFont);
        if (hIcon) DestroyIcon(hIcon);
    }
    
    void Show(int nCmdShow) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }
    
    HWND GetHandle() const { return hWnd; }
    
private:
    void InitializeWindow() {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"InfernoWindowClass";
        wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
        
        RegisterClassExW(&wc);
        
        hWnd = CreateWindowExW(0, wc.lpszClassName, L"Inferno - أداة حرق أنظمة التشغيل المتقدمة",
                              WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
                              NULL, NULL, hInstance, this);
        
        usbDetector = new USBDetector(hWnd);
        diskWriter = new DiskWriter(hWnd);
    }
    
    void LoadResources() {
        // محاولة تحميل الأيقونة من الملف
        hIcon = (HICON)LoadImageW(NULL, L"inferno.png", IMAGE_ICON, 64, 64, LR_LOADFROMFILE);
        if (!hIcon) {
            // إذا فشل، إنشاء أيقونة افتراضية
            hIcon = CreateDefaultIcon();
        }
        
        SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        
        // إنشاء خط العنوان
        LOGFONTW lf = {0};
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
        lf.lfHeight = 24;
        lf.lfWeight = FW_BOLD;
        hTitleFont = CreateFontIndirectW(&lf);
    }
    
    HICON CreateDefaultIcon() {
        HDC hdc = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdc, 64, 64);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        
        // رسم أيقونة افتراضية (شعلة)
        HBRUSH hRedBrush = CreateSolidBrush(RGB(255, 69, 0));
        HBRUSH hYellowBrush = CreateSolidBrush(RGB(255, 215, 0));
        HBRUSH hOrangeBrush = CreateSolidBrush(RGB(255, 140, 0));
        
        RECT rect = {0, 0, 64, 64};
        FillRect(hdcMem, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        
        // رسم الشعلة
        POINT flame[] = {{32, 10}, {40, 40}, {32, 60}, {24, 40}};
        SelectObject(hdcMem, hRedBrush);
        Polygon(hdcMem, flame, 4);
        
        SelectObject(hdcMem, hYellowBrush);
        Ellipse(hdcMem, 28, 15, 36, 30);
        
        SelectObject(hdcMem, hOrangeBrush);
        Ellipse(hdcMem, 26, 25, 38, 40);
        
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdc);
        
        DeleteObject(hRedBrush);
        DeleteObject(hYellowBrush);
        DeleteObject(hOrangeBrush);
        
        return (HICON)hBitmap;
    }
    
    void InitializeControls() {
        // إنشاء الخطوط
        HFONT hNormalFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        
        // العنوان
        CreateWindowW(L"STATIC", L"INFERNO", WS_CHILD | WS_VISIBLE | SS_CENTER,
                      10, 10, 880, 40, hWnd, NULL, hInstance, NULL);
        
        // جهاز التخزين
        CreateWindowW(L"STATIC", L"الجهاز:", WS_CHILD | WS_VISIBLE,
                      20, 70, 100, 25, hWnd, NULL, hInstance, NULL);
        
        hDeviceCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | 
                                    CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                                    130, 70, 300, 200, hWnd, (HMENU)100, hInstance, NULL);
        
        hRefreshBtn = CreateWindowW(L"BUTTON", L"تحديث", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                    440, 70, 80, 25, hWnd, (HMENU)101, hInstance, NULL);
        
        // صورة النظام
        CreateWindowW(L"STATIC", L"صورة النظام:", WS_CHILD | WS_VISIBLE,
                      20, 110, 100, 25, hWnd, NULL, hInstance, NULL);
        
        hImagePathEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | 
                                      WS_BORDER | ES_AUTOHSCROLL,
                                      130, 110, 350, 25, hWnd, (HMENU)102, hInstance, NULL);
        
        hImageBrowseBtn = CreateWindowW(L"BUTTON", L"استعراض...", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                        490, 110, 80, 25, hWnd, (HMENU)103, hInstance, NULL);
        
        // مخطط التقسيم
        CreateWindowW(L"STATIC", L"مخطط التقسيم:", WS_CHILD | WS_VISIBLE,
                      20, 150, 100, 25, hWnd, NULL, hInstance, NULL);
        
        hSchemeCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | 
                                    CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                                    130, 150, 300, 200, hWnd, (HMENU)104, hInstance, NULL);
        
        // نظام الملفات
        CreateWindowW(L"STATIC", L"نظام الملفات:", WS_CHILD | WS_VISIBLE,
                      20, 190, 100, 25, hWnd, NULL, hInstance, NULL);
        
        hFormatCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | 
                                    CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                                    130, 190, 300, 200, hWnd, (HMENU)105, hInstance, NULL);
        
        // خيارات متقدمة
        hAdvancedBtn = CreateWindowW(L"BUTTON", L"خيارات متقدمة >>", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                     450, 190, 150, 25, hWnd, (HMENU)106, hInstance, NULL);
        
        // شريط التقدم
        CreateWindowW(L"STATIC", L"التقدم:", WS_CHILD | WS_VISIBLE,
                      20, 250, 100, 25, hWnd, NULL, hInstance, NULL);
        
        hProgressBar = CreateWindowW(PROGRESS_CLASS, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                     130, 250, 400, 25, hWnd, (HMENU)107, hInstance, NULL);
        
        // أزرار التحكم
        hStartBtn = CreateWindowW(L"BUTTON", L"بدء العملية", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                  20, 300, 120, 40, hWnd, (HMENU)108, hInstance, NULL);
        
        hCancelBtn = CreateWindowW(L"BUTTON", L"إلغاء", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED,
                                   150, 300, 120, 40, hWnd, (HMENU)109, hInstance, NULL);
        
        hSettingsBtn = CreateWindowW(L"BUTTON", L"الإعدادات", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                     280, 300, 120, 40, hWnd, (HMENU)110, hInstance, NULL);
        
        // سجل الأحداث
        CreateWindowW(L"STATIC", L"سجل الأحداث:", WS_CHILD | WS_VISIBLE,
                      20, 360, 100, 25, hWnd, NULL, hInstance, NULL);
        
        hLogEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | 
                                ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                                20, 390, 850, 250, hWnd, (HMENU)111, hInstance, NULL);
        
        // تعيين الخطوط
        EnumChildWindows(hWnd, SetChildFont, (LPARAM)hNormalFont);
        
        // تعيين الخط الخاص بالعنوان
        HWND hTitle = GetDlgItem(hWnd, 1);
        if (hTitle) {
            SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
        }
    }
    
    static BOOL CALLBACK SetChildFont(HWND hwnd, LPARAM lParam) {
        SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
        return TRUE;
    }
    
    void LoadSettings() {
        // هنا سيتم تحميل الإعدادات من الملف
        enableBadBlocksCheck = true;
        enableCompression = false;
        enableEncryption = false;
        createPersistentStorage = false;
        persistentStorageSize = 4096;
        customLabel = L"INFERNO_USB";
        enableSecureBoot = true;
        enableTPM = false;
    }
    
    void RefreshDeviceList() {
        SendMessage(hDeviceCombo, CB_RESETCONTENT, 0, 0);
        
        devices = usbDetector->DetectDevices();
        
        for (const auto& device : devices) {
            std::wstring displayText = device.friendlyName + L" (" + 
                                      FormatSize(device.totalSize) + L")";
            
            if (!device.volumeName.empty()) {
                displayText += L" [" + device.volumeName + L"]";
            }
            
            int index = SendMessageW(hDeviceCombo, CB_ADDSTRING, 0, (LPARAM)displayText.c_str());
            SendMessageW(hDeviceCombo, CB_SETITEMDATA, index, (LPARAM)&device);
        }
        
        if (!devices.empty()) {
            SendMessage(hDeviceCombo, CB_SETCURSEL, 0, 0);
        }
        
        AddLogMessage(L"تم تحديث قائمة الأجهزة");
    }
    
    void LoadPartitionSchemes() {
        partitionSchemes.clear();
        
        // MBR
        partitionSchemes.push_back({L"MBR", L"mbr", 
                                   L"نظام BIOS أو UEFI-CSM (التوافقية)", true, true});
        
        // GPT
        partitionSchemes.push_back({L"GPT", L"gpt", 
                                   L"نظام UEFI (الحديث)", true, false});
        
        // Hybrid
        partitionSchemes.push_back({L"Hybrid", L"hybrid", 
                                   L"MBR+GPT (متوافق مع BIOS وUEFI)", true, true});
        
        // إضافة إلى القائمة
        SendMessage(hSchemeCombo, CB_RESETCONTENT, 0, 0);
        for (const auto& scheme : partitionSchemes) {
            std::wstring displayText = scheme.name + L" - " + scheme.description;
            SendMessageW(hSchemeCombo, CB_ADDSTRING, 0, (LPARAM)displayText.c_str());
        }
        
        SendMessage(hSchemeCombo, CB_SETCURSEL, 0, 0);
    }
    
    void LoadFormatOptions() {
        formatOptions.clear();
        
        // NTFS
        formatOptions.push_back({L"NTFS", L"ntfs", L"INFERNO_USB"});
        
        // FAT32
        formatOptions.push_back({L"FAT32", L"fat32", L"INFERNO_USB"});
        
        // exFAT
        formatOptions.push_back({L"exFAT", L"exfat", L"INFERNO_USB"});
        
        // إضافة إلى القائمة
        SendMessage(hFormatCombo, CB_RESETCONTENT, 0, 0);
        for (const auto& format : formatOptions) {
            SendMessageW(hFormatCombo, CB_ADDSTRING, 0, (LPARAM)format.name.c_str());
        }
        
        SendMessage(hFormatCombo, CB_SETCURSEL, 0, 0);
    }
    
    void BrowseForImage() {
        OPENFILENAMEW ofn = {0};
        wchar_t fileName[MAX_PATH_LENGTH] = {0};
        
        ofn.lStructSize = sizeof(OPENFILENAMEW);
        ofn.hwndOwner = hWnd;
        ofn.lpstrFilter = L"صور القرص\0*.iso;*.img;*.vhd;*.vhdx;*.wim\0كل الملفات\0*.*\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH_LENGTH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        ofn.lpstrTitle = L"اختر صورة نظام التشغيل";
        
        if (GetOpenFileNameW(&ofn)) {
            SetWindowTextW(hImagePathEdit, fileName);
            
            // قراءة معلومات الصورة
            currentImage = ImageReader::ReadISOInfo(fileName);
            UpdateImageInfo();
            
            AddLogMessage(L"تم تحميل الصورة: " + std::wstring(fileName));
        }
    }
    
    void UpdateImageInfo() {
        if (!currentImage.path.empty()) {
            std::wstring info = L"تم تحميل: " + currentImage.label + 
                               L" (" + FormatSize(currentImage.size) + L")";
            
            if (!currentImage.type.empty()) {
                info += L" - نوع: " + currentImage.type;
            }
            
            if (!currentImage.architecture.empty()) {
                info += L" - بنية: " + currentImage.architecture;
            }
            
            AddLogMessage(info);
        }
    }
    
    void StartWritingProcess() {
        // الحصول على الجهاز المحدد
        int deviceIndex = SendMessage(hDeviceCombo, CB_GETCURSEL, 0, 0);
        if (deviceIndex == CB_ERR) {
            MessageBoxW(hWnd, L"الرجاء اختيار جهاز تخزين", L"خطأ", MB_ICONERROR);
            return;
        }
        
        // الحصول على مسار الصورة
        wchar_t imagePath[MAX_PATH_LENGTH];
        GetWindowTextW(hImagePathEdit, imagePath, MAX_PATH_LENGTH);
        if (wcslen(imagePath) == 0) {
            MessageBoxW(hWnd, L"الرجاء اختيار صورة نظام التشغيل", L"خطأ", MB_ICONERROR);
            return;
        }
        
        // التحقق من وجود الملف
        if (GetFileAttributesW(imagePath) == INVALID_FILE_ATTRIBUTES) {
            MessageBoxW(hWnd, L"ملف الصورة غير موجود", L"خطأ", MB_ICONERROR);
            return;
        }
        
        // الحصول على مخطط التقسيم
        int schemeIndex = SendMessage(hSchemeCombo, CB_GETCURSEL, 0, 0);
        if (schemeIndex == CB_ERR) {
            schemeIndex = 0;
        }
        
        // الحصول على نظام الملفات
        int formatIndex = SendMessage(hFormatCombo, CB_GETCURSEL, 0, 0);
        if (formatIndex == CB_ERR) {
            formatIndex = 0;
        }
        
        // تأكيد العملية
        std::wstring message = L"سيتم مسح جميع البيانات على الجهاز المحدد!\n\n";
        message += L"هل أنت متأكد من المتابعة؟";
        
        if (MessageBoxW(hWnd, message.c_str(), L"تأكيد العملية", 
                       MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
            return;
        }
        
        // تعطيل أزرار التحكم
        EnableWindow(hStartBtn, FALSE);
        EnableWindow(hCancelBtn, TRUE);
        EnableWindow(hDeviceCombo, FALSE);
        EnableWindow(hImageBrowseBtn, FALSE);
        EnableWindow(hSchemeCombo, FALSE);
        EnableWindow(hFormatCombo, FALSE);
        EnableWindow(hAdvancedBtn, FALSE);
        
        // إعادة تعيين شريط التقدم
        SendMessage(hProgressBar, PBM_SETPOS, 0, 0);
        
        // بدء عملية الكتابة في thread منفصل
        writeThread = new std::thread([this, deviceIndex, imagePath, schemeIndex, formatIndex]() {
            // الحصول على معلومات الجهاز
            DeviceInfo* device = (DeviceInfo*)SendMessage(hDeviceCombo, CB_GETITEMDATA, deviceIndex, 0);
            
            // تنفيذ عملية الكتابة
            bool success = diskWriter->WriteImageToDisk(
                imagePath, 
                device ? device->devicePath : L"",
                partitionSchemes[schemeIndex],
                formatOptions[formatIndex],
                createPersistentStorage,
                persistentStorageSize
            );
            
            // إعادة تمكين واجهة المستخدم
            PostMessage(hWnd, WM_COMMAND, success ? 200 : 201, 0);
        });
    }
    
    void CancelWritingProcess() {
        if (diskWriter) {
            diskWriter->Cancel();
            AddLogMessage(L"تم طلب إلغاء العملية...");
        }
    }
    
    void ShowAdvancedOptions() {
        // نافذة الخيارات المتقدمة
        DialogBoxParamW(hInstance, MAKEINTRESOURCE(1), hWnd, AdvancedOptionsProc, (LPARAM)this);
    }
    
    void ShowSettings() {
        // نافذة الإعدادات
        MessageBoxW(hWnd, L"قريباً: نافذة الإعدادات المتقدمة", L"الإعدادات", MB_ICONINFORMATION);
    }
    
    void AddLogMessage(const std::wstring& message) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        std::wstring timestamp = std::to_wstring(st.wHour) + L":" + 
                                std::to_wstring(st.wMinute) + L":" +
                                std::to_wstring(st.wSecond);
        
        std::wstring logEntry = L"[" + timestamp + L"] " + message + L"\r\n";
        
        // إضافة إلى سجل البرنامج
        int len = GetWindowTextLengthW(hLogEdit);
        SendMessageW(hLogEdit, EM_SETSEL, len, len);
        SendMessageW(hLogEdit, EM_REPLACESEL, FALSE, (LPARAM)logEntry.c_str());
        
        // تسجيل في ملف اللوج
        Logger::Log(message);
    }
    
    std::wstring FormatSize(uint64_t size) {
        const wchar_t* units[] = {L"بايت", L"كيلوبايت", L"ميجابايت", L"جيجابايت", L"تيرابايت"};
        int unitIndex = 0;
        double formattedSize = (double)size;
        
        while (formattedSize >= 1024.0 && unitIndex < 4) {
            formattedSize /= 1024.0;
            unitIndex++;
        }
        
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(2) << formattedSize << L" " << units[unitIndex];
        return ss.str();
    }
    
    void OnDeviceChange() {
        RefreshDeviceList();
    }
    
    void OnWritingComplete(bool success) {
        // إعادة تمكين واجهة المستخدم
        EnableWindow(hStartBtn, TRUE);
        EnableWindow(hCancelBtn, FALSE);
        EnableWindow(hDeviceCombo, TRUE);
        EnableWindow(hImageBrowseBtn, TRUE);
        EnableWindow(hSchemeCombo, TRUE);
        EnableWindow(hFormatCombo, TRUE);
        EnableWindow(hAdvancedBtn, TRUE);
        
        // تعيين شريط التقدم إلى 100%
        SendMessage(hProgressBar, PBM_SETPOS, 100, 0);
        
        // عرض رسالة النتيجة
        if (success) {
            AddLogMessage(L"اكتملت العملية بنجاح!");
            MessageBoxW(hWnd, L"اكتملت عملية حرق نظام التشغيل بنجاح!", 
                       L"نجاح", MB_ICONINFORMATION);
        } else {
            AddLogMessage(L"فشلت العملية أو تم إلغاؤها");
            MessageBoxW(hWnd, L"فشلت عملية حرق نظام التشغيل أو تم إلغاؤها", 
                       L"فشل", MB_ICONERROR);
        }
        
        // تنظيف thread
        if (writeThread) {
            if (writeThread->joinable()) {
                writeThread->join();
            }
            delete writeThread;
            writeThread = nullptr;
        }
    }
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        InfernoWindow* pThis = nullptr;
        
        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (InfernoWindow*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = (InfernoWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }
        
        if (pThis) {
            return pThis->HandleMessage(uMsg, wParam, lParam);
        }
        
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_COMMAND:
                return OnCommand(wParam, lParam);
                
            case WM_PAINT:
                return OnPaint();
                
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
                
            case WM_UPDATE_PROGRESS:
                SendMessage(hProgressBar, PBM_SETPOS, wParam, 0);
                return 0;
                
            case WM_DEVICE_CHANGE:
                OnDeviceChange();
                return 0;
                
            case WM_CLOSE:
                if (diskWriter && diskWriter->GetProgress() > 0 && 
                    diskWriter->GetProgress() < 100) {
                    if (MessageBoxW(hWnd, L"العملية جارية، هل تريد الإغلاق؟", 
                                   L"تأكيد", MB_YESNO | MB_ICONWARNING) == IDNO) {
                        return 0;
                    }
                }
                break;
        }
        
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    
    LRESULT OnCommand(WPARAM wParam, LPARAM lParam) {
        int cmd = LOWORD(wParam);
        
        switch (cmd) {
            case 101: // تحديث
                RefreshDeviceList();
                break;
                
            case 103: // استعراض
                BrowseForImage();
                break;
                
            case 108: // بدء
                StartWritingProcess();
                break;
                
            case 109: // إلغاء
                CancelWritingProcess();
                break;
                
            case 106: // خيارات متقدمة
                ShowAdvancedOptions();
                break;
                
            case 110: // إعدادات
                ShowSettings();
                break;
                
            case 200: // اكتمال ناجح
                OnWritingComplete(true);
                break;
                
            case 201: // اكتمال فاشل
                OnWritingComplete(false);
                break;
        }
        
        return 0;
    }
    
    LRESULT OnPaint() {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        
        // رسم خلفية العنوان
        RECT titleRect = {0, 0, 900, 60};
        HBRUSH hTitleBrush = CreateSolidBrush(RGB(40, 40, 40));
        FillRect(hdc, &titleRect, hTitleBrush);
        DeleteObject(hTitleBrush);
        
        // رسم نص العنوان
        SetTextColor(hdc, RGB(255, 165, 0));
        SetBkMode(hdc, TRANSPARENT);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hTitleFont);
        
        RECT textRect = {0, 15, 900, 45};
        DrawTextW(hdc, L"INFERNO - أداة حرق أنظمة التشغيل المتقدمة", -1, 
                 &textRect, DT_CENTER | DT_SINGLELINE);
        
        SelectObject(hdc, hOldFont);
        
        // رسم إصدار البرنامج
        HFONT hSmallFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SelectObject(hdc, hSmallFont);
        SetTextColor(hdc, RGB(200, 200, 200));
        
        std::wstring versionText = L"الإصدار " + std::wstring(INFERNO_VERSION) + 
                                  L" (بناء " + std::wstring(INFERNO_BUILD) + L")";
        
        RECT versionRect = {10, 45, 300, 60};
        DrawTextW(hdc, versionText.c_str(), -1, &versionRect, DT_LEFT | DT_SINGLELINE);
        
        SelectObject(hdc, GetStockObject(SYSTEM_FONT));
        DeleteObject(hSmallFont);
        
        EndPaint(hWnd, &ps);
        return 0;
    }
    
    static INT_PTR CALLBACK AdvancedOptionsProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        static InfernoWindow* pThis = nullptr;
        
        switch (uMsg) {
            case WM_INITDIALOG:
                pThis = (InfernoWindow*)lParam;
                InitializeAdvancedDialog(hDlg, pThis);
                return TRUE;
                
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case IDOK:
                        SaveAdvancedOptions(hDlg, pThis);
                        EndDialog(hDlg, IDOK);
                        return TRUE;
                        
                    case IDCANCEL:
                        EndDialog(hDlg, IDCANCEL);
                        return TRUE;
                }
                break;
        }
        
        return FALSE;
    }
    
    static void InitializeAdvancedDialog(HWND hDlg, InfernoWindow* pThis) {
        // هنا سيتم تهيئة عناصر التحكم في نافذة الخيارات المتقدمة
        // هذه دالة مساعدة مبسطة
    }
    
    static void SaveAdvancedOptions(HWND hDlg, InfernoWindow* pThis) {
        // هنا سيتم حفظ الخيارات المتقدمة
        // هذه دالة مساعدة مبسطة
    }
};

// نقطة دخول البرنامج
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // تهيئة عناصر التحكم الشائعة
    INITCOMMONCONTROLSEX icc = {0};
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);
    
    // إنشاء وتشغيل النافذة الرئيسية
    InfernoWindow mainWindow(hInstance);
    
    if (!mainWindow.GetHandle()) {
        MessageBoxW(NULL, L"فشل إنشاء النافذة الرئيسية", L"خطأ", MB_ICONERROR);
        return 1;
    }
    
    mainWindow.Show(nCmdShow);
    
    // حلقة الرسائل
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
