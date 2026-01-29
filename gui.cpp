#include "src/imgui/imgui.h"
#include "src/imgui/imgui_impl_win32.h"
#include "src/imgui/imgui_impl_dx11.h"
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <array>
#include <format>
#include <fstream>
#include <algorithm>
#include <mmsystem.h>
#include <future>
#include <filesystem>
#include <direct.h>
#include "synthdaw.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "shlwapi.lib")
#include <shlwapi.h>

struct note {
    //0-32
    char tick;
    //0-120
    char note;
    //0 - silent, 1 - active, 2 - continuous
    char state;
    float x;
    float y;
};


struct track {
    int radio = 0;
    std::array<std::string, 1024> ts;
    int total_tacts = 0;
};

float position = 0;
//32*120 = ticks*notes
std::array<note, 32 * 120> notes;
std::array<track, 255> tracks;
const char* note_sign[12] = {"C.", "C+", "D.", "D+", "E.", "F.", "F+", "G.", "G+", "A.", "A+", "B."};
std::string total = "";
ImVec2 first_pos = { 0., 0. };
bool first_use = true;
std::string saved = "";
static std::vector<char> mem_buf;

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
int CreateGUI();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void MainWindow();
void string_to_array(std::string intact, int tact, std::array<note, 32 * 120>& go);
std::string array_to_string(const std::array<note, 32 * 120>& notes, int tact);
std::string save_file_dialog(std::string name, std::string ext);
std::string open_file_dialog();
void load_string(int& total_tacts, std::string& total, std::string buffer, int& tact, int& tempo, int& track);
std::string work_dir;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    //changing working directory
    char wd[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, wd, MAX_PATH);
    PathRemoveFileSpecA(wd);
    work_dir = std::string(wd);
    std::filesystem::current_path(work_dir);
    for (char i = 0; i < 32; i++) {
        for (char j = 0; j < 120; j++) {
            notes[static_cast<std::array<note, 3840Ui64>::size_type>(i * 120 + j)] = { i, j, 0, 0., 0. };
        }
    }
    return CreateGUI();
}

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if(pBackBuffer != NULL)g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

std::string save_file_dialog(std::string name, std::string ext) {
    OPENFILENAMEA ofn;
    char szFileName[MAX_PATH] = "";

    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    std::string nameext = (name + "(*." + ext + ")\0*."+"\0All Files (*.*)\0*.*\0");
    ofn.lpstrFilter = nameext.c_str();
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = ext.c_str();

    if (GetSaveFileNameA(&ofn) == TRUE) {
        return szFileName;
    }
    else {
        return "None";
    }
}

std::string open_file_dialog() {
    OPENFILENAMEA ofn;
    char szFileName[MAX_PATH] = "";

    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "txt";

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return szFileName;
    }
    else {
        return "None";
    }
}

void load_string(int& total_tracks, std::string& total, std::string buffer, int& tact, int& tempo, int& track) {
    for (int i = 0; i < 32 * 120; i++) {
        notes[i].state = 0;
    }
    size_t pos = 0;
    total_tracks = 0;
    total = buffer;

    tempo = stoi(total.substr(0, 3));
    std::string formed = std::string(buffer).substr(3, buffer.length() - 3);
    std::cout << formed << std::endl;
    std::string to_track = "";
    while (pos < formed.length()-3) {
        while (formed[pos] != 'E') {
            to_track += formed[pos];
            pos++;
        }
        if(formed[pos] == 'E' && to_track != "") {
            tact = 0;
            std::string to_tact = "";
            tracks[track].radio = stoi(to_track.substr(0,3));
            to_track = to_track.substr(6, to_track.length());
            std::cout << to_track << std::endl;
            for (int i = 0; i < to_track.length() + 1; i++) {
                if (to_track[i] == 'T' || i == to_track.length()) {
                    string_to_array(to_tact, tact, notes);
                    tracks[track].ts[tact] = array_to_string(notes, tact);
                    tact++;
                    //total_tacts++;
                    if (i < to_track.length() - 1) {
                        i += 2;
                    }
                    to_tact = "";
                    continue;
                }
                else {
                    to_tact += to_track[i];
                }
            }
            track++;
            pos += 3;
            to_track = "";
        }
        std::cout << pos << formed.length()<< std::endl;
    }
    track = 0;
    tact = 0;
    string_to_array(tracks[0].ts[0], tact, notes);
}

void MainWindow() {
    if(!mem_buf.empty())
        mem_buf.clear();
    ImGui::Begin("Window", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);


    static int tempo = 120;
    static int ctrack = 0;
    static std::string progress = "Waiting for render";
    static int tact = 0;
    static int instrument_radio = 0;
    int total_tracks = 0;
    ImGuiIO& io = ImGui::GetIO();

    tracks[ctrack].radio = instrument_radio;

    ImGui::Text("Choose instrument:");
    ImGui::RadioButton("Sine wave", &instrument_radio, 0);
    ImGui::SameLine(); ImGui::RadioButton("Saw wave", &instrument_radio, 1);
    ImGui::SameLine(); ImGui::RadioButton("Noise 32767 bit", &instrument_radio, 2);
    ImGui::RadioButton("Drum Kit", &instrument_radio, 3);
    ImGui::SameLine(); ImGui::RadioButton("Square wave", &instrument_radio, 4);
    ImGui::SameLine(); ImGui::RadioButton("Triangle wave", &instrument_radio, 5);
    ImGui::Text("Tempo:"); ImGui::SameLine(); ImGui::SetNextItemWidth(250.0f); ImGui::SliderInt(" ", &tempo, 100, 200, "%d", ImGuiSliderFlags_ClampOnInput);
    if (ImGui::Button("Save Project to .txt")) {
        std::string filename = save_file_dialog("Text File", "txt");
        std::ofstream MyFile(filename);
        if (MyFile.is_open()) {
            MyFile << total;
            MyFile.close();
        }
    }
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::Button("Load Project from .txt")) {
        std::string filename = open_file_dialog();
        std::ifstream MyFile(filename);
        std::string line;
        if (MyFile.is_open()) {
            std::getline(MyFile, line);
            load_string(total_tracks, total, line, tact, tempo, ctrack);
            MyFile.close();
        }
    }
    if (ImGui::Button("Play (not works)")) {
        //use another library (winapi sucks TOO MUCH)
        //sndPlaySoundA(NULL, 0);
        //create_sound("test", total, instrument_radio, true, mem_buf);
        //static LPCSTR data = mem_buf.data();
        //PlaySoundA(data, NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        sndPlaySoundA(NULL, 0);
    }
    if (ImGui::Button("Render .wav")) {
        std::string filename = save_file_dialog("WAVE file", "wav");
        //strange but interesting
        std::future<void> func_res = std::async(std::launch::async, create_sound, filename, total, false, std::ref(mem_buf), std::ref(progress));
    }
    ImGui::SameLine();
    ImGui::Text(progress.c_str());
    if (ImGui::Button("Save to clipboard")) {
        ImGui::SetClipboardText(total.c_str());
    }
    ImGui::Text("Tact: %d", tact);
    if (ImGui::Button("Prev Tact")) {
        (tact > 0) ? tact-- : tact = 0;
        string_to_array(tracks[ctrack].ts[tact], tact, notes);
    }
    ImGui::SameLine();
    if (ImGui::Button("Next Tact")) {
        (tact < 1024) ? tact++ : tact = 1023;
        string_to_array(tracks[ctrack].ts[tact], tact, notes);
    }
    ImGui::Text("Track: %d", ctrack);
    if (ImGui::Button("Prev Track")) {
        (ctrack > 0) ? ctrack-- : ctrack = 0;
        instrument_radio = tracks[ctrack].radio;
        string_to_array(tracks[ctrack].ts[tact], tact, notes);
    }
    ImGui::SameLine();
    if (ImGui::Button("Next Track")) {
        (ctrack < 255) ? ctrack++ : ctrack = 254;
        instrument_radio = tracks[ctrack].radio;
        string_to_array(tracks[ctrack].ts[tact], tact, notes);
    }

    ImVec2 mouse = ImGui::GetMousePos();
    bool on_screen = false;
    float change = io.MouseWheel;
    int cn = 0, ct = 0, st = 0;
    {
        if (first_use) {
            ImGui::SetNextWindowSize(ImVec2(600, 450));
            first_use = false;
        }
        ImGui::Begin("Piano Roll");
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        mouse.x -= wp.x;
        mouse.y -= wp.y;
        if (mouse.x > 0 && mouse.x < ws.x && mouse.y > 0 && mouse.y < ws.y) {
            on_screen = true;
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
                first_pos.x += change * 30;
            }
            else {
                first_pos.y += change * 30;
            }
        }
        for (char i = 0; i < 32; i++) {
            for (char j = 0; j < 120; j++) {
                int cur = i * 120 + j;
                ImU32 color = (0, 0, 0, 255);
                switch (notes[cur].state) {
                case 0:
                    if(i%8 >= 4)
                        color = IM_COL32(30, 30, 30, 255);
                    else
                        color = IM_COL32(60, 60, 60, 255);
                    break;
                case 1:
                    color = IM_COL32(58, 146, 158, 255);
                    break;
                case 2:
                    color = IM_COL32(64, 160, 56, 255);
                    break;
                }
                drawList->AddRectFilled(ImVec2(notes[cur].x + wp.x + 1, notes[cur].y - 25 + wp.y + 25 + 1), ImVec2(notes[cur].x + 25 + wp.x - 1, notes[cur].y + wp.y + 25 - 1), color);
                notes[cur].x = first_pos.x + (25 * i);
                notes[cur].y = first_pos.y + (25 * j) + 25;
                if (notes[cur].x <= mouse.x && notes[cur].y <= mouse.y) {
                    if (notes[cur].x + 25 > mouse.x && notes[cur].y + 25 > mouse.y && on_screen) {
                        cn = notes[cur].note;
                        ct = notes[cur].tick;
                        st = notes[cur].state;
                        std::string text = note_sign[cn % 12] + std::to_string(cn/12) + std::string("\nt:") + std::to_string(ct);
                        drawList->AddText(ImVec2(notes[cur].x + wp.x, notes[cur].y + wp.y), IM_COL32(255, 255, 255, 255), (text.c_str()));
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            (notes[cur].state != 0) ? notes[cur].state = 0 : notes[cur].state = 1;
                        }
                        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                            (notes[cur].state != 0) ? notes[cur].state = 0 : notes[cur].state = 2;
                        }
                    }
                }
            }
        }
        ImGui::End();
    }
    total = "";
    total += std::to_string(tempo);
    tracks[ctrack].ts[tact] = array_to_string(notes, tact);

    //recreate in future (not deleting tacts/track if there is no notes in tact/track)
    for (int t = total_tracks; t < 255; t++) {
        if (tracks[t].total_tacts != 0) {
            total_tracks = t+1;
        }
    }
    for (int t = 0; t < total_tracks+1; t++) {
        if (t != total_tracks) {
            std::string new_instr = std::format("{:03}", tracks[t].radio);
            total += new_instr;
        }
        for (int i = tracks[t].total_tacts; i < 1024; i++) {
            if (tracks[t].ts[i] != "") {
                tracks[t].total_tacts = i + 1;
            }
        }
        for (int i = 0; i < tracks[t].total_tacts; i++) {
            total += "TCT";
            total += tracks[t].ts[i];
        }
        if (t != total_tracks)
            total += "END";
    }
    total += "EOF";
    ImGui::Text("mouse x: %f\nmouse y: %f\non screen: %d\nnote, tick:{%s%d, %d}\nstate: %d\ntotal tracks: %d\ntotal tacts: %d", mouse.x, mouse.y, on_screen, note_sign[cn % 12], cn / 12, ct, st, total_tracks, tracks[ctrack].total_tacts);
    ImGui::Text("Application in development! Use custom\nstrings at your own risk! They can\ncrash application with wrong formatting!\n(in drum kit)\nkick C0, clap C0+, hihat D0, snare D0+");
    ImGui::PushTextWrapPos(320);
    ImGui::TextWrapped("Text: %s", total.c_str());
    
    ImGui::End();
}

void string_to_array(std::string intact, int tact, std::array<note, 32 * 120>& go){
    for (int i = 0; i < 32 * 120; i++) {
        go[i].state = 0;
    }
    for (int i = 0; i < intact.length(); i++) {
        int note = 0, tick = 0, dur = 0;
        std::string current = intact.substr(i, 7);
        //note
        switch (current[0]) {
        case 'C': note = 0; break;
        case 'D': note = 2; break;
        case 'E': note = 4; break;
        case 'F': note = 5; break;
        case 'G': note = 7; break;
        case 'A': note = 9; break;
        case 'B': note = 11; break;
        }
        if (current[2] == '+') {
            note++;
        }
        note += (12 * (current[1] - '0'));
        //tick+dur
        tick = stoi(current.substr(3, 2));
        dur = stoi(current.substr(5, 2));

        go[(tick * 120) + note].state = 1;
        if (dur > 1) {
            for (int j = 1; j < dur; j++) {
                go[((tick + j) * 120) + note].state = 2;
            }
        }
        i += 6;
        continue;
    }
}

std::string array_to_string(const std::array<note, 32 * 120>& notes, int tact) {
    int chord = 0;
    std::string result = "";
    std::array<std::string, 32> tick_strings;

    for (int i = 0; i < 32; i++) {
        tick_strings[i] = "";
        chord = 0;
        for (int j = 0; j < 120; j++) {
            int cur = i * 120 + j;
            if (notes[cur].state == 1) {
                chord++;
                int longitude = 1;
                int next = cur + 120;
                while (true) {
                    if (next < 32 * 120) {
                        if (notes[next].state == 2) {
                            longitude++;
                            next += 120;
                        }
                        else {
                            break;
                        }
                    }
                    else {
                        break;
                    }
                }
                std::string formated = std::format("{:02}{:02}", ((int)(notes[cur].tick)), (int)longitude);
                std::string state = note_sign[notes[cur].note % 12][0] + std::to_string(notes[cur].note / 12) + note_sign[notes[cur].note % 12][1] + formated; 
                tick_strings[i] += state;
            }
        }
        result += tick_strings[i];
    }
    return result;
}

int CreateGUI() {

    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    RegisterClassExW(&wc);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"SynthDaw v0.0.3 alpha | 256 track!! DRUMS!!! NEW INSTRUMENTS!!! 35MB of RAM app using omg!", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 200, 200, screenWidth/2, screenHeight/2, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    //mainloop
    bool stopped = false;
    bool show_main = true;
    while (!stopped) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                stopped = true;
        }
        if (stopped)
            break;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (show_main) {
            ImGui::SetNextWindowSize(ImVec2(screenWidth/2, screenHeight/2));
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            MainWindow();
        }

        ImGui::Render();

        const float clear_color[4] = { 0, 0, 0, 0 };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}