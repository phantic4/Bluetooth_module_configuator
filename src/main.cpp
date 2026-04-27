#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <cwctype>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

constexpr UINT WM_SERIAL_DATA = WM_APP + 1;

constexpr int APP_ID_PORT = 1001;
constexpr int APP_ID_CONNECT_BAUD = 1002;
constexpr int APP_ID_CONNECT = 1003;
constexpr int APP_ID_DISCONNECT = 1004;
constexpr int APP_ID_MODULE = 1005;
constexpr int APP_ID_NAME = 1006;
constexpr int APP_ID_PIN = 1007;
constexpr int APP_ID_SET_BAUD = 1008;
constexpr int APP_ID_ROLE = 1009;
constexpr int APP_ID_TEST = 1010;
constexpr int APP_ID_VERSION = 1011;
constexpr int APP_ID_SET_NAME = 1012;
constexpr int APP_ID_SET_BAUD_BUTTON = 1013;
constexpr int APP_ID_SET_ROLE = 1014;
constexpr int APP_ID_SET_PIN = 1015;
constexpr int APP_ID_RESET = 1016;
constexpr int APP_ID_RAW = 1017;
constexpr int APP_ID_RAW_SEND = 1018;
constexpr int APP_ID_LOG = 1019;
constexpr int APP_ID_CLEAR = 1020;

COLORREF kBg = RGB(0, 0, 0);
COLORREF kPanel = RGB(1, 3, 6);
COLORREF kPanel2 = RGB(1, 2, 5);
COLORREF kText = RGB(198, 90, 255);
COLORREF kMuted = RGB(170, 205, 230);
COLORREF kBlue = RGB(0, 190, 255);
COLORREF kOrange = RGB(255, 126, 36);
COLORREF kPurple = RGB(198, 90, 255);
COLORREF kRed = RGB(255, 83, 83);
COLORREF kEdit = RGB(0, 0, 0);

HINSTANCE gInstance = nullptr;
HFONT gFont = nullptr;
HFONT gTitleFont = nullptr;
HBRUSH gBgBrush = nullptr;
HBRUSH gPanelBrush = nullptr;
HBRUSH gEditBrush = nullptr;
HWND gMain = nullptr;
HWND gLog = nullptr;
std::mutex gLogMutex;

std::wstring widen(const std::string& value) {
    if (value.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

std::string narrow(const std::wstring& value) {
    if (value.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring textOf(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    std::wstring value(len + 1, L'\0');
    GetWindowTextW(hwnd, value.data(), len + 1);
    value.resize(len);
    return value;
}

int intOf(HWND hwnd, int fallback) {
    std::wstring value = textOf(hwnd);
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

void setFont(HWND hwnd, HFONT font = nullptr) {
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : gFont), TRUE);
}

HMENU controlId(int id) {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

void appendLog(const std::wstring& line, COLORREF = kText) {
    std::lock_guard<std::mutex> lock(gLogMutex);
    if (!gLog) return;
    int len = GetWindowTextLengthW(gLog);
    SendMessageW(gLog, EM_SETSEL, len, len);
    std::wstring text = line + L"\r\n";
    SendMessageW(gLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(gLog, EM_SCROLLCARET, 0, 0);
}

std::wstring lastErrorText(DWORD code = GetLastError()) {
    wchar_t* buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message = buffer ? buffer : L"unknown error";
    if (buffer) LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r' || message.back() == L' ')) message.pop_back();
    return message;
}

std::wstring normalizePort(std::wstring port) {
    port.erase(std::remove_if(port.begin(), port.end(), [](wchar_t ch) { return iswspace(ch) != 0; }), port.end());
    if (port.rfind(LR"(\\.\)", 0) == 0) return port;
    return LR"(\\.\)" + port;
}

class SerialPort {
public:
    bool open(const std::wstring& port, DWORD baud, HWND notify, std::wstring& error) {
        close();
        notifyWindow = notify;
        handle = CreateFileW(normalizePort(port).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            error = lastErrorText();
            return false;
        }

        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(handle, &dcb)) {
            error = lastErrorText();
            close();
            return false;
        }
        dcb.BaudRate = baud;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary = TRUE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;

        if (!SetCommState(handle, &dcb)) {
            error = lastErrorText();
            close();
            return false;
        }

        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = 30;
        timeouts.ReadTotalTimeoutConstant = 30;
        timeouts.ReadTotalTimeoutMultiplier = 5;
        timeouts.WriteTotalTimeoutConstant = 1000;
        timeouts.WriteTotalTimeoutMultiplier = 10;
        SetCommTimeouts(handle, &timeouts);
        PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

        running = true;
        reader = std::thread([this] { readLoop(); });
        return true;
    }

    void close() {
        running = false;
        if (reader.joinable()) reader.join();
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }

    bool isOpen() const {
        return handle != INVALID_HANDLE_VALUE;
    }

    bool write(const std::string& data, std::wstring& error) {
        if (handle == INVALID_HANDLE_VALUE) {
            error = L"serial port is not connected";
            return false;
        }
        DWORD written = 0;
        BOOL ok = WriteFile(handle, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
        if (!ok || written != data.size()) {
            error = lastErrorText();
            return false;
        }
        return true;
    }

private:
    void readLoop() {
        char buffer[256];
        while (running) {
            DWORD read = 0;
            if (handle == INVALID_HANDLE_VALUE) break;
            BOOL ok = ReadFile(handle, buffer, sizeof(buffer), &read, nullptr);
            if (ok && read > 0) {
                auto* data = new std::string(buffer, buffer + read);
                PostMessageW(notifyWindow, WM_SERIAL_DATA, 0, reinterpret_cast<LPARAM>(data));
            } else {
                Sleep(10);
            }
        }
    }

    HANDLE handle = INVALID_HANDLE_VALUE;
    std::thread reader;
    std::atomic_bool running = false;
    HWND notifyWindow = nullptr;
};

SerialPort gSerial;

enum class Profile { Hm10, Hc05, Hc06, Generic };

enum class Action { Test, Version, SetName, SetBaud, SetRole, SetPin, Reset, Raw };

Profile currentProfile() {
    HWND combo = GetDlgItem(gMain, APP_ID_MODULE);
    int index = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (index == 1) return Profile::Hc05;
    if (index == 2) return Profile::Hc06;
    if (index == 3) return Profile::Generic;
    return Profile::Hm10;
}

std::string lineEnding(Profile profile) {
    if (profile == Profile::Hc05 || profile == Profile::Generic) return "\r\n";
    return "";
}

std::string hm10BaudCode(int baud) {
    switch (baud) {
    case 9600: return "0";
    case 19200: return "1";
    case 38400: return "2";
    case 57600: return "3";
    case 115200: return "4";
    default: return "0";
    }
}

std::string hc06BaudCode(int baud) {
    switch (baud) {
    case 1200: return "1";
    case 2400: return "2";
    case 4800: return "3";
    case 9600: return "4";
    case 19200: return "5";
    case 38400: return "6";
    case 57600: return "7";
    case 115200: return "8";
    default: return "4";
    }
}

std::string selectedRoleCode(Profile profile) {
    HWND role = GetDlgItem(gMain, APP_ID_ROLE);
    int index = static_cast<int>(SendMessageW(role, CB_GETCURSEL, 0, 0));
    bool central = index == 1;
    if (profile == Profile::Hc05 || profile == Profile::Generic) return central ? "1" : "0";
    return central ? "1" : "0";
}

std::string buildCommand(Action action, bool& supported, std::wstring& label) {
    supported = true;
    Profile profile = currentProfile();
    std::string end = lineEnding(profile);
    std::string name = narrow(textOf(GetDlgItem(gMain, APP_ID_NAME)));
    std::string pin = narrow(textOf(GetDlgItem(gMain, APP_ID_PIN)));
    int baud = intOf(GetDlgItem(gMain, APP_ID_SET_BAUD), 9600);
    std::string role = selectedRoleCode(profile);

    if (action == Action::Raw) {
        label = L"Raw";
        return narrow(textOf(GetDlgItem(gMain, APP_ID_RAW))) + end;
    }

    if (profile == Profile::Hm10) {
        switch (action) {
        case Action::Test: label = L"HM-10 test"; return "AT";
        case Action::Version: label = L"HM-10 version"; return "AT+VERS?";
        case Action::SetName: label = L"HM-10 set name"; return "AT+NAME" + name;
        case Action::SetBaud: label = L"HM-10 set baud"; return "AT+BAUD" + hm10BaudCode(baud);
        case Action::SetRole: label = L"HM-10 set role"; return "AT+ROLE" + role;
        case Action::SetPin: label = L"HM-10 set password"; return "AT+PASS" + pin;
        case Action::Reset: label = L"HM-10 reset"; return "AT+RESET";
        default: break;
        }
    }

    if (profile == Profile::Hc05) {
        switch (action) {
        case Action::Test: label = L"HC-05 test"; return "AT" + end;
        case Action::Version: label = L"HC-05 version"; return "AT+VERSION?" + end;
        case Action::SetName: label = L"HC-05 set name"; return "AT+NAME=" + name + end;
        case Action::SetBaud: label = L"HC-05 set UART"; return "AT+UART=" + std::to_string(baud) + ",1,0" + end;
        case Action::SetRole: label = L"HC-05 set role"; return "AT+ROLE=" + role + end;
        case Action::SetPin: label = L"HC-05 set password"; return "AT+PSWD=" + pin + end;
        case Action::Reset: label = L"HC-05 reset"; return "AT+RESET" + end;
        default: break;
        }
    }

    if (profile == Profile::Hc06) {
        switch (action) {
        case Action::Test: label = L"HC-06 test"; return "AT";
        case Action::Version: label = L"HC-06 version"; return "AT+VERSION";
        case Action::SetName: label = L"HC-06 set name"; return "AT+NAME" + name;
        case Action::SetBaud: label = L"HC-06 set baud"; return "AT+BAUD" + hc06BaudCode(baud);
        case Action::SetRole: supported = false; label = L"HC-06 role"; return "";
        case Action::SetPin: label = L"HC-06 set PIN"; return "AT+PIN" + pin;
        case Action::Reset: label = L"HC-06 reset"; return "AT+RESET";
        default: break;
        }
    }

    switch (action) {
    case Action::Test: label = L"Generic test"; return "AT" + end;
    case Action::Version: label = L"Generic version"; return "AT+VERSION?" + end;
    case Action::SetName: label = L"Generic set name"; return "AT+NAME=" + name + end;
    case Action::SetBaud: label = L"Generic set baud"; return "AT+BAUD=" + std::to_string(baud) + end;
    case Action::SetRole: label = L"Generic set role"; return "AT+ROLE=" + role + end;
    case Action::SetPin: label = L"Generic set PIN"; return "AT+PIN=" + pin + end;
    case Action::Reset: label = L"Generic reset"; return "AT+RESET" + end;
    default: break;
    }

    supported = false;
    label = L"Unknown";
    return "";
}

void sendAction(Action action) {
    bool supported = false;
    std::wstring label;
    std::string command = buildCommand(action, supported, label);
    if (!supported) {
        appendLog(L"INFO  This preset does not support that setting.", kPurple);
        return;
    }
    if (command.empty()) {
        appendLog(L"INFO  Command is empty.", kPurple);
        return;
    }
    std::wstring error;
    if (!gSerial.write(command, error)) {
        appendLog(L"ERROR " + error, kRed);
        return;
    }
    std::wstring shown = widen(command);
    for (auto& ch : shown) {
        if (ch == L'\r') ch = L' ';
        if (ch == L'\n') ch = L' ';
    }
    appendLog(L"TX    " + label + L": " + shown, kBlue);
}

HWND makeControl(const wchar_t* cls, const wchar_t* text, DWORD style, int id, int x, int y, int w, int h, HWND parent) {
    HWND hwnd = CreateWindowExW(0, cls, text, style | WS_CHILD | WS_VISIBLE, x, y, w, h, parent, controlId(id), gInstance, nullptr);
    setFont(hwnd);
    return hwnd;
}

HWND makeEdit(int id, const wchar_t* text, int x, int y, int w, int h, HWND parent, DWORD extra = 0) {
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | extra, x, y, w, h, parent, controlId(id), gInstance, nullptr);
    setFont(hwnd);
    return hwnd;
}

HWND makeCombo(int id, int x, int y, int w, int h, HWND parent) {
    HWND hwnd = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL, x, y, w, h, parent, controlId(id), gInstance, nullptr);
    setFont(hwnd);
    return hwnd;
}

HWND makeButton(int id, const wchar_t* text, int x, int y, int w, int h, HWND parent) {
    HWND hwnd = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x, y, w, h, parent, controlId(id), gInstance, nullptr);
    setFont(hwnd);
    return hwnd;
}

void addCombo(HWND combo, const wchar_t* value) {
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
}

void selectCombo(HWND combo, int index) {
    SendMessageW(combo, CB_SETCURSEL, index, 0);
}

void label(HWND parent, const wchar_t* text, int x, int y, int w, int h, COLORREF = kMuted) {
    makeControl(L"STATIC", text, WS_CHILD | WS_VISIBLE, -1, x, y, w, h, parent);
}

void createUi(HWND hwnd) {
    makeControl(L"STATIC", L"Bluetooth Module AT Configurator", WS_CHILD | WS_VISIBLE, -1, 24, 18, 520, 34, hwnd);
    setFont(GetWindow(hwnd, GW_CHILD), gTitleFont);
    makeControl(L"STATIC", L"Use a USB-to-TTL adapter to change module settings with preset AT commands.", WS_CHILD | WS_VISIBLE, -1, 26, 54, 760, 24, hwnd);

    label(hwnd, L"COM port", 34, 105, 130, 22);
    makeEdit(APP_ID_PORT, L"COM3", 34, 128, 150, 28, hwnd);
    label(hwnd, L"Connect baud", 202, 105, 150, 22);
    HWND connectBaud = makeCombo(APP_ID_CONNECT_BAUD, 202, 128, 150, 180, hwnd);
    for (auto baud : { L"9600", L"19200", L"38400", L"57600", L"115200" }) addCombo(connectBaud, baud);
    selectCombo(connectBaud, 0);
    makeButton(APP_ID_CONNECT, L"Connect", 370, 126, 118, 34, hwnd);
    makeButton(APP_ID_DISCONNECT, L"Disconnect", 502, 126, 130, 34, hwnd);

    label(hwnd, L"Module preset", 34, 188, 160, 22);
    HWND module = makeCombo(APP_ID_MODULE, 34, 211, 260, 180, hwnd);
    addCombo(module, L"HM-10 / BLE UART");
    addCombo(module, L"HC-05");
    addCombo(module, L"HC-06");
    addCombo(module, L"Generic AT");
    selectCombo(module, 0);

    label(hwnd, L"New name", 318, 188, 160, 22);
    makeEdit(APP_ID_NAME, L"RobotBT", 318, 211, 180, 28, hwnd);
    label(hwnd, L"PIN / password", 522, 188, 160, 22);
    makeEdit(APP_ID_PIN, L"123456", 522, 211, 160, 28, hwnd);

    label(hwnd, L"New baud", 34, 262, 160, 22);
    HWND setBaud = makeCombo(APP_ID_SET_BAUD, 34, 285, 180, 180, hwnd);
    for (auto baud : { L"9600", L"19200", L"38400", L"57600", L"115200" }) addCombo(setBaud, baud);
    selectCombo(setBaud, 0);
    label(hwnd, L"Role", 238, 262, 160, 22);
    HWND role = makeCombo(APP_ID_ROLE, 238, 285, 220, 120, hwnd);
    addCombo(role, L"Peripheral / Slave");
    addCombo(role, L"Central / Master");
    selectCombo(role, 0);

    makeButton(APP_ID_TEST, L"Test AT", 34, 342, 120, 34, hwnd);
    makeButton(APP_ID_VERSION, L"Version", 166, 342, 120, 34, hwnd);
    makeButton(APP_ID_SET_NAME, L"Set Name", 298, 342, 120, 34, hwnd);
    makeButton(APP_ID_SET_BAUD_BUTTON, L"Set Baud", 430, 342, 120, 34, hwnd);
    makeButton(APP_ID_SET_ROLE, L"Set Role", 562, 342, 120, 34, hwnd);
    makeButton(APP_ID_SET_PIN, L"Set PIN", 34, 390, 120, 34, hwnd);
    makeButton(APP_ID_RESET, L"Reset", 166, 390, 120, 34, hwnd);
    makeButton(APP_ID_CLEAR, L"Clear Log", 298, 390, 120, 34, hwnd);

    label(hwnd, L"Raw AT command", 34, 458, 180, 22);
    makeEdit(APP_ID_RAW, L"AT", 34, 482, 450, 28, hwnd);
    makeButton(APP_ID_RAW_SEND, L"Send Raw", 502, 480, 130, 34, hwnd);

    gLog = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                           720, 100, 430, 520, hwnd, controlId(APP_ID_LOG), gInstance, nullptr);
    setFont(gLog);
    appendLog(L"Connect your USB-to-TTL adapter, choose the COM port, then press Connect.", kText);
}

void connectSerial() {
    std::wstring port = textOf(GetDlgItem(gMain, APP_ID_PORT));
    int baud = intOf(GetDlgItem(gMain, APP_ID_CONNECT_BAUD), 9600);
    std::wstring error;
    if (gSerial.open(port, static_cast<DWORD>(baud), gMain, error)) {
        appendLog(L"OK    Connected to " + port + L" at " + std::to_wstring(baud) + L" baud.", kText);
    } else {
        appendLog(L"ERROR Could not open " + port + L": " + error, kRed);
    }
}

void drawButton(const DRAWITEMSTRUCT* item) {
    bool pressed = (item->itemState & ODS_SELECTED) != 0;
    bool disabled = (item->itemState & ODS_DISABLED) != 0;
    RECT r = item->rcItem;

    HBRUSH back = CreateSolidBrush(kPanel);
    FillRect(item->hDC, &r, back);
    DeleteObject(back);

    RECT buttonRect = r;
    InflateRect(&buttonRect, -1, -1);
    COLORREF fill = disabled ? RGB(72, 72, 72) : (pressed ? RGB(210, 86, 18) : kOrange);
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 172, 92));
    HGDIOBJ oldBrush = SelectObject(item->hDC, brush);
    HGDIOBJ oldPen = SelectObject(item->hDC, pen);
    RoundRect(item->hDC, buttonRect.left, buttonRect.top, buttonRect.right, buttonRect.bottom, 8, 8);
    SelectObject(item->hDC, oldBrush);
    SelectObject(item->hDC, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    wchar_t text[128]{};
    GetWindowTextW(item->hwndItem, text, 128);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, RGB(0, 0, 0));
    SelectObject(item->hDC, gFont);
    DrawTextW(item->hDC, text, -1, &buttonRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
void paintBackground(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, gBgBrush);

    HBRUSH panel = CreateSolidBrush(kPanel);
    HBRUSH panel2 = CreateSolidBrush(kPanel2);
    HPEN bluePen = CreatePen(PS_SOLID, 2, kBlue);
    HPEN purplePen = CreatePen(PS_SOLID, 2, kPurple);

    HGDIOBJ oldBrush = SelectObject(hdc, panel);
    HGDIOBJ oldPen = SelectObject(hdc, bluePen);
    RoundRect(hdc, 22, 88, 696, 628, 16, 16);
    SelectObject(hdc, panel2);
    SelectObject(hdc, purplePen);
    RoundRect(hdc, 708, 88, 1164, 628, 16, 16);

    HBRUSH bar = CreateSolidBrush(kBlue);
    RECT bottom{0, rc.bottom - 18, rc.right, rc.bottom};
    FillRect(hdc, &bottom, bar);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(panel);
    DeleteObject(panel2);
    DeleteObject(bluePen);
    DeleteObject(purplePen);
    DeleteObject(bar);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        gMain = hwnd;
        createUi(hwnd);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paintBackground(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, kMuted);
        return reinterpret_cast<LRESULT>(gPanelBrush);
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdc, kEdit);
        SetTextColor(hdc, kText);
        return reinterpret_cast<LRESULT>(gEditBrush);
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdc, kEdit);
        SetTextColor(hdc, kText);
        return reinterpret_cast<LRESULT>(gEditBrush);
    }

    case WM_DRAWITEM:
        drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
        return TRUE;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == APP_ID_CONNECT) connectSerial();
        else if (id == APP_ID_DISCONNECT) { gSerial.close(); appendLog(L"OK    Disconnected.", kMuted); }
        else if (id == APP_ID_TEST) sendAction(Action::Test);
        else if (id == APP_ID_VERSION) sendAction(Action::Version);
        else if (id == APP_ID_SET_NAME) sendAction(Action::SetName);
        else if (id == APP_ID_SET_BAUD_BUTTON) sendAction(Action::SetBaud);
        else if (id == APP_ID_SET_ROLE) sendAction(Action::SetRole);
        else if (id == APP_ID_SET_PIN) sendAction(Action::SetPin);
        else if (id == APP_ID_RESET) sendAction(Action::Reset);
        else if (id == APP_ID_RAW_SEND) sendAction(Action::Raw);
        else if (id == APP_ID_CLEAR) SetWindowTextW(gLog, L"");
        return 0;
    }

    case WM_SERIAL_DATA: {
        auto* data = reinterpret_cast<std::string*>(lParam);
        if (data) {
            std::wstring shown = widen(*data);
            for (auto& ch : shown) {
                if (ch == L'\r') ch = L' ';
                if (ch == L'\n') ch = L' ';
            }
            appendLog(L"RX    " + shown, kText);
            delete data;
        }
        return 0;
    }

    case WM_CLOSE:
        gSerial.close();
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    gInstance = instance;
    INITCOMMONCONTROLSEX icc{sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    gFont = CreateFontW(18, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    gTitleFont = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    gBgBrush = CreateSolidBrush(kBg);
    gPanelBrush = CreateSolidBrush(kPanel);
    gEditBrush = CreateSolidBrush(kEdit);

    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"BluetoothModuleConfiguratorWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = gBgBrush;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Bluetooth Module AT Configurator", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1190, 700, nullptr, nullptr, instance, nullptr);
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(gFont);
    DeleteObject(gTitleFont);
    DeleteObject(gBgBrush);
    DeleteObject(gPanelBrush);
    DeleteObject(gEditBrush);
    return 0;
}







