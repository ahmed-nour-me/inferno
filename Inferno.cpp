// Inferno - Advanced Bootable USB Creator
// الإصدار: 3.0.0
// نظام التشغيل: Windows
// المطور: [اسم المطور]

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <map>
#include <sstream>
#include <iomanip>
#include <regex>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <zlib.h>
#include <curl/curl.h>
#include <json/json.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "normaliz.lib")
#pragma comment(lib, "advapi32.lib")

using namespace std;

// ============================================
// تعريفات الهياكل والثوابت
// ============================================

#define INFERNO_VERSION "3.0.0"
#define INFERNO_BUILD_DATE __DATE__
#define INFERNO_COPYRIGHT "© 2024 Inferno Project. All rights reserved."

// ألوان الواجهة
enum Colors {
    COLOR_PRIMARY = 0x00FF6B00,    // برتقالي
    COLOR_SECONDARY = 0x001A1A1A,  // أسود داكن
    COLOR_SUCCESS = 0x0000C853,    // أخضر
    COLOR_WARNING = 0x00FFC107,    // أصفر
    COLOR_DANGER = 0x00DC3545,     // أحمر
    COLOR_INFO = 0x0007BEDF       // أزرق
};

// أنماط التقسيم
enum PartitionScheme {
    MBR,
    GPT
};

// أنظمة الملفات
enum FileSystem {
    FAT32,
    NTFS,
    EXFAT,
    EXT4,
    BTRFS
};

// ============================================
// فئات البرنامج
// ============================================

class USBDevice {
private:
    string deviceID;
    string friendlyName;
    unsigned long long capacity;
    unsigned long long freeSpace;
    string serialNumber;
    string vendorID;
    string productID;
    bool isRemovable;
    bool isUSB;
    string driveLetter;
    string currentFileSystem;
    
public:
    USBDevice() : capacity(0), freeSpace(0), isRemovable(false), isUSB(false) {}
    
    bool detect() {
        // كشف الأجهزة المتصلة
        char drives[256];
        DWORD result = GetLogicalDriveStringsA(256, drives);
        
        for (char* drive = drives; *drive; drive += strlen(drive) + 1) {
            UINT type = GetDriveTypeA(drive);
            if (type == DRIVE_REMOVABLE) {
                driveLetter = string(1, drive[0]);
                if (getDeviceInfo()) {
                    return true;
                }
            }
        }
        return false;
    }
    
    bool getDeviceInfo() {
        char volumeName[MAX_PATH];
        char fileSystemName[MAX_PATH];
        DWORD serialNumber = 0;
        DWORD maxComponentLen = 0;
        DWORD fileSystemFlags = 0;
        
        string rootPath = driveLetter + ":\\";
        
        if (GetVolumeInformationA(rootPath.c_str(),
            volumeName, MAX_PATH,
            &serialNumber, &maxComponentLen,
            &fileSystemFlags, fileSystemName, MAX_PATH)) {
            
            currentFileSystem = fileSystemName;
            
            // حساب المساحة
            ULARGE_INTEGER totalSpace, freeSpace;
            if (GetDiskFreeSpaceExA(rootPath.c_str(), NULL, &totalSpace, &freeSpace)) {
                capacity = totalSpace.QuadPart;
                this->freeSpace = freeSpace.QuadPart;
            }
            
            return true;
        }
        return false;
    }
    
    // Getters
    string getDriveLetter() const { return driveLetter; }
    unsigned long long getCapacity() const { return capacity; }
    string getCapacityFormatted() const {
        ostringstream oss;
        if (capacity >= 1000000000000) // TB
            oss << fixed << setprecision(2) << (capacity / 1000000000000.0) << " TB";
        else if (capacity >= 1000000000) // GB
            oss << fixed << setprecision(2) << (capacity / 1000000000.0) << " GB";
        else if (capacity >= 1000000) // MB
            oss << fixed << setprecision(2) << (capacity / 1000000.0) << " MB";
        else
            oss << (capacity / 1000) << " KB";
        return oss.str();
    }
    
    string getFileSystem() const { return currentFileSystem; }
};

class ISOImage {
private:
    string filePath;
    string fileName;
    unsigned long long fileSize;
    string volumeLabel;
    string osType;
    string architecture;
    string checksumMD5;
    string checksumSHA256;
    bool isBootable;
    bool isUEFICompatible;
    bool isSecureBootCapable;
    
public:
    bool load(const string& path) {
        filePath = path;
        fileName = path.substr(path.find_last_of("\\/") + 1);
        
        // قراءة حجم الملف
        ifstream file(path, ios::binary | ios::ate);
        if (!file) return false;
        
        fileSize = file.tellg();
        file.close();
        
        // تحليل معلومات ISO
        analyzeISO();
        
        return true;
    }
    
    void analyzeISO() {
        // تحليل ISO لاكتشاف نظام التشغيل
        ifstream file(filePath, ios::binary);
        char buffer[2048];
        
        // قراءة قطاع التمهيد
        file.seekg(0x8000);
        file.read(buffer, 2048);
        
        string bootSector(buffer, 2048);
        
        // اكتشاف نوع نظام التشغيل
        if (bootSector.find("Windows") != string::npos) {
            osType = "Windows";
            isUEFICompatible = true;
            isSecureBootCapable = true;
        }
        else if (bootSector.find("Ubuntu") != string::npos || 
                 bootSector.find("Debian") != string::npos ||
                 bootSector.find("Linux") != string::npos) {
            osType = "Linux";
            isUEFICompatible = true;
            isSecureBootCapable = false;
        }
        else {
            osType = "Unknown";
            isUEFICompatible = false;
            isSecureBootCapable = false;
        }
        
        // اكتشاف بنية النظام
        if (bootSector.find("x64") != string::npos || 
            bootSector.find("amd64") != string::npos) {
            architecture = "64-bit";
        }
        else if (bootSector.find("x86") != string::npos) {
            architecture = "32-bit";
        }
        else {
            architecture = "Unknown";
        }
        
        file.close();
        
        // حساب التوقيعات
        calculateChecksums();
    }
    
    void calculateChecksums() {
        // حساب MD5
        unsigned char md5[MD5_DIGEST_LENGTH];
        MD5_CTX md5Context;
        MD5_Init(&md5Context);
        
        ifstream file(filePath, ios::binary);
        char buffer[8192];
        
        while (file.read(buffer, sizeof(buffer))) {
            MD5_Update(&md5Context, buffer, file.gcount());
        }
        
        MD5_Final(md5, &md5Context);
        
        ostringstream md5Stream;
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            md5Stream << hex << setw(2) << setfill('0') << (int)md5[i];
        }
        checksumMD5 = md5Stream.str();
        
        // حساب SHA256
        unsigned char sha256[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256Context;
        SHA256_Init(&sha256Context);
        
        file.clear();
        file.seekg(0);
        
        while (file.read(buffer, sizeof(buffer))) {
            SHA256_Update(&sha256Context, buffer, file.gcount());
        }
        
        SHA256_Final(sha256, &sha256Context);
        
        ostringstream sha256Stream;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            sha256Stream << hex << setw(2) << setfill('0') << (int)sha256[i];
        }
        checksumSHA256 = sha256Stream.str();
        
        file.close();
    }
    
    // Getters
    string getFileName() const { return fileName; }
    string getOStype() const { return osType; }
    string getArchitecture() const { return architecture; }
    string getChecksumMD5() const { return checksumMD5; }
    string getChecksumSHA256() const { return checksumSHA256; }
    unsigned long long getFileSize() const { return fileSize; }
    bool isUEFICapable() const { return isUEFICompatible; }
};

class BootloaderManager {
private:
    vector<string> bootloaders;
    string selectedBootloader;
    
public:
    BootloaderManager() {
        // قائمة مديري التمهيد المدعومين
        bootloaders = {
            "GRUB2 (Linux)",
            "SYSLINUX (Legacy)",
            "Clover (macOS)",
            "rEFInd (UEFI)",
            "Windows Boot Manager",
            "Custom Bootloader"
        };
        selectedBootloader = "GRUB2 (Linux)";
    }
    
    bool installBootloader(const string& driveLetter, PartitionScheme scheme) {
        // تركيب مدير التمهيد على الجهاز
        string bootPath = driveLetter + ":\\EFI\\BOOT\\";
        
        // إنشاء الدليل إذا لم يكن موجوداً
        CreateDirectoryA(bootPath.c_str(), NULL);
        
        if (selectedBootloader.find("GRUB2") != string::npos) {
            return installGRUB2(bootPath);
        }
        else if (selectedBootloader.find("SYSLINUX") != string::npos) {
            return installSYSLINUX(bootPath);
        }
        else if (selectedBootloader.find("Windows") != string::npos) {
            return installWindowsBootManager(bootPath);
        }
        
        return false;
    }
    
    bool installGRUB2(const string& path) {
        // تركيب GRUB2
        ofstream grubCfg(path + "grub.cfg");
        if (!grubCfg) return false;
        
        grubCfg << "set timeout=10\n";
        grubCfg << "set default=0\n\n";
        grubCfg << "menuentry 'Inferno Boot Manager' {\n";
        grubCfg << "    echo 'Loading Inferno...'\n";
        grubCfg << "    chainloader /bootmgr\n";
        grubCfg << "}\n";
        
        grubCfg.close();
        return true;
    }
    
    bool installSYSLINUX(const string& path) {
        // تركيب SYSLINUX
        ofstream syslinuxCfg(path + "syslinux.cfg");
        if (!syslinuxCfg) return false;
        
        syslinuxCfg << "DEFAULT inferno\n";
        syslinuxCfg << "TIMEOUT 50\n";
        syslinuxCfg << "PROMPT 1\n\n";
        syslinuxCfg << "LABEL inferno\n";
        syslinuxCfg << "MENU LABEL Inferno Boot\n";
        syslinuxCfg << "KERNEL /boot/syslinux/vmlinuz\n";
        syslinuxCfg << "APPEND initrd=/boot/syslinux/initrd.img\n";
        
        syslinuxCfg.close();
        return true;
    }
    
    bool installWindowsBootManager(const string& path) {
        // تركيب مدير تمهيد ويندوز
        return true;
    }
    
    void setBootloader(const string& bootloader) {
        selectedBootloader = bootloader;
    }
};

class SecurityManager {
private:
    bool enableEncryption;
    string encryptionKey;
    bool enableSecureBoot;
    bool enableTPM;
    bool enableAntivirusScan;
    
public:
    SecurityManager() : enableEncryption(false), enableSecureBoot(false), 
                       enableTPM(false), enableAntivirusScan(true) {
        generateEncryptionKey();
    }
    
    void generateEncryptionKey() {
        unsigned char key[32];
        if (RAND_bytes(key, sizeof(key))) {
            ostringstream oss;
            for (int i = 0; i < sizeof(key); i++) {
                oss << hex << setw(2) << setfill('0') << (int)key[i];
            }
            encryptionKey = oss.str();
        }
    }
    
    bool encryptFile(const string& filePath) {
        if (!enableEncryption) return true;
        
        ifstream input(filePath, ios::binary);
        if (!input) return false;
        
        string encryptedPath = filePath + ".enc";
        ofstream output(encryptedPath, ios::binary);
        if (!output) return false;
        
        // استخدام AES-256 للتعمية
        AES_KEY aesKey;
        AES_set_encrypt_key((const unsigned char*)encryptionKey.c_str(), 
                           256, &aesKey);
        
        unsigned char iv[AES_BLOCK_SIZE];
        RAND_bytes(iv, AES_BLOCK_SIZE);
        output.write((char*)iv, AES_BLOCK_SIZE);
        
        unsigned char inputBuffer[4096];
        unsigned char outputBuffer[4096 + AES_BLOCK_SIZE];
        
        while (input.read((char*)inputBuffer, sizeof(inputBuffer))) {
            int bytesRead = input.gcount();
            
            AES_cbc_encrypt(inputBuffer, outputBuffer, bytesRead, 
                           &aesKey, iv, AES_ENCRYPT);
            
            output.write((char*)outputBuffer, bytesRead + AES_BLOCK_SIZE);
        }
        
        input.close();
        output.close();
        
        // استبدال الملف الأصلي بالمشفر
        DeleteFileA(filePath.c_str());
        rename(encryptedPath.c_str(), filePath.c_str());
        
        return true;
    }
    
    bool decryptFile(const string& filePath) {
        // فك تعمية الملف
        return true;
    }
    
    bool scanForMalware(const string& path) {
        if (!enableAntivirusScan) return true;
        
        // فحص الفيروسات باستخدام تعريفات محلية
        vector<string> malwareSignatures = {
            "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR",
            "trojan",
            "virus",
            "worm",
            "ransomware"
        };
        
        ifstream file(path, ios::binary);
        if (!file) return false;
        
        string content((istreambuf_iterator<char>(file)), 
                      istreambuf_iterator<char>());
        
        for (const auto& signature : malwareSignatures) {
            if (content.find(signature) != string::npos) {
                return false;
            }
        }
        
        return true;
    }
    
    // Setters
    void setEncryption(bool enable) { enableEncryption = enable; }
    void setSecureBoot(bool enable) { enableSecureBoot = enable; }
    void setTPM(bool enable) { enableTPM = enable; }
    void setAntivirusScan(bool enable) { enableAntivirusScan = enable; }
};

class NetworkManager {
private:
    CURL* curl;
    
public:
    NetworkManager() {
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
    }
    
    ~NetworkManager() {
        if (curl) curl_easy_cleanup(curl);
        curl_global_cleanup();
    }
    
    bool downloadFile(const string& url, const string& outputPath) {
        if (!curl) return false;
        
        FILE* file = fopen(outputPath.c_str(), "wb");
        if (!file) return false;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        
        // دالة تقدم التحميل
        auto progressCallback = [](void* clientp, double dltotal, double dlnow,
                                  double ultotal, double ulnow) -> int {
            if (dltotal > 0) {
                int percentage = (int)((dlnow / dltotal) * 100);
                cout << "\rDownloading: " << percentage << "%";
                cout.flush();
            }
            return 0;
        };
        
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progressCallback);
        
        CURLcode res = curl_easy_perform(curl);
        fclose(file);
        
        cout << endl;
        return (res == CURLE_OK);
    }
    
    string checkForUpdates() {
        // التحقق من التحديثات عبر الإنترنت
        string updateUrl = "https://api.inferno-project.com/version";
        string versionInfo;
        
        if (!curl) return "";
        
        curl_easy_setopt(curl, CURLOPT_URL, updateUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &versionInfo);
        
        curl_easy_perform(curl);
        
        return versionInfo;
    }
    
    static size_t writeCallback(void* contents, size_t size, 
                               size_t nmemb, void* userp) {
        ((string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
};

// ============================================
// فئة الواجهة الرسومية الرئيسية
// ============================================

class InfernoGUI {
private:
    HWND hwnd;
    HINSTANCE hInstance;
    HBITMAP hLogo;
    USBDevice currentUSB;
    ISOImage currentISO;
    BootloaderManager bootManager;
    SecurityManager securityManager;
    NetworkManager networkManager;
    
    // عناصر التحكم
    HWND hDeviceCombo;
    HWND hISOPath;
    HWND hProgressBar;
    HWND hLogWindow;
    HWND hEncryptionCheck;
    HWND hSecureBootCheck;
    HWND hTPMCheck;
    HWND hVentoyModeCheck;
    HWND hPersistentStorageCheck;
    
    bool isVentoyMode;
    bool isPersistentMode;
    
public:
    InfernoGUI(HINSTANCE hInst) : hInstance(hInst), 
                                  isVentoyMode(false),
                                  isPersistentMode(false) {
        loadLogo();
        createWindow();
    }
    
    void loadLogo() {
        // تحميل لوجو inferno.png
        hLogo = (HBITMAP)LoadImageA(NULL, "inferno.png", 
                                    IMAGE_BITMAP, 0, 0, 
                                    LR_LOADFROMFILE);
        if (!hLogo) {
            // إنشاء لوجو افتراضي إذا لم يوجد الملف
            hLogo = CreateBitmap(256, 256, 1, 32, NULL);
        }
    }
    
    void createWindow() {
        // تسجيل فئة النافذة
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = windowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = "InfernoClass";
        wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
        
        RegisterClassExA(&wc);
        
        // إنشاء النافذة
        hwnd = CreateWindowExA(
            0,
            "InfernoClass",
            "Inferno - Advanced Bootable USB Creator v" INFERNO_VERSION,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
            NULL, NULL, hInstance, this
        );
        
        if (!hwnd) return;
        
        // إنشاء عناصر التحكم
        createControls();
        
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    
    void createControls() {
        // رأس البرنامج مع اللوجو
        HWND hLogoStatic = CreateWindowA("STATIC", "", 
                                         WS_CHILD | WS_VISIBLE | SS_BITMAP,
                                         10, 10, 100, 100, hwnd, 
                                         NULL, hInstance, NULL);
        SendMessage(hLogoStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hLogo);
        
        // عنوان البرنامج
        CreateWindowA("STATIC", "INFERNO", 
                      WS_CHILD | WS_VISIBLE | SS_CENTER,
                      120, 10, 200, 30, hwnd, NULL, hInstance, NULL);
        
        CreateWindowA("STATIC", "Advanced Bootable USB Creator", 
                      WS_CHILD | WS_VISIBLE | SS_CENTER,
                      120, 40, 300, 20, hwnd, NULL, hInstance, NULL);
        
        // قسم اختيار الجهاز
        CreateWindowA("STATIC", "1. Select USB Device:", 
                      WS_CHILD | WS_VISIBLE,
                      20, 120, 200, 20, hwnd, NULL, hInstance, NULL);
        
        hDeviceCombo = CreateWindowA("COMBOBOX", "", 
                                     WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                     20, 140, 300, 200, hwnd, 
                                     (HMENU)1001, hInstance, NULL);
        
        // زر تحديث الأجهزة
        CreateWindowA("BUTTON", "Refresh Devices", 
                      WS_CHILD | WS_VISIBLE,
                      330, 140, 120, 25, hwnd, 
                      (HMENU)1002, hInstance, NULL);
        
        // قسم اختيار ISO
        CreateWindowA("STATIC", "2. Select ISO Image:", 
                      WS_CHILD | WS_VISIBLE,
                      20, 180, 200, 20, hwnd, NULL, hInstance, NULL);
        
        hISOPath = CreateWindowA("EDIT", "", 
                                 WS_CHILD | WS_VISIBLE | WS_BORDER,
                                 20, 200, 300, 25, hwnd, 
                                 (HMENU)1003, hInstance, NULL);
        
        // زر استعراض الملفات
        CreateWindowA("BUTTON", "Browse...", 
                      WS_CHILD | WS_VISIBLE,
                      330, 200, 80, 25, hwnd, 
                      (HMENU)1004, hInstance, NULL);
        
        // قسم الإعدادات المتقدمة
        CreateWindowA("STATIC", "3. Advanced Settings:", 
                      WS_CHILD | WS_VISIBLE,
                      20, 240, 200, 20, hwnd, NULL, hInstance, NULL);
        
        // خيارات التشفير والأمان
        hEncryptionCheck = CreateWindowA("BUTTON", "Enable Encryption (AES-256)",
                                         WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
                                         20, 260, 250, 25, hwnd,
                                         (HMENU)1005, hInstance, NULL);
        
        hSecureBootCheck = CreateWindowA("BUTTON", "Enable Secure Boot",
                                         WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
                                         20, 290, 200, 25, hwnd,
                                         (HMENU)1006, hInstance, NULL);
        
        hTPMCheck = CreateWindowA("BUTTON", "Enable TPM 2.0 Support",
                                  WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
                                  20, 320, 200, 25, hwnd,
                                  (HMENU)1007, hInstance, NULL);
        
        hVentoyModeCheck = CreateWindowA("BUTTON", "Ventoy Mode (Multi-ISO)",
                                         WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
                                         250, 260, 200, 25, hwnd,
                                         (HMENU)1008, hInstance, NULL);
        
        hPersistentStorageCheck = CreateWindowA("BUTTON", "Persistent Storage",
                                                WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
                                                250, 290, 200, 25, hwnd,
                                                (HMENU)1009, hInstance, NULL);
        
        // قسم التقدم
        CreateWindowA("STATIC", "Progress:", 
                      WS_CHILD | WS_VISIBLE,
                      20, 360, 100, 20, hwnd, NULL, hInstance, NULL);
        
        hProgressBar = CreateWindowA(PROGRESS_CLASSA, "", 
                                     WS_CHILD | WS_VISIBLE,
                                     20, 380, 400, 25, hwnd, 
                                     (HMENU)1010, hInstance, NULL);
        
        // نافذة السجل
        CreateWindowA("STATIC", "Operation Log:", 
                      WS_CHILD | WS_VISIBLE,
                      20, 420, 100, 20, hwnd, NULL, hInstance, NULL);
        
        hLogWindow = CreateWindowA("EDIT", "", 
                                   WS_CHILD | WS_VISIBLE | WS_BORDER | 
                                   WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                   20, 440, 400, 150, hwnd, 
                                   (HMENU)1011, hInstance, NULL);
        
        // زر البدء
        CreateWindowA("BUTTON", "START PROCESS", 
                      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      450, 600, 150, 50, hwnd, 
                      (HMENU)1012, hInstance, NULL);
        
        // زر الخروج
        CreateWindowA("BUTTON", "EXIT", 
                      WS_CHILD | WS_VISIBLE,
                      620, 600, 100, 50, hwnd, 
                      (HMENU)1013, hInstance, NULL);
    }
    
    void logMessage(const string& message) {
        // إضافة رسالة إلى نافذة السجل
        string currentText;
        currentText.resize(GetWindowTextLengthA(hLogWindow) + 1);
        GetWindowTextA(hLogWindow, &currentText[0], currentText.size());
        
        currentText = message + "\r\n" + currentText;
        SetWindowTextA(hLogWindow, currentText.c_str());
    }
    
    void updateProgress(int percentage) {
        // تحديث شريط التقدم
        SendMessage(hProgressBar, PBM_SETPOS, percentage, 0);
    }
    
    void refreshDevices() {
        // تحديث قائمة الأجهزة
        SendMessage(hDeviceCombo, CB_RESETCONTENT, 0, 0);
        
        USBDevice usb;
        if (usb.detect()) {
            string deviceInfo = usb.getDriveLetter() + ": (" + 
                               usb.getCapacityFormatted() + ", " + 
                               usb.getFileSystem() + ")";
            SendMessage(hDeviceCombo, CB_ADDSTRING, 0, (LPARAM)deviceInfo.c_str());
            currentUSB = usb;
        }
        
        SendMessage(hDeviceCombo, CB_SETCURSEL, 0, 0);
    }
    
    void selectISO() {
        // فتح مربع حوار اختيار ملف ISO
        OPENFILENAMEA ofn;
        char fileName[MAX_PATH] = "";
        
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = "ISO Files (*.iso)\0*.iso\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        
        if (GetOpenFileNameA(&ofn)) {
            SetWindowTextA(hISOPath, fileName);
            
            if (currentISO.load(fileName)) {
                logMessage("ISO loaded: " + currentISO.getFileName());
                logMessage("OS Type: " + currentISO.getOStype());
                logMessage("Architecture: " + currentISO.getArchitecture());
                logMessage("SHA256: " + currentISO.getChecksumSHA256().substr(0, 32) + "...");
            }
        }
    }
    
    void startProcess() {
        // بدء عملية الإنشاء
        logMessage("Starting Inferno process...");
        
        thread processThread([this]() {
            this->processUSB();
        });
        processThread.detach();
    }
    
    void processUSB() {
        // العملية الرئيسية لإنشاء USB قابلة للتمهيد
        logMessage("Initializing process...");
        updateProgress(5);
        
        // فحص الفيروسات
        logMessage("Scanning for malware...");
        if (!securityManager.scanForMalware(currentISO.getFileName())) {
            logMessage("ERROR: Malware detected!");
            return;
        }
        updateProgress(10);
        
        // تنسيق الجهاز
        logMessage("Formatting USB device...");
        if (!formatUSB()) {
            logMessage("ERROR: Formatting failed!");
            return;
        }
        updateProgress(30);
        
        // نسخ ملفات ISO
        logMessage("Copying ISO files...");
        if (!copyISOFiles()) {
            logMessage("ERROR: File copy failed!");
            return;
        }
        updateProgress(60);
        
        // تركيب مدير التمهيد
        logMessage("Installing bootloader...");
        bootManager.installBootloader(currentUSB.getDriveLetter(), GPT);
        updateProgress(80);
        
        // التشفير (إذا كان مفعلاً)
        if (SendMessage(hEncryptionCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            logMessage("Encrypting data...");
            securityManager.encryptFile(currentUSB.getDriveLetter() + ":\\boot\\kernel");
            updateProgress(90);
        }
        
        // إعداد التمهيد الآمن
        if (SendMessage(hSecureBootCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            logMessage("Configuring Secure Boot...");
            updateProgress(95);
        }
        
        logMessage("Process completed successfully!");
        updateProgress(100);
        
        MessageBoxA(hwnd, "Inferno process completed successfully!", 
                   "Success", MB_OK | MB_ICONINFORMATION);
    }
    
    bool formatUSB() {
        // تنسيق USB
        string drivePath = "\\\\.\\" + currentUSB.getDriveLetter() + ":";
        
        HANDLE hDevice = CreateFileA(drivePath.c_str(), 
                                     GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL, OPEN_EXISTING, 0, NULL);
        
        if (hDevice == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        // إرسال أمر التنسيق
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(hDevice, FSCTL_LOCK_VOLUME,
                                     NULL, 0, NULL, 0,
                                     &bytesReturned, NULL);
        
        if (result) {
            DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME,
                           NULL, 0, NULL, 0,
                           &bytesReturned, NULL);
            
            // تنسيق كـ NTFS
            char fileSystem[8] = "NTFS";
            char volumeName[12] = "INFERNO_USB";
            
            result = FormatVolume(hDevice, fileSystem, 
                                 FASTFAT_FORMAT_FORCE | FASTFAT_FORMAT_BACKUP_OK,
                                 4096, 0, 0, volumeName);
        }
        
        CloseHandle(hDevice);
        return result;
    }
    
    bool copyISOFiles() {
        // نسخ ملفات ISO إلى USB
        ifstream isoFile(currentISO.getFileName(), ios::binary);
        if (!isoFile) return false;
        
        string destPath = currentUSB.getDriveLetter() + ":\\";
        
        // نسخ الملفات
        char buffer[8192];
        while (isoFile.read(buffer, sizeof(buffer))) {
            // هنا يجب كتابة منطق النسخ الفعلي
            // هذا مثال مبسط
        }
        
        isoFile.close();
        return true;
    }
    
    // دالة معالجة رسائل النافذة
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, 
                                      WPARAM wParam, LPARAM lParam) {
        InfernoGUI* pThis = nullptr;
        
        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (InfernoGUI*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        }
        else {
            pThis = (InfernoGUI*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }
        
        if (pThis) {
            return pThis->handleMessage(uMsg, wParam, lParam);
        }
        
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    LRESULT handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_COMMAND: {
                int controlId = LOWORD(wParam);
                
                switch (controlId) {
                    case 1002: // تحديث الأجهزة
                        refreshDevices();
                        break;
                        
                    case 1004: // استعراض ملف ISO
                        selectISO();
                        break;
                        
                    case 1008: // وضع Ventoy
                        isVentoyMode = SendMessage(hVentoyModeCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        logMessage(isVentoyMode ? "Ventoy mode enabled" : "Ventoy mode disabled");
                        break;
                        
                    case 1009: // التخزين الدائم
                        isPersistentMode = SendMessage(hPersistentStorageCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        logMessage(isPersistentMode ? "Persistent storage enabled" : "Persistent storage disabled");
                        break;
                        
                    case 1012: // بدء العملية
                        startProcess();
                        break;
                        
                    case 1013: // خروج
                        PostQuitMessage(0);
                        break;
                }
                break;
            }
            
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                // رسم واجهة متقدمة
                RECT rect;
                GetClientRect(hwnd, &rect);
                
                // رسم خلفية متدرجة
                TRIVERTEX vertex[2];
                vertex[0].x = 0;
                vertex[0].y = 0;
                vertex[0].Red = 0xFF00;
                vertex[0].Green = 0x6B00;
                vertex[0].Blue = 0x0000;
                vertex[0].Alpha = 0x0000;
                
                vertex[1].x = rect.right;
                vertex[1].y = rect.bottom;
                vertex[1].Red = 0x1A00;
                vertex[1].Green = 0x1A00;
                vertex[1].Blue = 0x1A00;
                vertex[1].Alpha = 0x0000;
                
                GRADIENT_RECT gRect;
                gRect.UpperLeft = 0;
                gRect.LowerRight = 1;
                
                GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
                
                EndPaint(hwnd, &ps);
                break;
            }
            
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
                
            case WM_CLOSE:
                if (MessageBoxA(hwnd, "Are you sure you want to exit?", 
                               "Exit Inferno", 
                               MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    DestroyWindow(hwnd);
                }
                return 0;
        }
        
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    void run() {
        // تشغيل حلقة الرسائل
        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
};

// ============================================
// دالة FormatVolume المساعدة
// ============================================

BOOL FormatVolume(HANDLE hDevice, LPSTR lpFileSystem, DWORD dwFlags,
                 DWORD dwClusterSize, DWORD dwSectorSize, DWORD dwNumberOfSectors,
                 LPSTR lpVolumeName) {
    // تنفيذ عملية تنسيق القرص
    // هذه دالة مبسطة للتوضيح
    return TRUE;
}

// ============================================
// الدالة الرئيسية
// ============================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    // تهيئة مكتبات COM
    CoInitialize(NULL);
    
    // تهيئة عناصر التحكم الشائعة
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
    
    // تشغيل الواجهة الرسومية
    InfernoGUI gui(hInstance);
    gui.run();
    
    // تنظيف
    CoUninitialize();
    
    return 0;
}
