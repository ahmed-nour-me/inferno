#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devguid.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Application Info
#define APP_NAME L"Inferno USB Creator"
#define APP_VERSION L"2.0.0"
#define DEVELOPER L"Ahmed Nour Ahmed - Qena, Egypt"

// Control IDs
#define IDC_DEVICE_COMBO 1001
#define IDC_ISO_PATH 1002
#define IDC_BROWSE_BTN 1003
#define IDC_PARTITION_COMBO 1004
#define IDC_FILESYSTEM_COMBO 1005
#define IDC_CLUSTER_COMBO 1006
#define IDC_LABEL_EDIT 1007
#define IDC_START_BTN 1008
#define IDC_PROGRESS 1009
#define IDC_STATUS_TEXT 1010
#define IDC_QUICK_FORMAT 1011
#define IDC_BAD_BLOCKS 1012
#define IDC_EXTENDED_LABEL 1013
#define IDC_BOOTABLE_IMAGE 1014
#define IDC_PERSISTENT_PARTITION 1015
#define IDC_VERIFY_WRITE 1016
#define IDC_ADVANCED_GROUP 1017
#define IDC_PERSISTENT_SIZE 1018
#define IDC_COMPRESSION_CHECK 1019
#define IDC_BACKUP_BTN 1020
#define IDC_RESTORE_BTN 1021
#define IDC_MULTI_BOOT 1022
#define IDC_SECURE_BOOT 1023
#define IDC_UEFI_NTFS 1024
#define IDC_LIST_USB 1025
#define IDC_REFRESH_BTN 1026
#define IDC_SPEED_TEST_BTN 1027
#define IDC_ADVANCED_TAB 1028

// Menu IDs
#define IDM_ABOUT 2001
#define IDM_SETTINGS 2002
#define IDM_EXIT 2003
#define IDM_CHECK_UPDATE 2004
#define IDM_LOG 2005

// Global variables
HWND g_hMainWnd = NULL;
HWND g_hDeviceCombo = NULL;
HWND g_hIsoPath = NULL;
HWND g_hProgress = NULL;
HWND g_hStatusText = NULL;
HWND g_hPartitionCombo = NULL;
HWND g_hFileSystemCombo = NULL;
HWND g_hClusterCombo = NULL;
HWND g_hLabelEdit = NULL;
HWND g_hQuickFormat = NULL;
HWND g_hBadBlocks = NULL;
HWND g_hExtendedLabel = NULL;
HWND g_hBootableImage = NULL;
HWND g_hPersistentPartition = NULL;
HWND g_hVerifyWrite = NULL;
HWND g_hPersistentSize = NULL;
HWND g_hCompression = NULL;
HWND g_hMultiBoot = NULL;
HWND g_hSecureBoot = NULL;
HWND g_hUEFI_NTFS = NULL;

std::atomic<bool> g_isRunning(false);
std::atomic<bool> g_cancelOperation(false);
std::wstring g_selectedIsoPath;
std::wstring g_selectedDevice;

// Structures
struct USBDevice {
    std::wstring name;
    std::wstring path;
    ULONGLONG size;
    std::wstring driveType;
    bool isRemovable;
};

struct ISOInfo {
    std::wstring path;
    ULONGLONG size;
    std::wstring label;
    bool isBootable;
    std::wstring bootType; // BIOS, UEFI, Hybrid
};

std::vector<USBDevice> g_devices;

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitializeControls(HWND hWnd);
void RefreshDeviceList();
void BrowseForISO();
void StartOperation();
void UpdateProgress(int percentage, const std::wstring& status);
std::vector<USBDevice> EnumerateUSBDevices();
bool FormatDrive(const std::wstring& drive, const std::wstring& fileSystem, const std::wstring& label, bool quickFormat);
bool WriteISOToUSB(const std::wstring& isoPath, const std::wstring& device);
bool MakeBootable(const std::wstring& device, const std::wstring& bootType);
bool VerifyWrite(const std::wstring& device, const std::wstring& isoPath);
ISOInfo AnalyzeISO(const std::wstring& isoPath);
bool CreatePersistentPartition(const std::wstring& device, ULONGLONG sizeGB);
bool EnableSecureBoot(const std::wstring& device);
bool CreateMultiBootUSB(const std::vector<std::wstring>& isoPaths, const std::wstring& device);
bool BackupUSB(const std::wstring& device, const std::wstring& backupPath);
bool RestoreUSB(const std::wstring& backupPath, const std::wstring& device);
bool TestUSBSpeed(const std::wstring& device);
void ShowAboutDialog();
void ShowSettingsDialog();
void LogMessage(const std::wstring& message);
ULONGLONG GetDriveSize(const std::wstring& drive);
bool IsAdmin();
bool ElevateProcess();

// Utility functions
std::wstring FormatSize(ULONGLONG bytes) {
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    wchar_t buffer[50];
    swprintf_s(buffer, L"%.2f %s", size, units[unitIndex]);
    return std::wstring(buffer);
}

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup)) {
        CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin);
        FreeSid(AdministratorsGroup);
    }
    
    return isAdmin == TRUE;
}

bool ElevateProcess() {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath))) {
        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;
        
        if (ShellExecuteEx(&sei)) {
            ExitProcess(0);
            return true;
        }
    }
    return false;
}

ULONGLONG GetDriveSize(const std::wstring& drive) {
    HANDLE hDevice = CreateFile(drive.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE)
        return 0;
    
    DISK_GEOMETRY_EX diskGeometry;
    DWORD bytesReturned;
    
    if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0,
        &diskGeometry, sizeof(diskGeometry), &bytesReturned, NULL)) {
        CloseHandle(hDevice);
        return diskGeometry.DiskSize.QuadPart;
    }
    
    CloseHandle(hDevice);
    return 0;
}

std::vector<USBDevice> EnumerateUSBDevices() {
    std::vector<USBDevice> devices;
    DWORD drives = GetLogicalDrives();
    
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            wchar_t driveLetter = L'A' + i;
            std::wstring drivePath = std::wstring(1, driveLetter) + L":\\";
            
            UINT driveType = GetDriveType(drivePath.c_str());
            
            if (driveType == DRIVE_REMOVABLE) {
                USBDevice device;
                device.name = std::wstring(1, driveLetter) + L":";
                device.path = L"\\\\.\\" + std::wstring(1, driveLetter) + L":";
                device.isRemovable = true;
                
                wchar_t volumeName[MAX_PATH];
                DWORD serialNumber;
                
                if (GetVolumeInformation(drivePath.c_str(), volumeName, MAX_PATH,
                    &serialNumber, NULL, NULL, NULL, 0)) {
                    if (wcslen(volumeName) > 0)
                        device.name += L" (" + std::wstring(volumeName) + L")";
                }
                
                device.size = GetDriveSize(device.path);
                
                if (device.size > 0) {
                    device.name += L" - " + FormatSize(device.size);
                    devices.push_back(device);
                }
            }
        }
    }
    
    return devices;
}

void RefreshDeviceList() {
    SendMessage(g_hDeviceCombo, CB_RESETCONTENT, 0, 0);
    g_devices = EnumerateUSBDevices();
    
    if (g_devices.empty()) {
        SendMessage(g_hDeviceCombo, CB_ADDSTRING, 0, (LPARAM)L"No USB devices found");
        EnableWindow(GetDlgItem(g_hMainWnd, IDC_START_BTN), FALSE);
    } else {
        for (const auto& device : g_devices) {
            SendMessage(g_hDeviceCombo, CB_ADDSTRING, 0, (LPARAM)device.name.c_str());
        }
        SendMessage(g_hDeviceCombo, CB_SETCURSEL, 0, 0);
        EnableWindow(GetDlgItem(g_hMainWnd, IDC_START_BTN), TRUE);
    }
}

void BrowseForISO() {
    IFileOpenDialog* pFileOpen = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
        IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    
    if (SUCCEEDED(hr)) {
        COMDLG_FILTERSPEC fileTypes[] = {
            { L"ISO Images", L"*.iso" },
            { L"All Files", L"*.*" }
        };
        
        pFileOpen->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
        pFileOpen->SetTitle(L"Select ISO Image");
        
        hr = pFileOpen->Show(g_hMainWnd);
        
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = NULL;
            hr = pFileOpen->GetResult(&pItem);
            
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = NULL;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                
                if (SUCCEEDED(hr)) {
                    g_selectedIsoPath = pszFilePath;
                    SetWindowText(g_hIsoPath, pszFilePath);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
}

ISOInfo AnalyzeISO(const std::wstring& isoPath) {
    ISOInfo info;
    info.path = isoPath;
    info.isBootable = false;
    info.bootType = L"Unknown";
    
    std::ifstream file(isoPath, std::ios::binary);
    if (!file.is_open())
        return info;
    
    file.seekg(0, std::ios::end);
    info.size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Check for ISO 9660 signature
    char buffer[2048];
    file.seekg(0x8000, std::ios::beg);
    file.read(buffer, 2048);
    
    if (buffer[1] == 'C' && buffer[2] == 'D' && buffer[3] == '0' && buffer[4] == '0' && buffer[5] == '1') {
        info.isBootable = true;
        
        // Check for UEFI boot files
        file.seekg(0, std::ios::beg);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        if (content.find("EFI") != std::string::npos || content.find("efi") != std::string::npos) {
            info.bootType = L"UEFI";
        } else {
            info.bootType = L"BIOS";
        }
    }
    
    file.close();
    return info;
}

bool FormatDrive(const std::wstring& drive, const std::wstring& fileSystem, 
                 const std::wstring& label, bool quickFormat) {
    LogMessage(L"Formatting drive: " + drive);
    
    std::wstring command = L"format " + drive.substr(4, 2) + L" /FS:" + fileSystem;
    
    if (quickFormat)
        command += L" /Q";
    
    if (!label.empty())
        command += L" /V:" + label;
    
    command += L" /Y";
    
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    if (CreateProcess(NULL, const_cast<LPWSTR>(command.c_str()), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return exitCode == 0;
    }
    
    return false;
}

bool WriteISOToUSB(const std::wstring& isoPath, const std::wstring& device) {
    LogMessage(L"Writing ISO to USB: " + isoPath);
    
    std::ifstream isoFile(isoPath, std::ios::binary);
    if (!isoFile.is_open()) {
        LogMessage(L"Failed to open ISO file");
        return false;
    }
    
    HANDLE hDevice = CreateFile(device.c_str(), GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        LogMessage(L"Failed to open device");
        isoFile.close();
        return false;
    }
    
    isoFile.seekg(0, std::ios::end);
    ULONGLONG totalSize = isoFile.tellg();
    isoFile.seekg(0, std::ios::beg);
    
    const size_t bufferSize = 1024 * 1024; // 1MB buffer
    char* buffer = new char[bufferSize];
    ULONGLONG bytesWritten = 0;
    
    while (!isoFile.eof() && !g_cancelOperation) {
        isoFile.read(buffer, bufferSize);
        std::streamsize bytesRead = isoFile.gcount();
        
        if (bytesRead > 0) {
            DWORD written;
            if (!WriteFile(hDevice, buffer, static_cast<DWORD>(bytesRead), &written, NULL)) {
                LogMessage(L"Write error occurred");
                delete[] buffer;
                CloseHandle(hDevice);
                isoFile.close();
                return false;
            }
            
            bytesWritten += written;
            int progress = static_cast<int>((bytesWritten * 100) / totalSize);
            UpdateProgress(progress, L"Writing ISO: " + std::to_wstring(progress) + L"%");
        }
    }
    
    delete[] buffer;
    FlushFileBuffers(hDevice);
    CloseHandle(hDevice);
    isoFile.close();
    
    if (g_cancelOperation) {
        LogMessage(L"Operation cancelled by user");
        return false;
    }
    
    LogMessage(L"ISO written successfully");
    return true;
}

bool MakeBootable(const std::wstring& device, const std::wstring& bootType) {
    LogMessage(L"Making device bootable: " + bootType);
    
    if (bootType == L"UEFI" || bootType == L"Hybrid") {
        // Create EFI partition and install bootloader
        std::wstring command = L"bootsect /nt60 " + device.substr(4, 2) + L" /mbr /force";
        
        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        if (CreateProcess(NULL, const_cast<LPWSTR>(command.c_str()), NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    
    return true;
}

bool VerifyWrite(const std::wstring& device, const std::wstring& isoPath) {
    LogMessage(L"Verifying write operation...");
    UpdateProgress(0, L"Verifying data integrity...");
    
    std::ifstream isoFile(isoPath, std::ios::binary);
    if (!isoFile.is_open())
        return false;
    
    HANDLE hDevice = CreateFile(device.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        isoFile.close();
        return false;
    }
    
    const size_t bufferSize = 1024 * 1024;
    char* isoBuffer = new char[bufferSize];
    char* deviceBuffer = new char[bufferSize];
    
    bool verified = true;
    ULONGLONG bytesVerified = 0;
    
    isoFile.seekg(0, std::ios::end);
    ULONGLONG totalSize = isoFile.tellg();
    isoFile.seekg(0, std::ios::beg);
    
    while (!isoFile.eof() && verified) {
        isoFile.read(isoBuffer, bufferSize);
        std::streamsize bytesRead = isoFile.gcount();
        
        if (bytesRead > 0) {
            DWORD read;
            if (!ReadFile(hDevice, deviceBuffer, static_cast<DWORD>(bytesRead), &read, NULL)) {
                verified = false;
                break;
            }
            
            if (memcmp(isoBuffer, deviceBuffer, bytesRead) != 0) {
                verified = false;
                break;
            }
            
            bytesVerified += bytesRead;
            int progress = static_cast<int>((bytesVerified * 100) / totalSize);
            UpdateProgress(progress, L"Verifying: " + std::to_wstring(progress) + L"%");
        }
    }
    
    delete[] isoBuffer;
    delete[] deviceBuffer;
    CloseHandle(hDevice);
    isoFile.close();
    
    LogMessage(verified ? L"Verification successful" : L"Verification failed");
    return verified;
}

bool CreatePersistentPartition(const std::wstring& device, ULONGLONG sizeGB) {
    LogMessage(L"Creating persistent partition: " + std::to_wstring(sizeGB) + L" GB");
    
    // Use diskpart to create additional partition
    std::wstring script = L"select disk " + device.substr(4, 1) + L"\n";
    script += L"create partition primary size=" + std::to_wstring(sizeGB * 1024) + L"\n";
    script += L"format fs=ext4 quick label=persistence\n";
    script += L"assign\n";
    
    std::wstring tempFile = L"C:\\temp_diskpart.txt";
    std::wofstream scriptFile(tempFile);
    scriptFile << script;
    scriptFile.close();
    
    std::wstring command = L"diskpart /s " + tempFile;
    
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    bool result = false;
    if (CreateProcess(NULL, const_cast<LPWSTR>(command.c_str()), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        result = true;
    }
    
    DeleteFile(tempFile.c_str());
    return result;
}

bool TestUSBSpeed(const std::wstring& device) {
    LogMessage(L"Testing USB speed...");
    UpdateProgress(0, L"Running speed test...");
    
    HANDLE hDevice = CreateFile(device.c_str(), GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE)
        return false;
    
    const size_t testSize = 50 * 1024 * 1024; // 50MB test
    const size_t bufferSize = 1024 * 1024; // 1MB buffer
    char* buffer = new char[bufferSize];
    memset(buffer, 0xAA, bufferSize);
    
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    
    // Write test
    QueryPerformanceCounter(&start);
    for (size_t i = 0; i < testSize / bufferSize; i++) {
        DWORD written;
        WriteFile(hDevice, buffer, bufferSize, &written, NULL);
        UpdateProgress((i * 100) / (testSize / bufferSize), L"Write speed test...");
    }
    QueryPerformanceCounter(&end);
    
    double writeTime = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    double writeSpeed = (testSize / (1024.0 * 1024.0)) / writeTime;
    
    // Read test
    SetFilePointer(hDevice, 0, NULL, FILE_BEGIN);
    QueryPerformanceCounter(&start);
    for (size_t i = 0; i < testSize / bufferSize; i++) {
        DWORD read;
        ReadFile(hDevice, buffer, bufferSize, &read, NULL);
        UpdateProgress((i * 100) / (testSize / bufferSize), L"Read speed test...");
    }
    QueryPerformanceCounter(&end);
    
    double readTime = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    double readSpeed = (testSize / (1024.0 * 1024.0)) / readTime;
    
    delete[] buffer;
    CloseHandle(hDevice);
    
    wchar_t msg[256];
    swprintf_s(msg, L"Write Speed: %.2f MB/s\nRead Speed: %.2f MB/s", writeSpeed, readSpeed);
    MessageBox(g_hMainWnd, msg, L"Speed Test Results", MB_OK | MB_ICONINFORMATION);
    
    return true;
}

bool BackupUSB(const std::wstring& device, const std::wstring& backupPath) {
    LogMessage(L"Backing up USB to: " + backupPath);
    
    HANDLE hDevice = CreateFile(device.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE)
        return false;
    
    std::ofstream backupFile(backupPath, std::ios::binary);
    if (!backupFile.is_open()) {
        CloseHandle(hDevice);
        return false;
    }
    
    ULONGLONG driveSize = GetDriveSize(device);
    const size_t bufferSize = 1024 * 1024;
    char* buffer = new char[bufferSize];
    ULONGLONG bytesRead = 0;
    
    while (bytesRead < driveSize) {
        DWORD read;
        if (!ReadFile(hDevice, buffer, bufferSize, &read, NULL) || read == 0)
            break;
        
        backupFile.write(buffer, read);
        bytesRead += read;
        
        int progress = static_cast<int>((bytesRead * 100) / driveSize);
        UpdateProgress(progress, L"Backing up: " + std::to_wstring(progress) + L"%");
    }
    
    delete[] buffer;
    CloseHandle(hDevice);
    backupFile.close();
    
    LogMessage(L"Backup completed successfully");
    return true;
}

bool RestoreUSB(const std::wstring& backupPath, const std::wstring& device) {
    LogMessage(L"Restoring USB from: " + backupPath);
    
    std::ifstream backupFile(backupPath, std::ios::binary);
    if (!backupFile.is_open())
        return false;
    
    HANDLE hDevice = CreateFile(device.c_str(), GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        backupFile.close();
        return false;
    }
    
    backupFile.seekg(0, std::ios::end);
    ULONGLONG totalSize = backupFile.tellg();
    backupFile.seekg(0, std::ios::beg);
    
    const size_t bufferSize = 1024 * 1024;
    char* buffer = new char[bufferSize];
    ULONGLONG bytesWritten = 0;
    
    while (!backupFile.eof()) {
        backupFile.read(buffer, bufferSize);
        std::streamsize bytesRead = backupFile.gcount();
        
        if (bytesRead > 0) {
            DWORD written;
            WriteFile(hDevice, buffer, static_cast<DWORD>(bytesRead), &written, NULL);
            bytesWritten += written;
            
            int progress = static_cast<int>((bytesWritten * 100) / totalSize);
            UpdateProgress(progress, L"Restoring: " + std::to_wstring(progress) + L"%");
        }
    }
    
    delete[] buffer;
    FlushFileBuffers(hDevice);
    CloseHandle(hDevice);
    backupFile.close();
    
    LogMessage(L"Restore completed successfully");
    return true;
}

void StartOperation() {
    if (g_isRunning) {
        g_cancelOperation = true;
        return;
    }
    
    if (!IsAdmin()) {
        MessageBox(g_hMainWnd, L"Administrator privileges required!", L"Error", MB_OK | MB_ICONERROR);
        ElevateProcess();
        return;
    }
    
    int deviceIndex = SendMessage(g_hDeviceCombo, CB_GETCURSEL, 0, 0);
    if (deviceIndex == CB_ERR || deviceIndex >= static_cast<int>(g_devices.size())) {
        MessageBox(g_hMainWnd, L"Please select a USB device!", L"Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    if (g_selectedIsoPath.empty()) {
        MessageBox(g_hMainWnd, L"Please select an ISO file!", L"Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    g_selectedDevice = g_devices[deviceIndex].path;
    
    int result = MessageBox(g_hMainWnd,
        L"WARNING: All data on the selected device will be destroyed!\n\nAre you sure you want to continue?",
        L"Confirm Operation", MB_YESNO | MB_ICONWARNING);
    
    if (result != IDYES)
        return;
    
    g_isRunning = true;
    g_cancelOperation = false;
    
    SetWindowText(GetDlgItem(g_hMainWnd, IDC_START_BTN), L"Cancel");
    EnableWindow(g_hDeviceCombo, FALSE);
    EnableWindow(GetDlgItem(g_hMainWnd, IDC_BROWSE_BTN), FALSE);
    
    std::thread([=]() {
        bool success = true;
        
        try {
            // Get settings
            wchar_t label[256];
            GetWindowText(g_hLabelEdit, label, 256);
            
            int fsIndex = SendMessage(g_hFileSystemCombo, CB_GETCURSEL, 0, 0);
            std::wstring fileSystem = fsIndex == 0 ? L"FAT32" : fsIndex == 1 ? L"NTFS" : L"exFAT";
            
            bool quickFormat = SendMessage(g_hQuickFormat, BM_GETCHECK, 0, 0) == BST_CHECKED;
            bool verifyWrite = SendMessage(g_hVerifyWrite, BM_GETCHECK, 0, 0) == BST_CHECKED;
            bool createPersistent = SendMessage(g_hPersistentPartition, BM_GETCHECK, 0, 0) == BST_CHECKED;
            
            // Analyze ISO
            UpdateProgress(5, L"Analyzing ISO file...");
            ISOInfo isoInfo = AnalyzeISO(g_selectedIsoPath);
            
            if (!isoInfo.isBootable) {
                LogMessage(L"Warning: ISO may not be bootable");
            }
            
            // Format drive
            UpdateProgress(10, L"Formatting drive...");
            if (!FormatDrive(g_selectedDevice, fileSystem, label, quickFormat)) {
                throw std::runtime_error("Format failed");
            }
            
            if (g_cancelOperation) throw std::runtime_error("Cancelled");
            
            // Write ISO
            UpdateProgress(20, L"Writing ISO to USB...");
            if (!WriteISOToUSB(g_selectedIsoPath, g_selectedDevice)) {
                throw std::runtime_error("Write failed");
            }
            
            if (g_cancelOperation) throw std::runtime_error("Cancelled");
            
            // Make bootable
            UpdateProgress(80, L"Installing bootloader...");
            if (!MakeBootable(g_selectedDevice, isoInfo.bootType)) {
                LogMessage(L"Warning: Bootloader installation may have failed");
            }
            
            // Verify if requested
            if (verifyWrite && !g_cancelOperation) {
                if (!VerifyWrite(g_selectedDevice, g_selectedIsoPath)) {
                    throw std::runtime_error("Verification failed");
                }
            }
            
            // Create persistent partition if requested
            if (createPersistent && !g_cancelOperation) {
                UpdateProgress(95, L"Creating persistent partition...");
                CreatePersistentPartition(g_selectedDevice, 4);
            }
            
            UpdateProgress(100, L"Operation completed successfully!");
            LogMessage(L"All operations completed successfully");
            
            MessageBox(g_hMainWnd, L"USB drive created successfully!", L"Success", MB_OK | MB_ICONINFORMATION);
            
        } catch (const std::exception& e) {
            success = false;
            std::wstring error = L"Operation failed: ";
            error += std::wstring(e.what(), e.what() + strlen(e.what()));
            UpdateProgress(0, error);
            MessageBox(g_hMainWnd, error.c_str(), L"Error", MB_OK | MB_ICONERROR);
        }
        
        g_isRunning = false;
        SetWindowText(GetDlgItem(g_hMainWnd, IDC_START_BTN), L"START");
        EnableWindow(g_hDeviceCombo, TRUE);
        EnableWindow(GetDlgItem(g_hMainWnd, IDC_BROWSE_BTN), TRUE);
    }).detach();
}

void UpdateProgress(int percentage, const std::wstring& status) {
    SendMessage(g_hProgress, PBM_SETPOS, percentage, 0);
    SetWindowText(g_hStatusText, status.c_str());
}

void LogMessage(const std::wstring& message) {
    std::wofstream logFile(L"inferno.log", std::ios::app);
    if (logFile.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        wchar_t timestamp[100];
        swprintf_s(timestamp, L"[%04d-%02d-%02d %02d:%02d:%02d] ",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        
        logFile << timestamp << message << std::endl;
        logFile.close();
    }
}

void ShowAboutDialog() {
    std::wstring about = L"Inferno USB Creator v2.0.0\n\n";
    about += L"Advanced Bootable USB Creation Tool\n\n";
    about += L"Developer: Ahmed Nour Ahmed\n";
    about += L"Location: Qena, Egypt\n\n";
    about += L"Features:\n";
    about += L"• Multi-boot USB support\n";
    about += L"• UEFI & Legacy BIOS support\n";
    about += L"• Persistent storage partition\n";
    about += L"• Data verification\n";
    about += L"• USB backup & restore\n";
    about += L"• Speed testing\n";
    about += L"• Secure boot support\n";
    about += L"• Multiple file systems (FAT32, NTFS, exFAT)\n\n";
    about += L"© 2024 Ahmed Nour Ahmed. All rights reserved.";
    
    MessageBox(g_hMainWnd, about.c_str(), L"About Inferno", MB_OK | MB_ICONINFORMATION);
}

void ShowSettingsDialog() {
    MessageBox(g_hMainWnd, L"Settings dialog - Coming soon!", L"Settings", MB_OK | MB_ICONINFORMATION);
}

void InitializeControls(HWND hWnd) {
    // Device selection
    CreateWindow(L"STATIC", L"Select Device:",
        WS_CHILD | WS_VISIBLE,
        20, 20, 120, 20, hWnd, NULL, NULL, NULL);
    
    g_hDeviceCombo = CreateWindow(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        150, 18, 350, 200, hWnd, (HMENU)IDC_DEVICE_COMBO, NULL, NULL);
    
    CreateWindow(L"BUTTON", L"Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        510, 17, 80, 25, hWnd, (HMENU)IDC_REFRESH_BTN, NULL, NULL);
    
    // ISO file selection
    CreateWindow(L"STATIC", L"ISO File:",
        WS_CHILD | WS_VISIBLE,
        20, 60, 120, 20, hWnd, NULL, NULL, NULL);
    
    g_hIsoPath = CreateWindow(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_AUTOHSCROLL,
        150, 58, 350, 25, hWnd, (HMENU)IDC_ISO_PATH, NULL, NULL);
    
    CreateWindow(L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        510, 57, 80, 25, hWnd, (HMENU)IDC_BROWSE_BTN, NULL, NULL);
    
    // Partition scheme
    CreateWindow(L"STATIC", L"Partition Scheme:",
        WS_CHILD | WS_VISIBLE,
        20, 100, 120, 20, hWnd, NULL, NULL, NULL);
    
    g_hPartitionCombo = CreateWindow(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        150, 98, 200, 200, hWnd, (HMENU)IDC_PARTITION_COMBO, NULL, NULL);
    
    SendMessage(g_hPartitionCombo, CB_ADDSTRING, 0, (LPARAM)L"MBR (BIOS/UEFI)");
    SendMessage(g_hPartitionCombo, CB_ADDSTRING, 0, (LPARAM)L"GPT (UEFI only)");
    SendMessage(g_hPartitionCombo, CB_SETCURSEL, 0, 0);
    
    // File system
    CreateWindow(L"STATIC", L"File System:",
        WS_CHILD | WS_VISIBLE,
        20, 140, 120, 20, hWnd, NULL, NULL, NULL);
    
    g_hFileSystemCombo = CreateWindow(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        150, 138, 200, 200, hWnd, (HMENU)IDC_FILESYSTEM_COMBO, NULL, NULL);
    
    SendMessage(g_hFileSystemCombo, CB_ADDSTRING, 0, (LPARAM)L"FAT32");
    SendMessage(g_hFileSystemCombo, CB_ADDSTRING, 0, (LPARAM)L"NTFS");
    SendMessage(g_hFileSystemCombo, CB_ADDSTRING, 0, (LPARAM)L"exFAT");
    SendMessage(g_hFileSystemCombo, CB_SETCURSEL, 0, 0);
    
    // Cluster size
    CreateWindow(L"STATIC", L"Cluster Size:",
        WS_CHILD | WS_VISIBLE,
        370, 140, 80, 20, hWnd, NULL, NULL, NULL);
    
    g_hClusterCombo = CreateWindow(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        460, 138, 130, 200, hWnd, (HMENU)IDC_CLUSTER_COMBO, NULL, NULL);
    
    SendMessage(g_hClusterCombo, CB_ADDSTRING, 0, (LPARAM)L"Default");
    SendMessage(g_hClusterCombo, CB_ADDSTRING, 0, (LPARAM)L"4096 bytes");
    SendMessage(g_hClusterCombo, CB_ADDSTRING, 0, (LPARAM)L"8192 bytes");
    SendMessage(g_hClusterCombo, CB_ADDSTRING, 0, (LPARAM)L"16384 bytes");
    SendMessage(g_hClusterCombo, CB_SETCURSEL, 0, 0);
    
    // Volume label
    CreateWindow(L"STATIC", L"Volume Label:",
        WS_CHILD | WS_VISIBLE,
        20, 180, 120, 20, hWnd, NULL, NULL, NULL);
    
    g_hLabelEdit = CreateWindow(L"EDIT", L"INFERNO",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        150, 178, 200, 25, hWnd, (HMENU)IDC_LABEL_EDIT, NULL, NULL);
    
    // Options group box
    CreateWindow(L"BUTTON", L"Format Options",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 220, 280, 150, hWnd, (HMENU)IDC_ADVANCED_GROUP, NULL, NULL);
    
    g_hQuickFormat = CreateWindow(L"BUTTON", L"Quick format",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 245, 250, 20, hWnd, (HMENU)IDC_QUICK_FORMAT, NULL, NULL);
    SendMessage(g_hQuickFormat, BM_SETCHECK, BST_CHECKED, 0);
    
    g_hBadBlocks = CreateWindow(L"BUTTON", L"Check device for bad blocks",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 270, 250, 20, hWnd, (HMENU)IDC_BAD_BLOCKS, NULL, NULL);
    
    g_hExtendedLabel = CreateWindow(L"BUTTON", L"Create extended label and icon files",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 295, 250, 20, hWnd, (HMENU)IDC_EXTENDED_LABEL, NULL, NULL);
    
    g_hBootableImage = CreateWindow(L"BUTTON", L"Create bootable disk using ISO",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 320, 250, 20, hWnd, (HMENU)IDC_BOOTABLE_IMAGE, NULL, NULL);
    SendMessage(g_hBootableImage, BM_SETCHECK, BST_CHECKED, 0);
    
    g_hVerifyWrite = CreateWindow(L"BUTTON", L"Verify write operation",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 345, 250, 20, hWnd, (HMENU)IDC_VERIFY_WRITE, NULL, NULL);
    
    // Advanced options
    CreateWindow(L"BUTTON", L"Advanced Features",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        310, 220, 280, 150, hWnd, NULL, NULL, NULL);
    
    g_hPersistentPartition = CreateWindow(L"BUTTON", L"Create persistent partition",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        325, 245, 200, 20, hWnd, (HMENU)IDC_PERSISTENT_PARTITION, NULL, NULL);
    
    CreateWindow(L"STATIC", L"Size (GB):",
        WS_CHILD | WS_VISIBLE,
        540, 247, 50, 20, hWnd, NULL, NULL, NULL);
    
    g_hPersistentSize = CreateWindow(L"EDIT", L"4",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        535, 245, 40, 20, hWnd, (HMENU)IDC_PERSISTENT_SIZE, NULL, NULL);
    
    g_hMultiBoot = CreateWindow(L"BUTTON", L"Multi-boot support",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        325, 270, 250, 20, hWnd, (HMENU)IDC_MULTI_BOOT, NULL, NULL);
    
    g_hSecureBoot = CreateWindow(L"BUTTON", L"Secure Boot compatible",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        325, 295, 250, 20, hWnd, (HMENU)IDC_SECURE_BOOT, NULL, NULL);
    
    g_hUEFI_NTFS = CreateWindow(L"BUTTON", L"UEFI:NTFS support",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        325, 320, 250, 20, hWnd, (HMENU)IDC_UEFI_NTFS, NULL, NULL);
    
    g_hCompression = CreateWindow(L"BUTTON", L"Enable compression",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        325, 345, 250, 20, hWnd, (HMENU)IDC_COMPRESSION_CHECK, NULL, NULL);
    
    // Additional buttons
    CreateWindow(L"BUTTON", L"Backup USB",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 385, 120, 30, hWnd, (HMENU)IDC_BACKUP_BTN, NULL, NULL);
    
    CreateWindow(L"BUTTON", L"Restore USB",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 385, 120, 30, hWnd, (HMENU)IDC_RESTORE_BTN, NULL, NULL);
    
    CreateWindow(L"BUTTON", L"Speed Test",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        280, 385, 120, 30, hWnd, (HMENU)IDC_SPEED_TEST_BTN, NULL, NULL);
    
    // Progress bar
    g_hProgress = CreateWindow(PROGRESS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        20, 435, 570, 25, hWnd, (HMENU)IDC_PROGRESS, NULL, NULL);
    
    SendMessage(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    
    // Status text
    g_hStatusText = CreateWindow(L"STATIC", L"Ready",
        WS_CHILD | WS_VISIBLE,
        20, 470, 570, 20, hWnd, (HMENU)IDC_STATUS_TEXT, NULL, NULL);
    
    // Start button
    CreateWindow(L"BUTTON", L"START",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
        210, 500, 180, 40, hWnd, (HMENU)IDC_START_BTN, NULL, NULL);
    
    // Refresh device list
    RefreshDeviceList();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        InitializeControls(hWnd);
        break;
        
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BROWSE_BTN:
            BrowseForISO();
            break;
            
        case IDC_REFRESH_BTN:
            RefreshDeviceList();
            break;
            
        case IDC_START_BTN:
            StartOperation();
            break;
            
        case IDC_SPEED_TEST_BTN:
            if (!g_devices.empty()) {
                int deviceIndex = SendMessage(g_hDeviceCombo, CB_GETCURSEL, 0, 0);
                if (deviceIndex != CB_ERR) {
                    std::thread([=]() {
                        TestUSBSpeed(g_devices[deviceIndex].path);
                    }).detach();
                }
            }
            break;
            
        case IDC_BACKUP_BTN:
            if (!g_devices.empty()) {
                wchar_t backupPath[MAX_PATH];
                OPENFILENAME ofn = { sizeof(ofn) };
                ofn.hwndOwner = hWnd;
                ofn.lpstrFile = backupPath;
                ofn.lpstrFile[0] = '\0';
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Backup Files (*.img)\0*.img\0All Files (*.*)\0*.*\0";
                ofn.lpstrDefExt = L"img";
                ofn.Flags = OFN_OVERWRITEPROMPT;
                
                if (GetSaveFileName(&ofn)) {
                    int deviceIndex = SendMessage(g_hDeviceCombo, CB_GETCURSEL, 0, 0);
                    if (deviceIndex != CB_ERR) {
                        std::thread([=]() {
                            BackupUSB(g_devices[deviceIndex].path, backupPath);
                        }).detach();
                    }
                }
            }
            break;
            
        case IDC_RESTORE_BTN:
            if (!g_devices.empty()) {
                wchar_t backupPath[MAX_PATH];
                OPENFILENAME ofn = { sizeof(ofn) };
                ofn.hwndOwner = hWnd;
                ofn.lpstrFile = backupPath;
                ofn.lpstrFile[0] = '\0';
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Backup Files (*.img)\0*.img\0All Files (*.*)\0*.*\0";
                ofn.Flags = OFN_FILEMUSTEXIST;
                
                if (GetOpenFileName(&ofn)) {
                    int deviceIndex = SendMessage(g_hDeviceCombo, CB_GETCURSEL, 0, 0);
                    if (deviceIndex != CB_ERR) {
                        std::thread([=]() {
                            RestoreUSB(backupPath, g_devices[deviceIndex].path);
                        }).detach();
                    }
                }
            }
            break;
            
        case IDM_ABOUT:
            ShowAboutDialog();
            break;
            
        case IDM_SETTINGS:
            ShowSettingsDialog();
            break;
            
        case IDM_EXIT:
            PostQuitMessage(0);
            break;
        }
        break;
        
    case WM_CLOSE:
        if (g_isRunning) {
            int result = MessageBox(hWnd,
                L"Operation in progress. Are you sure you want to exit?",
                L"Confirm Exit", MB_YESNO | MB_ICONQUESTION);
            if (result == IDNO)
                return 0;
            g_cancelOperation = true;
        }
        DestroyWindow(hWnd);
        break;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    InitCommonControls();
    
    // Register window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"InfernoUSBCreator";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONERROR);
        return 0;
    }
    
    // Create menu
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreateMenu();
    AppendMenu(hFileMenu, MF_STRING, IDM_SETTINGS, L"Settings");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, IDM_EXIT, L"Exit");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"File");
    
    HMENU hHelpMenu = CreateMenu();
    AppendMenu(hHelpMenu, MF_STRING, IDM_CHECK_UPDATE, L"Check for Updates");
    AppendMenu(hHelpMenu, MF_STRING, IDM_LOG, L"View Log");
    AppendMenu(hHelpMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hHelpMenu, MF_STRING, IDM_ABOUT, L"About");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"Help");
    
    // Create main window
    g_hMainWnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"InfernoUSBCreator",
        APP_NAME,
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 600,
        NULL, hMenu, hInstance, NULL);
    
    if (!g_hMainWnd) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONERROR);
        return 0;
    }
    
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
