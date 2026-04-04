
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#include <winbase.h>
#include <math.h>
#include<cmath>
#include <tchar.h>
#include <vector> // 用于存储多警报文本
#include <random>  // 用于随机地障生成
#include <mmsystem.h>
#include<easyx.h>

#pragma comment(lib, "winmm.lib")

#include "framework.h"
#include "WindowsProject1.h"

#define MAX_LOADSTRING 100

// 补充缺失宏定义
#ifndef MWT_IDENTITY
#define MWT_IDENTITY 1
#endif
#ifndef MWT_LEFTMULTIPLY
#define MWT_LEFTMULTIPLY 2
#endif
#ifndef MWT_SET
#define MWT_SET 3
#endif
#ifndef GM_ADVANCED
#define GM_ADVANCED 2
#endif

// 全局变量
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
HWND g_hMainWnd = nullptr; // 主窗口句柄

// 飞机参数
int planeX = 400;
int planeY = 300;
double planeAngle = 0;
int speed = 5;
// 地障结构
struct Obstacle {
    int x;
    int y;
    int width=0;   // 随机宽度
    int height=0;  // 随机高度
    COLORREF color;
};
std::vector<Obstacle> obstacles;  // 地障集合
const int OBSTACLE_WIDTH = 15;        // 地障宽度（细，15像素）
const int OBSTACLE_MIN_HEIGHT = 10;           // 地障最小高度（10像素）
const int OBSTACLE_MAX_HEIGHT = 120;           // 地障最大高度（40像素）
const int OBSTACLE_GROUND_GAP = 0;   // 地障底部离地面的空隙（50像素）
const int OBSTACLE_COUNT = 5;         // 地障数量
int flyRightCount = 0;                      // 飞过右侧的次数
const int TRIGGER_COUNT = 7;                // 触发阈值（7次）
bool isRunwayGenerated = false;             // 是否已生成跑道+左侧地障
// 停机跑道参数
const int RUNWAY_WIDTH = 150;               // 跑道宽度
const int RUNWAY_HEIGHT = 8;                // 跑道高度（低，视觉上像跑道）
const int RUNWAY_X = 50;                    // 跑道X坐标（左侧）
const int RUNWAY_COLOR_R = 100;             // 跑道颜色（灰色）
const int RUNWAY_COLOR_G = 100;
const int RUNWAY_COLOR_B = 100;
bool isCrashed = false;  // 坠毁状态标记
// 飞机参数下方新增
bool isStall = false;          // 失速状态标记
const double STALL_DESCENT_SPEED = 8.0;  // 失速时的下坠速度（可自己调整数值）

// 全局变量区域添加（比如在STALL_ANGLE前面）
#ifndef M_PI
#define M_PI 3.14159265358979323846  // 手动定义π的精确值
#endif
const double STALL_ANGLE = M_PI / 10;  // 失速迎角阈值（18°转换为弧度）
const int STALL_SPEED_THRESHOLD = 4;   // 失速速度阈值（速度<4时触发警报）
const double SPEED_DECAY_RATE = 0.1;   // 迎角超18°时的速度衰减率
bool isAngleOverLimit;
double absAngle;
const double STALL_DESCENT_RATE = 16.0;  // 基础下降速度（像素/帧）
const double MAX_DESCENT_RATE = 5.0;    // 最大下降速度（避免下坠过快）
double currentDescentSpeed = 0.0;       // 当前实际下降速度

// 警报相关
bool isGroundAlarm = false;    // 优先级1（最高）：近地
bool isObstacleAlarm = false;    // 优先级1.5：接近地障（铁塔/建筑）
bool isSpeedAlarm = false;     // 优先级2：速度
bool isBorderAlarm = false;    // 优先级3（最低）：边界
std::vector<WCHAR*> alarmTexts;// 存储多警报文本（按优先级排序）
std::vector<COLORREF> alarmColors; // 对应警报文本颜色
DWORD lastAlarmTime = 0;       // 上次播放警报时间
const int ALARM_INTERVAL = 3000;// 3秒间隔
const int TEXT_LINE_SPACING = 35; // 警报文本行间距（像素）
DWORD g_lastTime = 0; // 记录上一帧的时间

// 函数声明
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void                DrawPlane(HDC hdc);
void                DrawAlarmTexts(HDC hdc); // 绘制多行警报文本
void                UpdatePlanePosition();
void                CheckAlarms(HWND hWnd);  // 收集多警报文本+颜色
void                ClearAlarmTexts();       // 清空警报文本缓存
void                PlayAlarmByPriority();
int                 GetHighestAlarmPriority();
COLORREF            GetAlarmColorByPriority(int priority);
void                InitObstacles();  // 初始化随机地障
void                DrawObstacles(HDC hdc);  // 绘制地障
bool                CheckCollision();  // 检测碰撞
void                PlayCrashSound();  // 播放坠毁音效
void                ResetGame();  // 重置游戏状态

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 初始化窗口标题
    wcscpy_s(szTitle, L"飞行模拟器");
    wcscpy_s(szWindowClass, L"PlaneAlarmClass");
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    InitObstacles();  // 初始化随机大小的地障

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT1));

    MSG msg;
    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            if (!isCrashed) {  // 坠毁后停止更新
                UpdatePlanePosition();
                CheckAlarms(g_hMainWnd);

                // 检测碰撞
                if (CheckCollision()) {
                    isCrashed = true;
                    PlayCrashSound();
                }
            }
            if (g_hMainWnd != nullptr)
                InvalidateRect(g_hMainWnd, nullptr, TRUE);
            // 替换 Sleep(16)
            DWORD currentTime = GetTickCount();
            DWORD deltaTime = currentTime - g_lastTime;
            if (deltaTime < 16) { // 目标是60帧 (1000/60 ≈ 16ms)
                Sleep(16 - deltaTime); // 只休眠剩下的时间
            }
            g_lastTime = GetTickCount(); // 更新时间
        }
    }

    // 释放警报文本缓存
    ClearAlarmTexts();
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT1);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    g_hMainWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!g_hMainWnd)
        return FALSE;

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    return TRUE;
}

// 仅绘制飞机
void DrawPlane(HDC hdc)
{
    POINT planePoints[] = {
        { 20, 0 }, {-15, 10 }, {-10, 3 }, {-20, 3 }, {-20, -3 },
        {-10, -3 }, {-15, -10 }, {20, 0 }, {10, 15 }, {-5, 5 },
        {-5, -5 }, {10, -15 }, {20, 0 }
    };
    int pointCount = sizeof(planePoints) / sizeof(POINT);

    // 飞机变换（仅作用于飞机）
    XFORM xform;
    xform.eM11 = (FLOAT)cos(planeAngle);
    xform.eM12 = (FLOAT)-sin(planeAngle);
    xform.eM21 = (FLOAT)sin(planeAngle);
    xform.eM22 = (FLOAT)cos(planeAngle);
    xform.eDx = (FLOAT)planeX;
    xform.eDy = (FLOAT)planeY;

    SetGraphicsMode(hdc, GM_ADVANCED);
    ModifyWorldTransform(hdc, &xform, MWT_SET);
    
    // 绘制飞机
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    Polyline(hdc, planePoints, pointCount);
    
    // 恢复资源
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    ModifyWorldTransform(hdc, nullptr, MWT_IDENTITY);
}

// 绘制右上角多行警报文本（按优先级从上到下）
void DrawAlarmTexts(HDC hdc)
{
    if (alarmTexts.empty() || hdc == nullptr || g_hMainWnd == nullptr)
        return;

    // 获取窗口尺寸
    RECT windowRect;
    GetClientRect(g_hMainWnd, &windowRect);
    int windowWidth = windowRect.right;

    // 创建字体
    HFONT hFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
    if (hFont == nullptr) return;
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SetBkMode(hdc, TRANSPARENT); // 透明背景
    
    // 逐行绘制警报文本（最高优先级在上）
    int baseY = 20; // 第一行Y坐标（顶部20px）
    for (int i = 0; i < alarmTexts.size(); i++)
    {
        // 计算单行文本宽度，确定右对齐X坐标
        SIZE textSize;
        GetTextExtentPoint32W(hdc, alarmTexts[i], wcslen(alarmTexts[i]), &textSize);
        int textX = windowWidth - textSize.cx - 20; // 右边缘-20px边距
        int textY = baseY + i * TEXT_LINE_SPACING;  // 逐行下移

        // 设置对应颜色，绘制文本
        SetTextColor(hdc, alarmColors[i]);
        TextOutW(hdc, textX, textY, alarmTexts[i], wcslen(alarmTexts[i]));
    }
   
    // 释放资源
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

// 生成2个左侧地障 + 1条停机跑道
void GenerateRunwayAndLeftObstacles() {
    if (g_hMainWnd == nullptr || isRunwayGenerated) return;

    RECT clientRect;
    GetClientRect(g_hMainWnd, &clientRect);
    int winHeight = clientRect.bottom - clientRect.top;
    int obstacleBottomY = winHeight - OBSTACLE_GROUND_GAP;

    // 初始化随机数（左侧地障高度随机）
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> heightDist(OBSTACLE_MIN_HEIGHT, OBSTACLE_MAX_HEIGHT);

    // ========== 1. 清空原有地障（可选：保留原有+新增，或替换） ==========
    obstacles.clear();  // 若想保留原有地障，注释这行

    // ========== 2. 生成2个左侧地障（X坐标固定在左侧） ==========
    Obstacle obs1, obs2;
    // 第一个左侧地障（X=60）
    obs1.x = 60;
    obs1.height = heightDist(gen);
    obs1.width = OBSTACLE_WIDTH;
    obs1.y = obstacleBottomY - obs1.height;
    // 第二个左侧地障（X=90）
    obs2.x = 90;
    obs2.height = heightDist(gen);
    obs2.width = OBSTACLE_WIDTH;
    obs2.y = obstacleBottomY - obs2.height;

    obstacles.push_back(obs1);
    obstacles.push_back(obs2);

    // ========== 3. 生成停机跑道（底部左侧，宽150，高8） ==========
    Obstacle runway;
    runway.x = RUNWAY_X;  // 左侧X=50
    runway.width = RUNWAY_WIDTH;  // 宽150
    runway.height = RUNWAY_HEIGHT;  // 高8（扁平）
    // 跑道Y坐标：底部离地10像素（比普通地障更贴近地面）
    runway.y = winHeight - 10 - runway.height;
    // 跑道颜色单独标记（用RGB值区分，后续绘制时用）
    runway.color = RGB(RUNWAY_COLOR_R, RUNWAY_COLOR_G, RUNWAY_COLOR_B);  // 需给Obstacle加color字段！

    obstacles.push_back(runway);
}

// 更新飞机位置
void UpdatePlanePosition() {
    if (g_hMainWnd == nullptr) return;

    // 1. 更新迎角和速度逻辑
    double absAngle = fabs(planeAngle);
    if (absAngle > M_PI) {
        absAngle = 2 * M_PI - absAngle;
    }

    // 标记迎角是否超过限制（用于按键控制和失速检测）
    bool isAngleOverLimit = (absAngle > STALL_ANGLE); // 这里修复：直接赋值给全局/状态标记

    if (isAngleOverLimit) {
        speed = max(speed - SPEED_DECAY_RATE, 0.0);
        isStall = (speed < STALL_SPEED_THRESHOLD);
        currentDescentSpeed = STALL_DESCENT_RATE; // 使用基础下降速度
    }
    else {
        isStall = false;
        currentDescentSpeed = 0.0;
    }

    // 2. 更新位置 (X轴)
    planeX += (int)(speed * cos(planeAngle));

    // 3. 更新位置 (Y轴)：正常飞行 + 失速下坠叠加
    double normalYDelta = speed * sin(planeAngle);
    planeY += (int)(normalYDelta + currentDescentSpeed);

    // 4. 边界检测
    RECT rect;
    GetClientRect(g_hMainWnd, &rect);
    int windowWidth = rect.right;
    int windowHeight = rect.bottom;

    bool isCrossRight = false;
    if (planeX < 0) {
        planeX = windowWidth;
        isCrossRight = true;
    }
    if (planeX > windowWidth) planeX = 0;
    if (planeY < 0) planeY = 0;
    if (planeY >= windowHeight) {
        planeY = windowHeight;
        isCrashed = true;
    }

    // 5. 检查是否生成跑道
    if (isCrossRight && !isRunwayGenerated) {
        flyRightCount++;
        if (flyRightCount >= TRIGGER_COUNT) {
            GenerateRunwayAndLeftObstacles();
            isRunwayGenerated = true;
        }
    }
}

// 收集多警报文本+颜色（按优先级排序）
void CheckAlarms(HWND hWnd)
{
    if (hWnd == nullptr)
    {
        ClearAlarmTexts();
        return;
    }

    // 清空上一轮警报文本
    ClearAlarmTexts();

    RECT rect;
    GetClientRect(hWnd, &rect);
    int windowWidth = rect.right;
    int windowHeight = rect.bottom;

    // 1. 检测各警报条件
// 重置状态
    isObstacleAlarm = false; // 接近地障
    isGroundAlarm = false;   // 接近地面


    // --- 逻辑A：检测地障（绿色柱子） ---
    // 只有在没有生成跑道时，才检测随机地障（或者根据你的需求调整）
    // 如果你想让跑道生成后，旁边的两个柱子也继续报警，请保留这个循环
    for (const auto& obs : obstacles) {
        // 特殊处理：如果这个地障是“跑道”本身，我们不报警（跳过）
        // 假设跑道的颜色是灰色的，我们通过颜色判断跳过
        if (obs.color == RGB(RUNWAY_COLOR_R, RUNWAY_COLOR_G, RUNWAY_COLOR_B)) {
            continue; // 跳过跑道，不检测跑道报警
        }

        // 计算距离
        if (planeY < obs.y) { // 飞机在地障上方
            int verticalDistance = obs.y - planeY; // 距离 = 地障顶部 - 飞机高度
            // 如果距离很近（例如小于地障高度的一半，或者固定值），触发警报
            if (verticalDistance < 50) { // 50像素以内报警
                isObstacleAlarm = true;
                break; // 只要撞到一个就报警
            }
        }
    }

    // --- 逻辑B：检测近地（海面/地面） ---
    // 如果还没有触发地障报警，或者你想两者同时存在，就检查窗口底部
    // 这里我们设定：如果窗口底部还有100像素，就触发“近地”报警
    if (planeY > windowHeight - 100) {
        isGroundAlarm = true;
    }

    isSpeedAlarm = (speed > 8 || speed < 2);         // 速度异常
    isBorderAlarm = (planeX < 50 || planeX > windowWidth - 50 || planeY < 50); // 边界

    // 2. 按优先级从上到下添加警报文本+颜色（最高优先级先加）  
      if (isStall) {
        WCHAR* text = new WCHAR[50];
        wcscpy_s(text, 50, L"⚠️ 紧急优先级：飞机失速！");
        alarmTexts.push_back(text);
        alarmColors.push_back(GetAlarmColorByPriority(0));  // 新增0级（最高）
    }
      if (isObstacleAlarm) {
          WCHAR* text = new WCHAR[50];
          wcscpy_s(text, 50, L"⚠️ 最高优先级：接近地障（铁塔）！");
          alarmTexts.push_back(text);
          alarmColors.push_back(GetAlarmColorByPriority(1));
      }
    if (isGroundAlarm)
    {
        WCHAR* text = new WCHAR[50];
        wcscpy_s(text, 50, L"⚠️ 最高优先级：飞机接近地面！");
        alarmTexts.push_back(text);
        alarmColors.push_back(GetAlarmColorByPriority(1));
    }
    if (isSpeedAlarm)
    {
        WCHAR* text = new WCHAR[50];
        wcscpy_s(text, 50, L"⚠️ 中级优先级：速度异常！");
        alarmTexts.push_back(text);
        alarmColors.push_back(GetAlarmColorByPriority(2));
    }
    /*if (isBorderAlarm)
    {
        WCHAR* text = new WCHAR[50];
        wcscpy_s(text, 50, L"⚠️ 低级优先级：靠近窗口边界！");
        alarmTexts.push_back(text);
        alarmColors.push_back(GetAlarmColorByPriority(3));
    }*/

    // 3. 定时播放最高优先级警报音
    int highestPriority = GetHighestAlarmPriority();
    DWORD currentTime = GetTickCount();
    if (highestPriority > -1 && (currentTime - lastAlarmTime) >= ALARM_INTERVAL)
    {
        PlayAlarmByPriority();
        lastAlarmTime = currentTime;
    }
}

// 清空警报文本缓存（避免内存泄漏）
void ClearAlarmTexts()
{
    for (auto text : alarmTexts)
    {
        delete[] text;
    }
    alarmTexts.clear();
    alarmColors.clear();
}

// 获取最高优先级
int GetHighestAlarmPriority()
{
    if (isStall) return 0;          
    if (isGroundAlarm) return 1;
    if (isObstacleAlarm) return 1;   // 撞塔：次高
    if (isSpeedAlarm) return 2;
    if (isBorderAlarm) return 3;
    return -1;
}

// 按优先级返回颜色
COLORREF GetAlarmColorByPriority(int priority)
{
    switch (priority)
    {
    case 0: return RGB(255, 0, 255); // 失速：紫红色（最高级）
    case 1: return RGB(255, 0, 0);    // 最高：红色
    case 2: return RGB(255, 165, 0);  // 中级：橙色
    case 3: return RGB(170, 232, 66);  // 最低：绿色
    default: return RGB(0, 0, 0);
    }
}

// 播放最高优先级警报音
void PlayAlarmByPriority()
{
    int priority = GetHighestAlarmPriority();
    switch (priority)
    {
    case 0: // 失速警报（最急促）
        Beep(1000, 200);
        Beep(800, 200);
        Beep(1000, 200);
        break;
    case 1: // 近地：高频急促音
        Beep(2000, 200);
        Sleep(100);
        Beep(2000, 200);
        Sleep(100);
        Beep(2000, 200);
        break;
    case 2: // 速度：中频率长音
        Beep(1000, 500);
        break;
   /* case 3: // 边界：低频单音
        Beep(500, 300);
        break;*/
    default:
        break;
    }
}
// 初始化随机地障
void InitObstacles() {
    obstacles.clear(); 
    if (g_hMainWnd == nullptr) return; // 窗口未创建则直接返回

    // 获取当前窗口客户区的实际大小（排除边框/标题栏）
    RECT clientRect;
    GetClientRect(g_hMainWnd, &clientRect);
    int winWidth = clientRect.right - clientRect.left;
    int winHeight = clientRect.bottom - clientRect.top;
    // 初始化随机数生成器（确保每次生成的高度不同）
    std::random_device rd;
    std::mt19937 gen(rd());
    // 高度范围：OBSTACLE_MIN_HEIGHT ~ OBSTACLE_MAX_HEIGHT
    std::uniform_int_distribution<> heightDist(OBSTACLE_MIN_HEIGHT, OBSTACLE_MAX_HEIGHT);

    // 所有地障的底部Y坐标固定（保证不接触地面，且底部对齐）
    int obstacleBottomY = winHeight - OBSTACLE_GROUND_GAP;

    // 计算地障水平间距（保证均匀分布，左右留边）
    int totalGap = winWidth - 100;  // 左右各留50像素边距
    int spacing = totalGap / (OBSTACLE_COUNT + 1);  // 地障之间的间距
    
    // 创建5个随机地障
    for (int i = 0; i < 5; ++i) {
        Obstacle obs;
        // X坐标：水平均匀分布
        obs.x = 50 + spacing * (i + 1);
        // 高度：随机（10~120像素）
        obs.height = heightDist(gen);
        // 宽度：固定细宽度
        obs.width = OBSTACLE_WIDTH;
        // Y坐标 = 底部Y坐标 - 自身高度（保证底部对齐，顶部高低不平）
        obs.y = obstacleBottomY - obs.height;
        obs.color = RGB(34, 139, 34);
        obstacles.push_back(obs);
    }
}

// 绘制地障
void DrawObstacles(HDC hdc) {
    for (const auto& obs : obstacles) {
        // 根据颜色创建画刷（普通地障绿色，跑道灰色）
        HBRUSH hBrush = CreateSolidBrush(obs.color);
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));  // 边框黑色
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

        // 绘制地障/跑道
        Rectangle(hdc, obs.x, obs.y, obs.x + obs.width, obs.y + obs.height);

        // 释放资源
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);
        DeleteObject(hBrush);
    }
}

// 检测碰撞
bool CheckCollision() {
    if (isCrashed) return true;

    // 飞机碰撞盒
    RECT planeRect = { planeX - 20, planeY - 20, planeX + 20, planeY + 20 };

    // 检测与地障碰撞
    for (const auto& obs : obstacles) {
        RECT obsRect = { obs.x, obs.y, obs.x + obs.width, obs.y + obs.height };
        RECT temp;
        if (IntersectRect(&temp, &planeRect, &obsRect)) {
            return true;
        }
    }
    return false;
}

// 播放坠毁音效
void PlayCrashSound() {
    /*Beep(200, 300);
    Sleep(100);
    Beep(1500, 400);
    Sleep(100);
    Beep(100, 200);*/
    //MCIERROR mciError;
    mciSendString(L"open E:\\WindowsProject1\\Boom.mp3 alias myAudio", NULL, 0, NULL);
    mciSendString(L"play myAudio wait", NULL, 0, NULL);
    
    mciSendString(L"close myAudio", NULL, 0, NULL);
    
}

// 重置游戏
void ResetGame() {
    planeX = 400;
    planeY = 300;
    planeAngle = 0;
    speed = 5;
    isCrashed = false;
    isStall = false;               // 重置失速状态
    currentDescentSpeed = 0.0;     // 重置下降速度（新增）
    flyRightCount = 0;
    isRunwayGenerated = false;
    InitObstacles();  // 重新生成地障
}

// 窗口消息处理
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ERASEBKGND:
        // 直接返回非零值，告诉Windows：背景已经由我（双缓冲）处理了，不需要你擦除
        return 1;
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        if (wmId == IDM_ABOUT)
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
        else if (wmId == IDM_EXIT)
            DestroyWindow(hWnd);
        else
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    break;

    case WM_SIZE:
        // 窗口大小改变时，重新生成适配的地障
        InitObstacles();
        // 强制重绘窗口，让新地障显示出来
        InvalidateRect(hWnd, nullptr, TRUE);
        break;


    // 键盘控制
    case WM_KEYDOWN:
        // R键重置游戏（原有逻辑）
        if (wParam == 'R' || wParam == 'r') {
            ResetGame();
            break;
        }

        // 坠毁状态下不响应控制键（原有逻辑）
        if (isCrashed) break;

        // ========== 新增：失速时限制手动加速 ==========
        absAngle = fabs(planeAngle);
        if (absAngle > M_PI) isAngleOverLimit = (absAngle > STALL_ANGLE);

        switch (wParam)
        {
        case VK_LEFT:  planeAngle -= 0.1; break;
        case VK_RIGHT: planeAngle += 0.1; break;
        case VK_UP:            // 失速时（迎角超18°）无法手动加速
            if (!isAngleOverLimit) {
                speed = min(speed + 1, 10);
            }
            break;
        case VK_DOWN:  speed = max(speed - 1, 0); break;
        //case 'R':      ResetGame(); break;  // 添加R键重置
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // --- 双缓冲绘图开始 ---
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;

        // 1. 创建内存DC和兼容位图
        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdc, width, height);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

        // 2. 在内存DC上绘图（背景、地障、飞机、警报）
        // 2.1 填充背景（这里用深蓝色代表天空/海洋）
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 50)); // 深蓝背景
        FillRect(hMemDC, &clientRect, hBrush);
        DeleteObject(hBrush);

        // 2.2 绘制地障
        DrawObstacles(hMemDC);

        // 2.3 绘制飞机
        DrawPlane(hMemDC);

        // 2.4 绘制警报文本
        DrawAlarmTexts(hMemDC);

        // 3. 将内存DC一次性拷贝到屏幕DC（消除闪烁的关键）
        BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);

        // 4. 清理资源
        SelectObject(hMemDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemDC);
        // --- 双缓冲绘图结束 ---
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

// 关于对话框
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}