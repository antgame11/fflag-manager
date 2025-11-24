#include "native.hpp"
#include "constants.hpp"
#include "memory/memory.hpp"
#include "fflags/fflags.hpp"
#include "engine/engine.hpp"

// Required for JSON parsing the list
#include <nlohmann/json.hpp> 

#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <conio.h> 
#include <vector>
#include <fstream>
#include <algorithm> // for transform (lowercase)

// --- VISUAL CONFIG ---
#define COLOR_DEFAULT 7
#define COLOR_GREEN   10
#define COLOR_CYAN    11
#define COLOR_RED     12
#define COLOR_YELLOW  14
#define COLOR_WHITE   15

void SetColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void ClearScreen() {
    COORD topLeft = { 0, 0 };
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO screen;
    DWORD written;
    GetConsoleScreenBufferInfo(console, &screen);
    FillConsoleOutputCharacterA(console, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
    FillConsoleOutputAttribute(console, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE, screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
    SetConsoleCursorPosition(console, topLeft);
}

void DrawHeader() {
    SetColor(COLOR_CYAN);
    std::cout << R"(
   __  __      _                           _ 
  |  \/  | ___| |_  __ _   __ __  _ _  ___| |
  | |\/| |/ -_)  _|/ _` | / _/ _|| '_|/ -_)_|
  |_|  |_|\___|\__|\__,_| \__\__||_|  \___(_)
       FFLAG MANAGER [V4 - UNIVERSAL]
)" << std::endl;
    SetColor(COLOR_DEFAULT);
    std::cout << "========================================" << std::endl;
}

// Helper: Converts "True" -> 1, "False" -> 0, "50" -> 50
int ParseInputToValue(std::string input) {
    // Convert to lowercase
    std::transform(input.begin(), input.end(), input.begin(), ::tolower);
    
    if (input == "true" || input == "on" || input == "yes") return 1;
    if (input == "false" || input == "off" || input == "no") return 0;
    
    // Try parse number
    try {
        return std::stoi(input);
    } catch (...) {
        return 0; // Default error val
    }
}

// Helper: Detect if Roblox is running
bool WaitForRoblox() {
    odessa::g_memory = std::make_unique<odessa::c_memory>(odessa::constants::client_name);
    return odessa::g_memory->pid() != 0;
}

// Helper: Keybinder with ESC support
int CaptureKey() {
    while (_kbhit()) _getch();
    while (true) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) return 0; // Cancel
        for (int i = 1; i < 255; i++) {
            if (i == VK_LBUTTON || i == VK_RBUTTON) continue; 
            if (GetAsyncKeyState(i) & 0x8000) {
                while (GetAsyncKeyState(i) & 0x8000) Sleep(5); 
                return i;
            }
        }
        Sleep(10);
    }
}

// Helper: Check for fflags.json and load keys
std::vector<std::string> GetJsonKeys() {
    std::vector<std::string> keys;
    std::ifstream file("fflags.json");
    
    if (!file.good()) {
        // CREATE FILE IF MISSING
        std::ofstream outfile("fflags.json");
        outfile << "{\n";
        outfile << "  \"S2PhysicsSenderRate\": 15,\n";
        outfile << "  \"DFFlagDebugPerfMode\": \"True\",\n";
        outfile << "  \"TaskSchedulerLimitTargetFpsTo2402\": 9999\n";
        outfile << "}";
        outfile.close();
        keys.push_back("S2PhysicsSenderRate");
        keys.push_back("DFFlagDebugPerfMode");
        keys.push_back("TaskSchedulerLimitTargetFpsTo2402");
        return keys;
    }

    try {
        nlohmann::json j;
        file >> j;
        for (auto& element : j.items()) {
            keys.push_back(element.key());
        }
    } catch (...) {
        keys.push_back("ERROR_READING_JSON");
    }
    return keys;
}

std::int32_t main()
{
    SetConsoleTitleA("FFLAG MANAGER V4 - UNIVERSAL");

    // 1. CONNECT
    DrawHeader();
    std::cout << "[*] Waiting for Roblox..." << std::endl;
    while (!WaitForRoblox()) Sleep(1000);
    SetColor(COLOR_GREEN);
    std::cout << "[+] Attached (PID: " << odessa::g_memory->pid() << ")" << std::endl;
    SetColor(COLOR_DEFAULT);

    // 2. INIT ENGINE
    odessa::engine::g_fflags = std::make_unique<odessa::engine::c_fflags>();
    
    // Run the engine setup first to Apply defaults
    std::cout << "[*] Applying defaults from JSON..." << std::endl;
    odessa::engine::setup();
    Sleep(1000);

    while (true) {
        ClearScreen();
        DrawHeader();

        // 3. READ JSON & DISPLAY MENU
        std::vector<std::string> flagList = GetJsonKeys();
        
        std::cout << "Select a Flag to Bind:" << std::endl;
        SetColor(COLOR_YELLOW);
        for (size_t i = 0; i < flagList.size(); i++) {
            std::cout << " [" << (i + 1) << "] " << flagList[i] << std::endl;
        }
        std::cout << " [R] Reload List / Re-scan File" << std::endl;
        SetColor(COLOR_DEFAULT);
        
        std::cout << "\nInput Number > ";
        std::string inputChoice;
        std::cin >> inputChoice;

        if (inputChoice == "r" || inputChoice == "R") continue; // Refresh

        // Validate Choice
        int idx = 0;
        try {
            idx = std::stoi(inputChoice) - 1;
        } catch (...) { idx = -1; }

        if (idx < 0 || idx >= flagList.size()) {
            std::cout << "Invalid Selection!" << std::endl; Sleep(500); continue;
        }

        std::string targetFlag = flagList[idx];

        // 4. VERIFY FLAG EXISTS IN MEMORY
        auto flag = odessa::engine::g_fflags->find(targetFlag);
        if (!flag) {
            SetColor(COLOR_RED);
            std::cout << "\n[-] Flag '" << targetFlag << "' not found in memory." << std::endl;
            std::cout << "    Wait for game to load or check spelling." << std::endl;
            SetColor(COLOR_DEFAULT);
            Sleep(2500);
            continue;
        }

        // 5. CONFIGURATION
        std::string s_valNormal, s_valActive;
        
        // Clear cin buffer
        std::cin.ignore();

        std::cout << "\nTarget: ";
        SetColor(COLOR_CYAN); std::cout << targetFlag << std::endl; SetColor(COLOR_DEFAULT);
        
        std::cout << "Enter NORMAL/OFF Value (e.g. 15, True, False): ";
        std::getline(std::cin, s_valNormal);
        
        std::cout << "Enter ACTIVE/ON Value (e.g. 1, False, True): ";
        std::getline(std::cin, s_valActive);

        int valNormal = ParseInputToValue(s_valNormal);
        int valActive = ParseInputToValue(s_valActive);

        // 6. KEY BIND
        std::cout << "\nPress any Key to bind (ESC to cancel)..." << std::endl;
        int key = CaptureKey();
        if (key == 0) continue;

        // 7. MODE
        std::cout << "Mode: [1] Toggle | [2] Hold > ";
        char c_mode = _getch();
        bool useHold = (c_mode == '2');

        // 8. ACTIVE LOOP
        ClearScreen();
        DrawHeader();
        std::cout << "ACTIVE BIND: " << targetFlag << std::endl;
        std::cout << "KEY: " << key << " | TYPE: " << (useHold ? "HOLD" : "TOGGLE") << std::endl;
        std::cout << "PANIC: [END] to Reset & Exit" << std::endl;
        
        bool active = false;
        bool lastKey = false;
        
        flag.set(valNormal); // Reset first

        while (true) {
            static bool uiUpdate = true;
            
            // Panic
            if (GetAsyncKeyState(VK_END) & 0x8000) {
                flag.set(valNormal);
                return 0; 
            }
            
            bool keyPress = (GetAsyncKeyState(key) & 0x8000) != 0;
            bool stateChanged = false;

            if (useHold) {
                if (keyPress != active) {
                    active = keyPress;
                    stateChanged = true;
                }
            } else {
                if (keyPress && !lastKey) {
                    active = !active;
                    stateChanged = true;
                }
            }
            lastKey = keyPress;

            if (stateChanged) {
                if (active) {
                    flag.set(valActive);
                    Beep(600, 50);
                } else {
                    flag.set(valNormal);
                    Beep(1000, 50);
                }
                uiUpdate = true;
            }

            if (uiUpdate) {
                std::cout << "\rSTATUS: ";
                if (active) {
                    SetColor(COLOR_RED);
                    std::cout << ">>> ON  <<< (" << s_valActive << ")     "; 
                } else {
                    SetColor(COLOR_GREEN);
                    std::cout << "    OFF     (" << s_valNormal << ")     "; 
                }
                SetColor(COLOR_DEFAULT);
                uiUpdate = false;
            }
            Sleep(10);
        }
    }
    return 0;
}
