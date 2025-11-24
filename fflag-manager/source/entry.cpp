#include "native.hpp"
#include "constants.hpp"
#include "memory/memory.hpp"
#include "fflags/fflags.hpp"
#include "engine/engine.hpp"

#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <conio.h> // For _getch()
#include <vector>

// --- ADVANCED GUI CONFIG ---
#define COLOR_DEFAULT 7
#define COLOR_BLUE    9
#define COLOR_GREEN   10
#define COLOR_CYAN    11
#define COLOR_RED     12
#define COLOR_MAGENTA 13
#define COLOR_YELLOW  14

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
   __  __  _                           
  |  \/  |(_)  ___  __ __  ___   _ _ 
  | |\/| || | (_-<  \ V / / -_) | '_|
  |_|  |_||_| /__/   \_/  \___| |_|  
       FFLAG MANAGER [ADVANCED]      
)" << std::endl;
    SetColor(COLOR_DEFAULT);
    std::cout << "========================================" << std::endl;
}

// Returns the VK code, or 0 if aborted
int CaptureKey() {
    // Clear buffer
    while (_kbhit()) _getch();
    
    while (true) {
        // Check for ESC (VK_ESCAPE = 0x1B)
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            return 0; // 0 means aborted
        }

        // Scan common keys
        for (int i = 1; i < 255; i++) {
            // Skip Mouse buttons if you want, but they are useful (VK_LBUTTON=0x01)
            // Lets skip Left/Right click to avoid accidental binds
            if (i == VK_LBUTTON || i == VK_RBUTTON) continue;

            if (GetAsyncKeyState(i) & 0x8000) {
                while (GetAsyncKeyState(i) & 0x8000) Sleep(5); // Wait release
                return i;
            }
        }
        Sleep(10);
    }
}

// Helper to check roblox process
bool WaitForRoblox() {
    odessa::g_memory = std::make_unique<odessa::c_memory>(odessa::constants::client_name);
    return odessa::g_memory->pid() != 0;
}

std::int32_t main()
{
    SetConsoleTitleA("Roblox External - V3.0");

    // --- INIT ---
    DrawHeader();
    std::cout << "[*] Waiting for Roblox..." << std::endl;
    
    while (!WaitForRoblox()) Sleep(1000);
    
    SetColor(COLOR_GREEN);
    std::cout << "[+] Attached to PID: " << odessa::g_memory->pid() << std::endl;
    SetColor(COLOR_DEFAULT);

    odessa::engine::g_fflags = std::make_unique<odessa::engine::c_fflags>();
    
    std::cout << "[*] Reading fflags.json defaults..." << std::endl;
    odessa::engine::setup(); // Apply default values from JSON first

    Sleep(1500);

    // --- MENU LOOP ---
    while (true) {
        ClearScreen();
        DrawHeader();

        // 1. SELECT TARGET
        std::cout << "[1] Network Lag (S2PhysicsSenderRate)" << std::endl;
        std::cout << "[2] FPS Freeze  (TaskSchedulerLimitTargetFps)" << std::endl;
        std::cout << "[3] Exit" << std::endl;
        
        SetColor(COLOR_YELLOW);
        std::cout << "\n> Select Option: ";
        SetColor(COLOR_DEFAULT);
        
        char choice = _getch();
        if (choice == '3') break;

        std::string flagName = "";
        int valNormal = 0;
        int valActive = 0;
        std::string modeName = "";

        if (choice == '1') {
            modeName = "Network Lag";
            flagName = "S2PhysicsSenderRate";
            valNormal = 15; // Standard default
        } 
        else if (choice == '2') {
            modeName = "FPS Freeze";
            flagName = "TaskSchedulerLimitTargetFpsTo2402";
            valNormal = 9999; 
        } 
        else continue;

        // Verify Flag Exists
        auto flag = odessa::engine::g_fflags->find(flagName);
        if (!flag) {
            SetColor(COLOR_RED);
            std::cout << "\n[-] Error: Flag not found in memory." << std::endl;
            Sleep(2000);
            continue;
        }

        // 2. CONFIGURE INTENSITY
        std::cout << "\nTarget: " << flagName << std::endl;
        
        std::cout << "Enter 'Active' Value (e.g. 1 for lag): ";
        std::cin >> valActive;

        // 3. BIND KEY
        std::cout << "\nPress any key to bind (or ESC to cancel)..." << std::endl;
        int bindKey = CaptureKey();
        if (bindKey == 0) continue; // Go back to menu

        // 4. CHOOSE MODE
        std::cout << "Bound to Key Code: " << bindKey << std::endl;
        std::cout << "\nTrigger Mode:\n [1] TOGGLE (Press to on, Press to off)\n [2] HOLD (On while holding key)" << std::endl;
        char trigMode = _getch();
        bool useHoldMode = (trigMode == '2');

        // --- RUNNING STATE ---
        ClearScreen();
        DrawHeader();
        
        std::cout << "Mode: " << modeName << " | Trigger: " << (useHoldMode ? "HOLD" : "TOGGLE") << std::endl;
        std::cout << "PANIC KEY: [END] (Press to Reset & Exit)" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        bool isActive = false;
        bool lastKeyState = false;

        // Ensure default state
        flag.set(valNormal);

        // UI Loop
        while (true) {
            // Handle UI Redraw only on change to avoid flicker
            static bool lastActiveUI = !isActive; 
            
            // Check Panic
            if (GetAsyncKeyState(VK_END) & 0x8000) {
                flag.set(valNormal); // Reset
                SetColor(COLOR_RED);
                std::cout << "\n[!] PANIC ACTIVATED. EXITING..." << std::endl;
                Sleep(1000);
                return 0;
            }

            // Key Logic
            bool keyCurrent = (GetAsyncKeyState(bindKey) & 0x8000) != 0;

            if (useHoldMode) {
                // HOLD LOGIC
                if (keyCurrent != isActive) {
                    isActive = keyCurrent;
                    flag.set(isActive ? valActive : valNormal);
                    if(isActive) Beep(600, 50);
                }
            } 
            else {
                // TOGGLE LOGIC
                if (keyCurrent && !lastKeyState) { // KeyDown
                    isActive = !isActive;
                    flag.set(isActive ? valActive : valNormal);
                    if(isActive) Beep(600, 50); else Beep(1000, 50);
                }
            }
            lastKeyState = keyCurrent;

            // Simple UI Update
            if (isActive != lastActiveUI) {
                std::cout << "\rSTATUS: ";
                if (isActive) {
                    SetColor(COLOR_RED);
                    std::cout << " >> ACTIVE (" << valActive << ") <<    "; 
                } else {
                    SetColor(COLOR_GREEN);
                    std::cout << "    NORMAL (" << valNormal << ")       ";
                }
                SetColor(COLOR_DEFAULT);
                lastActiveUI = isActive;
            }

            Sleep(10);
        }
    }

    return 0;
}
