#include <iostream>
#include <Windows.h>
#include "Utils.h"
#include "d3d.h"
#include <dwmapi.h>
#include <vector>
#include "offsets.h"
#include "settings.h"
#include "struct.h"
#include <list>

static HWND Window = NULL;
IDirect3D9Ex* pObject = NULL;
static LPDIRECT3DDEVICE9 D3DDevice = NULL;
static LPDIRECT3DVERTEXBUFFER9 VertBuff = NULL;

#define OFFSET_UWORLD 0x95ecb60
#define teamIndex 0xED0
int localplayerID;

DWORD_PTR Uworld;
DWORD_PTR LocalPawn;
DWORD_PTR PlayerState;
DWORD_PTR Localplayer;
DWORD_PTR Rootcomp;
DWORD_PTR PlayerController;
DWORD_PTR Ulevel;
DWORD_PTR Persistentlevel;

Vector3 localactorpos;
Vector3 Localcam;

static void WindowMain();
static void InitializeD3D();
static void Loop();
static void ShutDown();

FTransform GetBoneIndex(DWORD_PTR mesh, int index)
{
	DWORD_PTR bonearray = read<DWORD_PTR>(DrverInit, FNProcID, mesh + 0x4A8);  // 4A8  changed often 4u

	if (bonearray == NULL) // added 4u
	{
		bonearray = read<DWORD_PTR>(DrverInit, FNProcID, mesh + 0x4A8 + 0x10); // added 4u
	}

	return read<FTransform>(DrverInit, FNProcID, bonearray + (index * 0x30));  // doesn't change
}

Vector3 GetBoneWithRotation(DWORD_PTR mesh, int id)
{
	FTransform bone = GetBoneIndex(mesh, id);
	FTransform ComponentToWorld = read<FTransform>(DrverInit, FNProcID, mesh + 0x1C0);  // have never seen this change 4u

	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

D3DMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
{
	float radPitch = (rot.x * float(M_PI) / 180.f);
	float radYaw = (rot.y * float(M_PI) / 180.f);
	float radRoll = (rot.z * float(M_PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = origin.x;
	matrix.m[3][1] = origin.y;
	matrix.m[3][2] = origin.z;
	matrix.m[3][3] = 1.f;

	return matrix;
}

//4u note:  changes to projectw2s and camera are the most diffucult changes to understand reworking old camloc, be careful blindly making edits

extern Vector3 CameraEXT(0, 0, 0);
float FovAngle;
Vector3 camrot;
Vector3 camloc;

Vector3 ProjectWorldToScreen(Vector3 WorldLocation, Vector3 camrot)
{
	Vector3 Screenlocation = Vector3(0, 0, 0);
	Vector3 Camera;

	auto chain69 = read<uintptr_t>(DrverInit, FNProcID, Localplayer + 0xa8);
	uint64_t chain699 = read<uintptr_t>(DrverInit, FNProcID, chain69 + 8);

	Camera.x = read<float>(DrverInit, FNProcID, chain699 + 0x7F8);  //camera pitch  watch out for x and y swapped 4u
	Camera.y = read<float>(DrverInit, FNProcID, Rootcomp + 0x12C);  //camera yaw

	float test = asin(Camera.x);
	float degrees = test * (180.0 / M_PI);
	Camera.x = degrees;

	if (Camera.y < 0)
		Camera.y = 360 + Camera.y;

	D3DMATRIX tempMatrix = Matrix(Camera);
	Vector3 vAxisX, vAxisY, vAxisZ;

	vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	uint64_t chain = read<uint64_t>(DrverInit, FNProcID, Localplayer + 0x70);
	uint64_t chain1 = read<uint64_t>(DrverInit, FNProcID, chain + 0x98);
	uint64_t chain2 = read<uint64_t>(DrverInit, FNProcID, chain1 + 0x130);

	Vector3 vDelta = WorldLocation - read<Vector3>(DrverInit, FNProcID, chain2 + 0x10); //camera location credits for Object9999
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	float zoom = read<float>(DrverInit, FNProcID, chain699 + 0x590);

	FovAngle = 80.0f / (zoom / 1.19f);
	float ScreenCenterX = Width / 2.0f;
	float ScreenCenterY = Height / 2.0f;

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	CameraEXT = Camera;

	return Screenlocation;
}


DWORD GUI(LPVOID in)
{
	while (1)
	{
		if (GetAsyncKeyState(VK_INSERT) & 1) {
			Settings::ShowMenu = !Settings::ShowMenu;
		}
		Sleep(2);
	}
}

typedef struct _FNlEntity
{
	uint64_t Actor;
	int ID;
	uint64_t mesh;
}FNlEntity;

std::vector<FNlEntity> entityList;

#define DEBUG

std::string GetGNamesByObjID(int32_t ObjectID)
{
	uint64_t gname = read<uint64_t>(DrverInit, FNProcID, base_address + 0x4CBB6F0);
	//printf(("gname: %p.\n"), gname);

	int64_t fNamePtr = read<uint64_t>(DrverInit, FNProcID, gname + int(ObjectID / 0x4000) * 0x8);
	int64_t fName = read<uint64_t>(DrverInit, FNProcID, fNamePtr + int(ObjectID % 0x4000) * 0x8);

	char pBuffer[64] = { NULL };

	info_t blyat;
	blyat.pid = FNProcID;
	blyat.address = fName + 0x10;
	blyat.value = &pBuffer;
	blyat.size = sizeof(pBuffer);

	unsigned long int asd;
	DeviceIoControl(DrverInit, ctl_read, &blyat, sizeof blyat, &blyat, sizeof blyat, &asd, nullptr);

	return std::string(pBuffer);
}

void cache()
{
	while (true)
	{
		std::vector<FNlEntity> tmpList;

		Uworld = read<DWORD_PTR>(DrverInit, FNProcID, base_address + OFFSET_UWORLD);
		DWORD_PTR Gameinstance = read<DWORD_PTR>(DrverInit, FNProcID, Uworld + 0x180);
		DWORD_PTR LocalPlayers = read<DWORD_PTR>(DrverInit, FNProcID, Gameinstance + 0x38);
		Localplayer = read<DWORD_PTR>(DrverInit, FNProcID, LocalPlayers);
		PlayerController = read<DWORD_PTR>(DrverInit, FNProcID, Localplayer + 0x30);
		LocalPawn = read<DWORD_PTR>(DrverInit, FNProcID, PlayerController + 0x2A0);

		PlayerState = read<DWORD_PTR>(DrverInit, FNProcID, LocalPawn + 0x240);
		Rootcomp = read<DWORD_PTR>(DrverInit, FNProcID, LocalPawn + 0x130);

		if (LocalPawn != 0) {
			localplayerID = read<int>(DrverInit, FNProcID, LocalPawn + 0x18);

			Persistentlevel = read<DWORD_PTR>(DrverInit, FNProcID, Uworld + 0x30);
			DWORD ActorCount = read<DWORD>(DrverInit, FNProcID, Persistentlevel + 0xA0);
			DWORD_PTR AActors = read<DWORD_PTR>(DrverInit, FNProcID, Persistentlevel + 0x98);

			for (int i = 0; i < ActorCount; i++)
			{
				uint64_t CurrentActor = read<uint64_t>(DrverInit, FNProcID, AActors + i * 0x8);

				int curactorid = read<int>(DrverInit, FNProcID, CurrentActor + 0x18);

				std::string test = GetGNamesByObjID(curactorid);
				std::cout << test << std::endl;

				if (curactorid == localplayerID || curactorid == localplayerID + 765)
				{
					FNlEntity fnlEntity{ };
					fnlEntity.Actor = CurrentActor;
					fnlEntity.mesh = read<uint64_t>(DrverInit, FNProcID, CurrentActor + 0x280);
					fnlEntity.ID = curactorid;
					tmpList.push_back(fnlEntity);
				}
			}
			entityList = tmpList;
			Sleep(1);
		}
	}
}

int main(int argc, const char* argv[])
{
	Sleep(300);  //slowed it down a bit 4u
	SetConsoleTitleA("Overlay Console");
	system("Color b");

	CreateThread(NULL, NULL, GUI, NULL, NULL, NULL);
	DrverInit = CreateFileW((L"\\\\.\\AnkrWare"), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (DrverInit == INVALID_HANDLE_VALUE)
	{
		printf("\n Driver Failure");
		Sleep(2000);
		exit(0);
	}

	info_t Input_Output_Data1;
	unsigned long int Readed_Bytes_Amount1;
	DeviceIoControl(DrverInit, ctl_clear, &Input_Output_Data1, sizeof Input_Output_Data1, &Input_Output_Data1, sizeof Input_Output_Data1, &Readed_Bytes_Amount1, nullptr);


	while (hWnd == NULL)
	{
		hWnd = FindWindowA(0, ("Fortnite  "));
		system("cls");
		printf("\n Looking for Fortnite Process!");
	}
	GetWindowThreadProcessId(hWnd, &FNProcID);

	info_t Input_Output_Data;
	Input_Output_Data.pid = FNProcID;
	unsigned long int Readed_Bytes_Amount;

	DeviceIoControl(DrverInit, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
	//base_address = (unsigned long long int)Input_Output_Data.data;
	//std::printf(("Process base address: %p.\n"), (void*)base_address);

	HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
	CloseHandle(handle);
	//HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
	//CloseHandle(handle);

	WindowMain();
	InitializeD3D();

	Loop();
	ShutDown();

	return 0;

}

HWND GameWnd = NULL;



void Window2Target()
{
	while (true)
	{
		if (hWnd)
		{
			ZeroMemory(&ProcessWH, sizeof(ProcessWH));
			GetWindowRect(hWnd, &ProcessWH);
			Width = ProcessWH.right - ProcessWH.left;
			Height = ProcessWH.bottom - ProcessWH.top;
			DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);

			if (dwStyle & WS_BORDER)
			{
				ProcessWH.top += 32;
				Height -= 39;
			}
			ScreenCenterX = Width / 2;
			ScreenCenterY = Height / 2;
			MoveWindow(Window, ProcessWH.left, ProcessWH.top, Width, Height, true);
		}
		else
		{
			exit(0);
		}
	}
}



const MARGINS Margin = { -1 };
/*void WindowMain()
{
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Window2Target, 0, 0, 0);

	WNDCLASSEX ctx;
	ZeroMemory(&ctx, sizeof(ctx));
	ctx.cbSize = sizeof(ctx);
	ctx.lpszClassName = L"fart.club";
	ctx.lpfnWndProc = WindowProc;
	RegisterClassEx(&ctx);

	if (hWnd)
	{
		GetClientRect(hWnd, &ProcessWH);
		POINT xy;
		ClientToScreen(hWnd, &xy);
		ProcessWH.left = xy.x;
		ProcessWH.top = xy.y;

		Width = ProcessWH.right;
		Height = ProcessWH.bottom;
	}
	else
		exit(2);

	Window = CreateWindowEx(NULL, L"fart.club", L"fart.club1", WS_POPUP | WS_VISIBLE, 0, 0, Width, Height, 0, 0, 0, 0);
	DwmExtendFrameIntoClientArea(Window, &Margin);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);
}*/

void WindowMain()
{

	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Window2Target, 0, 0, 0);
	WNDCLASSEX wClass =
	{
		sizeof(WNDCLASSEX),
		0,
		WindowProc,
		0,
		0,
		nullptr,
		LoadIcon(nullptr, IDI_APPLICATION),
		LoadCursor(nullptr, IDC_ARROW),
		nullptr,
		nullptr,
		TEXT("Test1"),
		LoadIcon(nullptr, IDI_APPLICATION)
	};

	if (!RegisterClassEx(&wClass))
		exit(1);

	hWnd = FindWindowW(NULL, TEXT("Fortnite  "));

	//printf("GameWnd Found! : %p\n", GameWnd);

	if (hWnd)
	{
		GetClientRect(hWnd, &ProcessWH);
		POINT xy;
		ClientToScreen(hWnd, &xy);
		ProcessWH.left = xy.x;
		ProcessWH.top = xy.y;


		Width = ProcessWH.right;
		Height = ProcessWH.bottom;
	}
	else exit(2);

	Window = CreateWindowExA(NULL, "Test1", "Test1", WS_POPUP | WS_VISIBLE, ProcessWH.left, ProcessWH.top, Width, Height, NULL, NULL, 0, NULL);
	DwmExtendFrameIntoClientArea(Window, &Margin);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);

}

void InitializeD3D()
{
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &pObject)))
		exit(3);

	ZeroMemory(&d3d, sizeof(d3d));
	d3d.BackBufferWidth = Width;
	d3d.BackBufferHeight = Height;
	d3d.BackBufferFormat = D3DFMT_A8R8G8B8;
	d3d.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	d3d.AutoDepthStencilFormat = D3DFMT_D16;
	d3d.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3d.EnableAutoDepthStencil = TRUE;
	d3d.hDeviceWindow = Window;
	d3d.Windowed = TRUE;

	pObject->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3d, &D3DDevice);

	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui_ImplWin32_Init(Window);
	ImGui_ImplDX9_Init(D3DDevice);

	ImGui::StyleColorsClassic();

	pObject->Release();
}


bool firstS = false;
void RadarLoop()
{

}

Vector3 Camera(unsigned __int64 RootComponent)
{
	unsigned __int64 PtrPitch;
	Vector3 Camera;

	auto pitch = read<uintptr_t>(DrverInit, FNProcID, Offsets::LocalPlayer + 0xb0);
	Camera.x = read<float>(DrverInit, FNProcID, RootComponent + 0x12C);
	Camera.y = read<float>(DrverInit, FNProcID, pitch + 0x678);

	float test = asin(Camera.y);
	float degrees = test * (180.0 / M_PI);

	Camera.y = degrees;

	if (Camera.x < 0)
		Camera.x = 360 + Camera.x;

	return Camera;
}

typedef struct
{
	DWORD R;
	DWORD G;
	DWORD B;
	DWORD A;
}RGBA;

class Color
{
public:
	RGBA red = { 255,0,0,255 };
	RGBA Magenta = { 255,0,255,255 };
	RGBA yellow = { 255,255,0,255 };
	RGBA grayblue = { 128,128,255,255 };
	RGBA green = { 128,224,0,255 };
	RGBA darkgreen = { 0,224,128,255 };
	RGBA brown = { 192,96,0,255 };
	RGBA pink = { 255,168,255,255 };
	RGBA DarkYellow = { 216,216,0,255 };
	RGBA SilverWhite = { 236,236,236,255 };
	RGBA purple = { 144,0,255,255 };
	RGBA Navy = { 88,48,224,255 };
	RGBA skyblue = { 0,136,255,255 };
	RGBA graygreen = { 128,160,128,255 };
	RGBA blue = { 0,96,192,255 };
	RGBA orange = { 255,128,0,255 };
	RGBA peachred = { 255,80,128,255 };
	RGBA reds = { 255,128,192,255 };
	RGBA darkgray = { 96,96,96,255 };
	RGBA Navys = { 0,0,128,255 };
	RGBA darkgreens = { 0,128,0,255 };
	RGBA darkblue = { 0,128,128,255 };
	RGBA redbrown = { 128,0,0,255 };
	RGBA purplered = { 128,0,128,255 };
	RGBA greens = { 0,255,0,255 };
	RGBA envy = { 0,255,255,255 };
	RGBA black = { 0,0,0,255 };
	RGBA gray = { 128,128,128,255 };
	RGBA white = { 255,255,255,255 };
	RGBA blues = { 30,144,255,255 };
	RGBA lightblue = { 135,206,250,160 };
	RGBA Scarlet = { 220, 20, 60, 160 };
	RGBA white_ = { 255,255,255,200 };
	RGBA gray_ = { 128,128,128,200 };
	RGBA black_ = { 0,0,0,200 };
	RGBA red_ = { 255,0,0,200 };
	RGBA Magenta_ = { 255,0,255,200 };
	RGBA yellow_ = { 255,255,0,200 };
	RGBA grayblue_ = { 128,128,255,200 };
	RGBA green_ = { 128,224,0,200 };
	RGBA darkgreen_ = { 0,224,128,200 };
	RGBA brown_ = { 192,96,0,200 };
	RGBA pink_ = { 255,168,255,200 };
	RGBA darkyellow_ = { 216,216,0,200 };
	RGBA silverwhite_ = { 236,236,236,200 };
	RGBA purple_ = { 144,0,255,200 };
	RGBA Blue_ = { 88,48,224,200 };
	RGBA skyblue_ = { 0,136,255,200 };
	RGBA graygreen_ = { 128,160,128,200 };
	RGBA blue_ = { 0,96,192,200 };
	RGBA orange_ = { 255,128,0,200 };
	RGBA pinks_ = { 255,80,128,200 };
	RGBA Fuhong_ = { 255,128,192,200 };
	RGBA darkgray_ = { 96,96,96,200 };
	RGBA Navy_ = { 0,0,128,200 };
	RGBA darkgreens_ = { 0,128,0,200 };
	RGBA darkblue_ = { 0,128,128,200 };
	RGBA redbrown_ = { 128,0,0,200 };
	RGBA purplered_ = { 128,0,128,200 };
	RGBA greens_ = { 0,255,0,200 };
	RGBA envy_ = { 0,255,255,200 };

	RGBA glassblack = { 0, 0, 0, 160 };
	RGBA GlassBlue = { 65,105,225,80 };
	RGBA glassyellow = { 255,255,0,160 };
	RGBA glass = { 200,200,200,60 };


	RGBA Plum = { 221,160,221,160 };

};
Color Col;

bool GetAimKey()
{
	return (GetAsyncKeyState(VK_RBUTTON));
}

void WriteAngles(float TargetX, float TargetY)
{
	float x = TargetX / 6.666666666666667f;
	float y = TargetY / 6.666666666666667f;
	y = -(y);

	writefloat(PlayerController + 0x418, y);
	writefloat(PlayerController + 0x418 + 0x4, x);
}

void aimbot(float x, float y)
{
	float ScreenCenterX = (Width / 2);
	float ScreenCenterY = (Height / 2);
	int AimSpeed = Settings::Smoothing;
	float TargetX = 0;
	float TargetY = 0;

	if (x != 0)
	{
		if (x > ScreenCenterX)
		{
			TargetX = -(ScreenCenterX - x);
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX > ScreenCenterX * 2) TargetX = 0;
		}

		if (x < ScreenCenterX)
		{
			TargetX = x - ScreenCenterX;
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX < 0) TargetX = 0;
		}
	}

	if (y != 0)
	{
		if (y > ScreenCenterY)
		{
			TargetY = -(ScreenCenterY - y);
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY > ScreenCenterY * 2) TargetY = 0;
		}

		if (y < ScreenCenterY)
		{
			TargetY = y - ScreenCenterY;
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY < 0) TargetY = 0;
		}
	}

	//WriteAngles(TargetX / 3.5f, TargetY / 3.5f);
	mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(TargetX), static_cast<DWORD>(TargetY), NULL, NULL);

	return;
}

void AimAt(DWORD_PTR entity)
{
	uint64_t currentactormesh = read<uint64_t>(DrverInit, FNProcID, entity + 0x280);
	auto rootHead = GetBoneWithRotation(currentactormesh, 7);
	Vector3 rootHeadOut = ProjectWorldToScreen(rootHead, Vector3(Localcam.y, Localcam.x, Localcam.z));

	if (rootHeadOut.y != 0 || rootHeadOut.y != 0)
	{
		aimbot(rootHeadOut.x, rootHeadOut.y);
	}
}

void DrawFilledRect(int x, int y, int w, int h, ImColor color)
{
	ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), ImColor(color), 0, 0);
}
#define ITEMS_ACTORID 867287
#define LLAMA_ACTORID 20644444
int botActorID = 0;

int r, g, b;
int r1, g2, b2;

float color_red = 1.;
float color_green = 0;
float color_blue = 0;
float color_random = 0.0;
float color_speed = -10.0;
bool rainbowmode = false;

void DrawLine(int x1, int y1, int x2, int y2, RGBA* color, int thickness)
{
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), ImGui::ColorConvertFloat4ToU32(ImVec4(color->R / 255.0, color->G / 255.0, color->B / 255.0, color->A / 255.0)), thickness);
}
int ScreenX = GetSystemMetrics(SM_CXSCREEN) / 2;
int ScreenY = GetSystemMetrics(SM_CYSCREEN);

void ShadowRGBText(int x, int y, ImColor color, const char* str)
{
	ImFont a;
	std::string utf_8_1 = std::string(str);
	std::string utf_8_2 = string_To_UTF8(utf_8_1);
	ImGui::GetOverlayDrawList()->AddText(ImVec2(x + 1, y + 2), ImGui::ColorConvertFloat4ToU32(ImColor(0, 0, 0, 240)), utf_8_2.c_str());
	ImGui::GetOverlayDrawList()->AddText(ImVec2(x + 1, y + 2), ImGui::ColorConvertFloat4ToU32(ImColor(0, 0, 0, 240)), utf_8_2.c_str());
	ImGui::GetOverlayDrawList()->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(ImVec4(color)), utf_8_2.c_str());
}

void DrawCornerBox(int x, int y, int w, int h, int borderPx, ImColor color)
{
	DrawFilledRect(x + borderPx, y, w / 3, borderPx, color); //top 
	DrawFilledRect(x + w - w / 3 + borderPx, y, w / 3, borderPx, color); //top 
	DrawFilledRect(x, y, borderPx, h / 3, color); //left 
	DrawFilledRect(x, y + h - h / 3 + borderPx * 2, borderPx, h / 3, color); //left 
	DrawFilledRect(x + borderPx, y + h + borderPx, w / 3, borderPx, color); //bottom 
	DrawFilledRect(x + w - w / 3 + borderPx, y + h + borderPx, w / 3, borderPx, color); //bottom 
	DrawFilledRect(x + w + borderPx, y, borderPx, h / 3, color);//right 
	DrawFilledRect(x + w + borderPx, y + h - h / 3 + borderPx * 2, borderPx, h / 3, color);//right 
}
void DrawNormalBox(int x, int y, int w, int h, int borderPx, ImColor color)
{
	DrawFilledRect(x + borderPx, y, w, borderPx, color); //top 
	DrawFilledRect(x + w - w + borderPx, y, w, borderPx, color); //top 
	DrawFilledRect(x, y, borderPx, h, color); //left 
	DrawFilledRect(x, y + h - h + borderPx * 2, borderPx, h, color); //left 
	DrawFilledRect(x + borderPx, y + h + borderPx, w, borderPx, color); //bottom 
	DrawFilledRect(x + w - w + borderPx, y + h + borderPx, w, borderPx, color); //bottom 
	DrawFilledRect(x + w + borderPx, y, borderPx, h, color);//right 
	DrawFilledRect(x + w + borderPx, y + h - h + borderPx * 2, borderPx, h, color);//right 
}
uintptr_t fnGetBounds = 0;
FBox GetFBox(uintptr_t Actor)
{
	if (!Actor) return {};

	Vector3 Origin, BoxExtend;

	auto fGetActorBounds = reinterpret_cast<void(__fastcall*)(__int64, char, Vector3*, Vector3*)>(fnGetBounds);

	fGetActorBounds, (__int64)Actor, (char)true, &Origin, &BoxExtend;

	FBox NewBox;
	NewBox.IsValid = 1;
	NewBox.Min = Origin - BoxExtend;
	NewBox.Max = Origin + BoxExtend;

	return NewBox;
}
void Background(int x, int y, int w, int h, ImColor color)
{
	ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), ImGui::ColorConvertFloat4ToU32(ImVec4(color)), 0, 0);
}
std::list<int> upper_part = { 65,66 };
std::list<int> right_arm = { 65, BONE_R_ARM_TOP, BONE_R_ARM_LOWER, BONE_MISC_R_HAND_1 };
std::list<int> left_arm = { 65, BONE_L_ARM_TOP, BONE_L_ARM_LOWER, BONE_MISC_L_HAND };
std::list<int> spine = { 65, BONE_PELVIS_1 };
std::list<int> lower_right = { BONE_PELVIS_2, BONE_R_THIGH ,76 };
std::list<int> lower_left = { BONE_PELVIS_2, BONE_L_THIGH ,69 };
std::list <int> Finger1 = { BONE_R_FINGER_1 };
std::list <int> Finger2 = { BONE_R_FINGER_2 };
std::list <int> Finger3 = { BONE_R_FINGER_3 };
std::list <int> FingerL1 = { BONE_L_FINGER_1 };
std::list <int> FingerL2 = { BONE_L_FINGER_2 };
std::list <int>FingerL3 = { BONE_L_FINGER_3 };
std::list<std::list<int>> Skeleton = { upper_part, right_arm, left_arm, spine, lower_right, lower_left, Finger1, Finger2, Finger3, FingerL1, FingerL2, FingerL3 };

void DrawSkeleton2(DWORD_PTR mesh)
{
	Vector3 neckpos = GetBoneWithRotation(mesh, 65);
	Vector3 pelvispos = GetBoneWithRotation(mesh, BONE_PELVIS_2);

	Vector3 previous(0, 0, 0);
	Vector3 current, p1, c1;
	Vector3 Localcam = Camera(Rootcomp);

	for (auto a : Skeleton)
	{
		previous = Vector3(0, 0, 0);
		for (int bone : a)
		{
			current = bone == 65 ? neckpos : (bone == BONE_PELVIS_2 ? pelvispos : GetBoneWithRotation(mesh, bone));
			if (previous.x == 0.f)
			{
				previous = current;
				continue;
			}

			p1 = ProjectWorldToScreen(previous, Vector3(Localcam.y, Localcam.x, Localcam.z));
			c1 = ProjectWorldToScreen(current, Vector3(Localcam.y, Localcam.x, Localcam.z));

			//DrawLine(vHipOut.x, vHipOut.y, vNeckOut.x, vNeckOut.y, &ESPcolor, 1);

			DrawLine(p1.x, p1.y, c1.x, c1.y, &Col.white, 1);


			previous = current;
		}
	}
}
void DrawSkeleton3(DWORD_PTR mesh)
{
	Vector3 neckpos = GetBoneWithRotation(mesh, 65);
	Vector3 pelvispos = GetBoneWithRotation(mesh, BONE_PELVIS_2);

	Vector3 previous(0, 0, 0);
	Vector3 current, p1, c1;
	Vector3 Localcam = Camera(Rootcomp);

	for (auto a : Skeleton)
	{
		previous = Vector3(0, 0, 0);
		for (int bone : a)
		{
			current = bone == 65 ? neckpos : (bone == BONE_PELVIS_2 ? pelvispos : GetBoneWithRotation(mesh, bone));
			if (previous.x == 0.f)
			{
				previous = current;
				continue;
			}

			p1 = ProjectWorldToScreen(previous, Vector3(Localcam.y, Localcam.x, Localcam.z));
			c1 = ProjectWorldToScreen(current, Vector3(Localcam.y, Localcam.x, Localcam.z));

			//DrawLine(vHipOut.x, vHipOut.y, vNeckOut.x, vNeckOut.y, &ESPcolor, 1);

				DrawLine(p1.x, p1.y, c1.x, c1.y, &Col.black, 3);
				DrawLine(p1.x, p1.y, c1.x, c1.y, &Col.white, 1);



			previous = current;
		}
	}
}
struct ActorStruct
{
	std::string name;
	uint64_t CurrentActor;
	int ID;
	bool Friendly;
	bool Loot;
	bool Llama;
	bool Bot;
};
std::vector<ActorStruct> ActorArray;
int llamacache = 0;
struct InfoStruct
{
	Vector3 Skel_Head;
	Vector3 Skel_Null;
	std::string name;
	Vector3 Pos;
	Vector3 Top;
	Vector3 Center;
	Vector3 Bottom;
	int ID;
	int Dist;
	bool Loot;
	bool Llama;
	bool Bot;
	bool Friendly;
	bool Target;
};
std::vector<InfoStruct> InfoArray;

Vector3 AimLocation;
uint64_t Target;
Vector3 TargetVelocity;
int ClosestDist;

float DistanceBetweenCross(float X, float Y)
{
	float ydist = (Y - (GetSystemMetrics(SM_CYSCREEN) / 2));
	float xdist = (X - (GetSystemMetrics(SM_CXSCREEN) / 2));
	float Hypotenuse = sqrt(pow(ydist, 2) + pow(xdist, 2));
	return Hypotenuse;
}

Vector3 Clamp(Vector3 r)
{
	if (r.y > 180.f)
		r.y -= 360.f;
	else if (r.y < -180.f)
		r.y += 360.f;

	if (r.y > 180.f)
		r.y -= 360.f;
	else if (r.y < -180.f)
		r.y += 360.f;

	if (r.x < -89.f)
		r.x = -89.f;
	else if (r.x > 89.f)
		r.x = 89.f;

	r.z = 0.f;

	return r;
}

Vector3 Vec2Rot(Vector3 vec)
{
	Vector3 rot;

	rot.y = RAD2DEG(std::atan2f(vec.y, vec.x));
	rot.x = RAD2DEG(std::atan2f(vec.z, std::sqrtf(vec.x * vec.x + vec.y * vec.y)));
	rot.z = 0.f;

	return rot;
}

void drawLoop() {
	if (Settings::AimbotFOV)
	{
		ImGui::GetOverlayDrawList()->AddCircle(ImVec2(ScreenCenterX, ScreenCenterY), Settings::AimbotFOV + 1, ImColor(0, 0, 0, 255), Settings::Roughness);
		ImGui::GetOverlayDrawList()->AddCircle(ImVec2(ScreenCenterX, ScreenCenterY), Settings::AimbotFOV, ImColor(255, 255, 255, 255), Settings::Roughness);
	}
	if (Settings::hitboxpos == 0)
	{
		Settings::hitbox = 66; //head
	}
	else if (Settings::hitboxpos == 1)
	{
		Settings::hitbox = 65; //neck
	}
	else if (Settings::hitboxpos == 2)
	{
		Settings::hitbox = 7; //chest
	}
	else if (Settings::hitboxpos == 3)
	{
		Settings::hitbox = 2; //pelvis
	}

	Uworld = read<DWORD_PTR>(DrverInit, FNProcID, base_address + OFFSET_UWORLD);
	//printf(_xor_("Uworld: %p.\n").c_str(), Uworld);

	DWORD_PTR Gameinstance = read<DWORD_PTR>(DrverInit, FNProcID, Uworld + 0x180); // changes sometimes 4u

	if (Gameinstance == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("Gameinstance: %p.\n").c_str(), Gameinstance);

	DWORD_PTR LocalPlayers = read<DWORD_PTR>(DrverInit, FNProcID, Gameinstance + 0x38);

	if (LocalPlayers == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("LocalPlayers: %p.\n").c_str(), LocalPlayers);

	Localplayer = read<DWORD_PTR>(DrverInit, FNProcID, LocalPlayers);

	if (Localplayer == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("LocalPlayer: %p.\n").c_str(), Localplayer);

	PlayerController = read<DWORD_PTR>(DrverInit, FNProcID, Localplayer + 0x30);

	if (PlayerController == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("playercontroller: %p.\n").c_str(), PlayerController);

	LocalPawn = read<uint64_t>(DrverInit, FNProcID, PlayerController + 0x2A0);  // changed often 4u sometimes called AcknowledgedPawn

	if (LocalPawn == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("Pawn: %p.\n").c_str(), LocalPawn);

	Rootcomp = read<uint64_t>(DrverInit, FNProcID, LocalPawn + 0x130);

	if (Rootcomp == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("Rootcomp: %p.\n").c_str(), Rootcomp);

	if (LocalPawn != 0) {
		localplayerID = read<int>(DrverInit, FNProcID, LocalPawn + 0x18);
	}

	Ulevel = read<DWORD_PTR>(DrverInit, FNProcID, Uworld + 0x30);
	//printf(_xor_("Ulevel: %p.\n").c_str(), Ulevel);

	if (Ulevel == (DWORD_PTR)nullptr)
		return;

	DWORD64 PlayerState = read<DWORD64>(DrverInit, FNProcID, LocalPawn + 0x240);  //changes often 4u

	if (PlayerState == (DWORD_PTR)nullptr)
		return;

	DWORD ActorCount = read<DWORD>(DrverInit, FNProcID, Ulevel + 0xA0);

	DWORD_PTR AActors = read<DWORD_PTR>(DrverInit, FNProcID, Ulevel + 0x98);
	//printf(_xor_("AActors: %p.\n").c_str(), AActors);

	if (AActors == (DWORD_PTR)nullptr)
		return;

	Vector3 Localcam = Camera(Rootcomp);

	for (int i = 0; i < ActorCount; i++)
	{
		uint64_t CurrentActor = read<uint64_t>(DrverInit, FNProcID, AActors + i * 0x8);

		int curactorid = read<int>(DrverInit, FNProcID, CurrentActor + 0x18);

		if (curactorid == localplayerID || curactorid == 17384284 || curactorid == 9875145 || curactorid == 9873134 || curactorid == 9876800 || curactorid == 9874439) // this number changes for bot and NPC often, modified from original 4u
		// you will need to print out the actorID on screen and find the new numbers, currently different numbers for bots, NPC(2), and bot in solo and 2 player games are different.
		//if (curactorid == localplayerID) //original changed 4u to target bots and NPC
		{
			if (CurrentActor == (uint64_t)nullptr || CurrentActor == -1 || CurrentActor == NULL)
				continue;

			uint64_t CurrentActorRootComponent = read<uint64_t>(DrverInit, FNProcID, CurrentActor + 0x130);

			if (CurrentActorRootComponent == (uint64_t)nullptr || CurrentActorRootComponent == -1 || CurrentActorRootComponent == NULL)
				continue;

			uint64_t currentactormesh = read<uint64_t>(DrverInit, FNProcID, CurrentActor + 0x280); // change as needed 4u

			if (currentactormesh == (uint64_t)nullptr || currentactormesh == -1 || currentactormesh == NULL)
				continue;

			int MyTeamId = read<int>(DrverInit, FNProcID, PlayerState + 0xED0);  //changes often 4u

			DWORD64 otherPlayerState = read<uint64_t>(DrverInit, FNProcID, CurrentActor + 0x240); //changes often 4u

			if (otherPlayerState == (uint64_t)nullptr || otherPlayerState == -1 || otherPlayerState == NULL)
				continue;

			int ActorTeamId = read<int>(DrverInit, FNProcID, otherPlayerState + 0xED0); //changes often 4u


			Vector3 Headpos = GetBoneWithRotation(currentactormesh, 66);
			Localcam = CameraEXT;
			localactorpos = read<Vector3>(DrverInit, FNProcID, Rootcomp + 0x11C);

			float distance = localactorpos.Distance(Headpos) / 100.f;

			if (distance < 1.5f)

				Vector3 Headpos = GetBoneWithRotation(currentactormesh, 66);
			Localcam = CameraEXT;
			localactorpos = read<Vector3>(DrverInit, FNProcID, Rootcomp + 0x11C);

			//float distance = localactorpos.Distance(Headpos) / 100.f;

			if (distance < 0.5)
				continue;
			//Vector3 CirclePOS = GetBoneWithRotation(currentactormesh, 2);
			localactorpos = read<Vector3>(DrverInit, FNProcID, Rootcomp + 0x11C);
			Vector3 rootOut = GetBoneWithRotation(currentactormesh, 0);
			Vector3 Out = ProjectWorldToScreen(rootOut, Vector3(Localcam.y, Localcam.x, Localcam.z));

			Vector3 HeadposW2s = ProjectWorldToScreen(Headpos, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 bone0 = GetBoneWithRotation(currentactormesh, 0);
			Vector3 bottom = ProjectWorldToScreen(bone0, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Headbox = ProjectWorldToScreen(Vector3(Headpos.x, Headpos.y, Headpos.z + 15), Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Aimpos = ProjectWorldToScreen(Vector3(Headpos.x, Headpos.y, Headpos.z + 10), Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 HeadBone = GetBoneWithRotation(currentactormesh, 96);
			Vector3 RootBone = GetBoneWithRotation(currentactormesh, 0);
			uint64_t TheirRootcomp = read<uint64_t>(DrverInit, FNProcID, CurrentActor + 0x130);
			Vector3 HeadBoneOut = ProjectWorldToScreen(Vector3(HeadBone.x, HeadBone.y, HeadBone.z), Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 RootBoneOut = ProjectWorldToScreen(RootBone, Vector3(Localcam.y, Localcam.x, Localcam.z));

			//	Vector3 RootPos = GetBoneWithRotation(ActorStruct.Mesh, select_hitbox());
			//	Vector3 selection;
#define vischeck 

			float BoxHeight = abs(HeadBoneOut.y - RootBoneOut.y);
			float BoxWidth = BoxHeight / 1.8f;

			float LeftX = (float)Headbox.x - (BoxWidth / 1);
			float LeftY = (float)bottom.y;

			float CornerHeight = abs(Headbox.y - bottom.y);
			float CornerWidth = CornerHeight * 0.75;
			//ImGui::GetOverlayDrawList()->AddCircle(ImVec2(ScreenCenterX, ScreenCenterY), Settings::AimbotFOV, ImColor(255, 255, 255, 230), Settings::Roughness);

			//int MyTeamId = read<int>(DrverInit, FNProcID, PlayerState + teamIndex);
			//DWORD64 otherPlayerState = read<uint64_t>(DrverInit, FNProcID, entity.Actor + 0x238);
			//int ActorTeamId = read<int>(DrverInit, FNProcID, otherPlayerState + teamIndex);
			//if wa(!Settings::Selfesp && isLocalPlayer(LocalPos, localactorpos))
				//continue;

			Vector3 vHeadBone = GetBoneWithRotation(currentactormesh, 96);
			Vector3 vHip = GetBoneWithRotation(currentactormesh, 2);
			Vector3 vNeck = GetBoneWithRotation(currentactormesh, 65);
			Vector3 vUpperArmLeft = GetBoneWithRotation(currentactormesh, 34);
			Vector3 vUpperArmRight = GetBoneWithRotation(currentactormesh, 91);
			Vector3 vLeftHand = GetBoneWithRotation(currentactormesh, 35);
			Vector3 vRightHand = GetBoneWithRotation(currentactormesh, 63);
			Vector3 vLeftHand1 = GetBoneWithRotation(currentactormesh, 33);
			Vector3 vRightHand1 = GetBoneWithRotation(currentactormesh, 60);
			Vector3 vRightThigh = GetBoneWithRotation(currentactormesh, 74);
			Vector3 vLeftThigh = GetBoneWithRotation(currentactormesh, 67);
			Vector3 vRightCalf = GetBoneWithRotation(currentactormesh, 75);
			Vector3 vLeftCalf = GetBoneWithRotation(currentactormesh, 68);
			Vector3 vLeftFoot = GetBoneWithRotation(currentactormesh, 69);
			//Vector3 vRightFoot = GetBoneWithRotation(currentactormesh, 76);
			//Vector3 vRightFoot = GetBoneWithRotation(currentactormesh, 76);
			Vector3 vRightFoot = GetBoneWithRotation(currentactormesh, 76);
			Vector3 Finger1 = GetBoneWithRotation(currentactormesh, BONE_R_FINGER_1);
			Vector3 Finger2 = GetBoneWithRotation(currentactormesh, BONE_R_FINGER_2);
			Vector3 Finger3 = GetBoneWithRotation(currentactormesh, BONE_R_FINGER_3);
			Vector3 FingerL1 = GetBoneWithRotation(currentactormesh, BONE_L_FINGER_1);
			Vector3 FingerL2 = GetBoneWithRotation(currentactormesh, BONE_L_FINGER_2);
			Vector3 FingerL3 = GetBoneWithRotation(currentactormesh, BONE_L_FINGER_3);

			Vector3 vHeadBoneOut = ProjectWorldToScreen(vHeadBone, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vHipOut = ProjectWorldToScreen(vHip, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vNeckOut = ProjectWorldToScreen(vNeck, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vUpperArmLeftOut = ProjectWorldToScreen(vUpperArmLeft, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vUpperArmRightOut = ProjectWorldToScreen(vUpperArmRight, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vLeftHandOut = ProjectWorldToScreen(vLeftHand, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vRightHandOut = ProjectWorldToScreen(vRightHand, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vLeftHandOut1 = ProjectWorldToScreen(vLeftHand1, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vRightHandOut1 = ProjectWorldToScreen(vRightHand1, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vRightThighOut = ProjectWorldToScreen(vRightThigh, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vLeftThighOut = ProjectWorldToScreen(vLeftThigh, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vRightCalfOut = ProjectWorldToScreen(vRightCalf, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vLeftCalfOut = ProjectWorldToScreen(vLeftCalf, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vLeftFootOut = ProjectWorldToScreen(vLeftFoot, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 vRightFootOut = ProjectWorldToScreen(vRightFoot, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Finger1R = ProjectWorldToScreen(Finger1, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Finger2R = ProjectWorldToScreen(Finger2, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Finger3R = ProjectWorldToScreen(Finger3, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Finger1L = ProjectWorldToScreen(FingerL1, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Finger2L = ProjectWorldToScreen(FingerL2, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Finger3L = ProjectWorldToScreen(FingerL3, Vector3(Localcam.y, Localcam.x, Localcam.z));

			float closestDistance = FLT_MAX;
			DWORD_PTR closestPawn = NULL;

			if (Settings::Selfesp)
			{
				Settings::PlayerESP = false;
				DrawCornerBox(RootBoneOut.x - (BoxWidth / 2), vHeadBoneOut.y, BoxWidth, BoxHeight, 1.0f, ImColor(66, 245, 242));
				if (Settings::Skeleton)
				{
					DrawSkeleton2(currentactormesh);
				}
				if (Settings::Distance)
				{
					char dist[64];
					char dist2[64];
					sprintf_s(dist, "Player", distance);
					sprintf_s(dist2, "      [%.fm]", distance);
					//sprintf_s(dist2, "Player", distance);
					Background(HeadposW2s.x - 40, HeadposW2s.y - 15, 100, 15, ImColor(0, 0, 0, 130));
					ShadowRGBText(HeadposW2s.x - 35, HeadposW2s.y - 15, ImColor(255, 255, 255, 200), dist);
					ShadowRGBText(HeadposW2s.x - 35, HeadposW2s.y - 15, ImColor(0, 255, 255, 200), dist2);
					//Background(HeadposW2s.x - 40, HeadposW2s.y + 100, 100, 15, ImColor(0, 0, 0, 130));
					//ShadowRGBText(HeadposW2s.x - 35, HeadposW2s.y + 100, ImColor(255, 255, 255, 200), dist);

					//[% .fm]
				}
				if (Settings::OutlinedSkeleton)
				{
					DrawSkeleton3(currentactormesh);
				}
			}

			bool bIsDying = read<bool>(DrverInit, FNProcID, CurrentActor + 0x520);
			if (ActorTeamId != MyTeamId && bIsDying)
			{
				if (Settings::Skeleton)
				{
					/*RGBA ESPcolor = Col.white;
					DrawLine(vHipOut.x, vHipOut.y, vNeckOut.x, vNeckOut.y, &ESPcolor, 1);

					DrawLine(vUpperArmLeftOut.x, vUpperArmLeftOut.y, vNeckOut.x, vNeckOut.y, &ESPcolor, 1);
					DrawLine(vUpperArmRightOut.x, vUpperArmRightOut.y, vNeckOut.x, vNeckOut.y, &ESPcolor, 1);

					DrawLine(vLeftHandOut.x, vLeftHandOut.y, vUpperArmLeftOut.x, vUpperArmLeftOut.y, &ESPcolor, 1);
					DrawLine(vRightHandOut.x, vRightHandOut.y, vUpperArmRightOut.x, vUpperArmRightOut.y, &ESPcolor, 1);

					DrawLine(vLeftHandOut.x, vLeftHandOut.y, vLeftHandOut1.x, vLeftHandOut1.y, &ESPcolor, 1);
					DrawLine(vRightHandOut.x, vRightHandOut.y, vRightHandOut1.x, vRightHandOut1.y, &ESPcolor, 1);

					DrawLine(vLeftThighOut.x, vLeftThighOut.y, vHipOut.x, vHipOut.y, &ESPcolor, 1);
					DrawLine(vRightThighOut.x, vRightThighOut.y, vHipOut.x, vHipOut.y, &ESPcolor, 1);

					DrawLine(vLeftCalfOut.x, vLeftCalfOut.y, vLeftThighOut.x, vLeftThighOut.y, &ESPcolor, 1);
					DrawLine(vRightCalfOut.x, vRightCalfOut.y, vRightThighOut.x, vRightThighOut.y, &ESPcolor, 1);

					DrawLine(vLeftFootOut.x, vLeftFootOut.y, vLeftCalfOut.x, vLeftCalfOut.y, &ESPcolor, 1);
					DrawLine(vRightFootOut.x, vRightFootOut.y, vRightCalfOut.x, vRightCalfOut.y, &ESPcolor, 1);

					DrawLine(Finger1.x, Finger1.y, Finger1.x, Finger1.y, &ESPcolor, 1);
					DrawLine(Finger2.x, Finger2.y, Finger2.x, Finger2.y, &ESPcolor, 1);
					DrawLine(Finger3.x, Finger3.y, Finger3.x, Finger3.y, &ESPcolor, 1);

					DrawLine(FingerL1.x, FingerL1.y, FingerL1.x, FingerL1.y, &ESPcolor, 1);
					DrawLine(FingerL2.x, FingerL2.y, FingerL2.x, FingerL2.y, &ESPcolor, 1);
					DrawLine(FingerL3.x, FingerL3.y, FingerL3.x, FingerL3.y, &ESPcolor, 1);*/

					DrawSkeleton2(currentactormesh);
				}
				if (Settings::ThreeDESP && currentactormesh && CurrentActor)
				{
					if (!CurrentActor) return;

					Vector3 min, max, vec1, vec2, vec3, vec4, vec5, vec6, vec7, vec8;

					FBox box = GetFBox(CurrentActor);

					if (!box.IsValid) return;

					min = box.Min;
					max = box.Max;

					vec1 = ProjectWorldToScreen(min, Vector3(Localcam.y, Localcam.x, Localcam.z));
					vec2 = ProjectWorldToScreen(max, Vector3(Localcam.y, Localcam.x, Localcam.z));
					vec3 = ProjectWorldToScreen(vec3, Vector3(Localcam.y, Localcam.x, Localcam.z));
					vec4 = ProjectWorldToScreen(vec4, Vector3(Localcam.y, Localcam.x, Localcam.z));
					vec5 = ProjectWorldToScreen(vec5, Vector3(Localcam.y, Localcam.x, Localcam.z));
					vec6 = ProjectWorldToScreen(vec6, Vector3(Localcam.y, Localcam.x, Localcam.z));
					vec7 = ProjectWorldToScreen(vec7, Vector3(Localcam.y, Localcam.x, Localcam.z));
					vec8 = ProjectWorldToScreen(vec8, Vector3(Localcam.y, Localcam.x, Localcam.z));

					if (vec1.x == 0 && vec1.y == 0) return;
					if (vec2.x == 0 && vec2.y == 0) return;
					if (vec3.x == 0 && vec3.y == 0) return;
					if (vec4.x == 0 && vec4.y == 0) return;
					if (vec5.x == 0 && vec5.y == 0) return;
					if (vec6.x == 0 && vec6.y == 0) return;
					if (vec7.x == 0 && vec7.y == 0) return;
					if (vec8.x == 0 && vec8.y == 0) return;

					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec1.x, vec1.y), ImVec2(vec5.x, vec5.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec2.x, vec2.y), ImVec2(vec8.x, vec8.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec3.x, vec3.y), ImVec2(vec7.x, vec7.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec4.x, vec4.y), ImVec2(vec6.x, vec6.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec1.x, vec1.y), ImVec2(vec3.x, vec3.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec1.x, vec1.y), ImVec2(vec4.x, vec4.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec8.x, vec8.y), ImVec2(vec3.x, vec3.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec8.x, vec8.y), ImVec2(vec4.x, vec4.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec2.x, vec2.y), ImVec2(vec6.x, vec6.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec2.x, vec2.y), ImVec2(vec7.x, vec7.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec5.x, vec5.y), ImVec2(vec6.x, vec6.y), ImColor(255, 255, 255, 255), 2);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(vec5.x, vec5.y), ImVec2(vec7.x, vec7.y), ImColor(255, 255, 255, 255), 2);
				}
				if (Settings::CornerESP)
				{
					//DrawCornerBox(RootBoneOut.x - BoxWidth / 2 + 1, vHeadBoneOut.y, BoxWidth, BoxHeight, 1.0f, ImColor(0, 0, 0));
					//DrawCornerBox(RootBoneOut.x - BoxWidth / 2 - 1, vHeadBoneOut.y, BoxWidth, BoxHeight, 1.0f, ImColor(0, 0, 0));
					//DrawCornerBox(RootBoneOut.x - BoxWidth / 2, vHeadBoneOut.y + 1, BoxWidth, BoxHeight, 1.0f, ImColor(0, 0, 0));
					//DrawCornerBox(RootBoneOut.x - BoxWidth / 2, vHeadBoneOut.y - 1, BoxWidth, BoxHeight, 1.0f, ImColor(0, 0, 0));
					DrawCornerBox(RootBoneOut.x - (BoxWidth / 2), vHeadBoneOut.y, BoxWidth, BoxHeight, 1.0f, ImColor(66, 245, 242));
				}
				float snaplinePower = 1.0f;
				if (Settings::Lines)
				{
					DrawLine(ScreenX, ScreenY, bottom.x, bottom.y, &Col.white, snaplinePower);
				}

				if (Settings::PlayerESP)
				{
					auto boxColor = ImColor(0, 221, 255, 255);
					auto boxColor2 = ImColor(0, 0, 0, 255);

					//DrawNormalBox(RootBoneOut.x - BoxWidth / 2 + 1, vHeadBoneOut.y, BoxWidth, BoxHeight, 1.0f, ImColor(0, 0, 0));
					//DrawNormalBox(RootBoneOut.x - BoxWidth / 2 - 1, vHeadBoneOut.y, BoxWidth, BoxHeight, 1.0f, ImColor(0, 0, 0));
					//DrawNormalBox(RootBoneOut.x - BoxWidth / 2, vHeadBoneOut.y + 1, BoxWidth, BoxHeight, 1.0f, ImColor(0, 0, 0));
					//DrawNormalBox(RootBoneOut.x - BoxWidth / 2, vHeadBoneOut.y - 1, BoxWidth, BoxHeight, 1.0f, ImColor(0, 0, 0));
					DrawNormalBox(RootBoneOut.x - (BoxWidth / 2), vHeadBoneOut.y, BoxWidth, BoxHeight, 1.5f, ImColor(0, 0, 0));
					DrawNormalBox(RootBoneOut.x - (BoxWidth / 2), vHeadBoneOut.y, BoxWidth, BoxHeight, 1.0f, ImColor(66, 245, 242));
				}

				if (Settings::Distance)
				{
					char dist[64];
					char dist2[64];
					sprintf_s(dist, "Player", distance);
					sprintf_s(dist2, "      [%.fm]", distance);
					//sprintf_s(dist2, "Player", distance);
					Background(HeadposW2s.x - 40, HeadposW2s.y - 15, 100, 15, ImColor(0, 0, 0, 130));
					ShadowRGBText(HeadposW2s.x - 35, HeadposW2s.y - 15, ImColor(255, 255, 255, 200), dist);
					ShadowRGBText(HeadposW2s.x - 35, HeadposW2s.y - 15, ImColor(0, 255, 255, 200), dist2);
					//Background(HeadposW2s.x - 40, HeadposW2s.y + 100, 100, 15, ImColor(0, 0, 0, 130));
					//ShadowRGBText(HeadposW2s.x - 35, HeadposW2s.y + 100, ImColor(255, 255, 255, 200), dist);

					//[% .fm]
				}
				if (Settings::OutlinedSkeleton)
				{
					DrawSkeleton3(currentactormesh);
				}
				auto dx = HeadposW2s.x - (Width / 2);
				auto dy = HeadposW2s.y - (Height / 2);
				auto dist = sqrtf(dx * dx + dy * dy);

				if (dist < Settings::AimbotFOV && dist < closestDistance) {
					closestDistance = dist;
					closestPawn = CurrentActor;
				}
			}

			if (Settings::MouseAimbot)
			{
				if (Settings::MouseAimbot && closestPawn && GetAsyncKeyState(VK_RBUTTON) < 0) {
					AimAt(closestPawn);
				}
			}
		}
	}
}
bool IsInLobby()
{
	ULONG Identifier = 10;

	Identifier = read<float>(DrverInit, FNProcID, ((ULONGLONG)base_address + 0x7D393B0));

	drawLoop();
	if (Identifier == 1)
		return true;

	else return false;
}

void ColorChange()
{
	if (rainbowmode)
	{
		static float Color[3];
		static DWORD Tickcount = 0;
		static DWORD Tickcheck = 0;
		ImGui::ColorConvertRGBtoHSV(color_red, color_green, color_blue, Color[0], Color[1], Color[2]);
		if (GetTickCount() - Tickcount >= 1)
		{
			if (Tickcheck != Tickcount)
			{
				Color[0] += 0.001f * color_speed;
				Tickcheck = Tickcount;
			}
			Tickcount = GetTickCount();
		}
		if (Color[0] < 0.0f) Color[0] += 1.0f;
		ImGui::ColorConvertHSVtoRGB(Color[0], Color[1], Color[2], color_red, color_green, color_blue);
	}
}

void Render() {

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	drawLoop();

	int X;
	int Y;
	float size1 = 3.0f;
	float size2 = 2.0f;

	if (rainbowmode)
	{
		ImGui::GetOverlayDrawList()->AddCircle(ImVec2(ScreenCenterX, ScreenCenterY), Settings::AimbotFOV, ImGui::GetColorU32({ color_red, color_green, color_blue, 230 }), 30);
	}

	if (Settings::Reticle)
	{
		ImGui::GetOverlayDrawList()->AddCircle(ImVec2(ScreenCenterX, ScreenCenterY), size1, ImColor(0, 0, 0, 255), 120);
		ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(ScreenCenterX, ScreenCenterY), size2, ImColor(0, 255, 251, 200), 120);
	}
	if (Settings::Crosshair)
	{
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2 - 12, Height / 2), ImVec2(Width / 2 - 2, Height / 2), ImGui::GetColorU32({ 0, 255, 255, 255.f }), 2.0f);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2 + 13, Height / 2), ImVec2(Width / 2 + 3, Height / 2), ImGui::GetColorU32({ 0, 255, 255, 255.f }), 2.0f);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2 - 12), ImVec2(Width / 2, Height / 2 - 2), ImGui::GetColorU32({ 0, 255, 255, 255.f }), 2.0f);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2 + 13), ImVec2(Width / 2, Height / 2 + 3), ImGui::GetColorU32({ 0, 255, 255, 255.f }), 2.0f);
	}
	if (Settings::Radar)
	{
		RadarLoop();
	}

	//wColorChange();

	bool circleedit = false;
	ImGui::SetNextWindowSize({ 300, 300 });
	if (Settings::ShowMenu)
	{

		ImGui::Begin("cobra version 1.1 [INSERT]", 0, ImGuiWindowFlags_::ImGuiWindowFlags_NoResize);
		//ImGui::SetNextWindowSize({300, 300});

		static int fortnitetab;
		ImGuiStyle* Style = &ImGui::GetStyle();
		if (ImGui::Button("aimbot", ImVec2(60, 20))) fortnitetab = 1;
		ImGui::SameLine();
		if (ImGui::Button("visuals", ImVec2(60, 20))) fortnitetab = 2;
		ImGui::SameLine();
		if (ImGui::Button("editing", ImVec2(60, 20))) fortnitetab = 3;
		if (fortnitetab == 1)
		{
			ImGui::Checkbox("Memory Aimbot", &Settings::Memory);
			ImGui::Checkbox("Mouse Aimbot", &Settings::MouseAimbot);
			//ImGui::Checkbox("Random Bone", &Settings::RandomAim);
			ImGui::Checkbox("Aimbot FOV", &Settings::AimbotCircle);
			//ImGui::Checkbox("Edit Circle", &circleedit);
			if (Settings::AimbotCircle)
			{
				ImGui::SliderFloat("Size", &Settings::AimbotFOV, 30, 900);
			}
			if (Settings::MouseAimbot)
			{
				ImGui::Text("making it 100 is like controller assist");
				ImGui::SliderFloat("Smoothing", &Settings::Smoothing, 0, 100);
			}
			//ImGui::Combo("", &Settings::hitboxpos, Settings::hitbox, sizeof(hitboxes) / sizeof(*hitboxes));
			//ImGui::Checkbox("Reticle", &Settings::Reticle);
			ImGui::Checkbox("Crosshair", &Settings::Crosshair);
		}
		if (fortnitetab == 2)
		{
			ImGui::Checkbox("Player Box", &Settings::PlayerESP);
			ImGui::Checkbox("Corner ESP", &Settings::CornerESP);
			ImGui::Checkbox("Distance ESP", &Settings::Distance);
			ImGui::Checkbox("Skeleton ESP", &Settings::Skeleton);
			ImGui::Checkbox("Outlined Skeleton ESP", &Settings::OutlinedSkeleton);
			ImGui::Checkbox("Snaplines", &Settings::Lines);
			ImGui::Checkbox("Self ESP", &Settings::Selfesp);
			//ImGui::Checkbox("3D ESP", &Settings::ThreeDESP);
		}
		ImGui::GetOverlayDrawList()->AddRectFilled(ImGui::GetIO().MousePos, ImVec2(ImGui::GetIO().MousePos.x + 7.f, ImGui::GetIO().MousePos.y + 7.f), ImColor(r1, g2, b2, 255));
		if (fortnitetab == 3)
		{
			ImGui::Text("Block Cursor");
			ImGui::SliderInt("Reds ", &r1, 1, 255);
			ImGui::SliderInt("Greens ", &g2, 1, 255);
			ImGui::SliderInt("Blues ", &b2, 1, 255);
			ImGui::ColorPicker4("box color", (float*)&Settings::BoxCol);
			ImGui::ColorPicker4("line color", (float*)&Settings::LineCol);
		}
		ImGui::End();
	}

	ImGui::EndFrame();
	D3DDevice->SetRenderState(D3DRS_ZENABLE, false);
	D3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	D3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	D3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

	if (D3DDevice->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		D3DDevice->EndScene();
	}
	HRESULT Results = D3DDevice->Present(NULL, NULL, NULL, NULL);

	if (Results == D3DERR_DEVICELOST && D3DDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		D3DDevice->Reset(&d3d);
		ImGui_ImplDX9_CreateDeviceObjects();
	}
}


MSG Message_Loop = { NULL };

void Loop()
{
	static RECT old_rc;
	ZeroMemory(&Message_Loop, sizeof(MSG));

	while (Message_Loop.message != WM_QUIT)
	{
		if (PeekMessage(&Message_Loop, Window, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Message_Loop);
			DispatchMessage(&Message_Loop);
		}

		HWND hwnd_active = GetForegroundWindow();

		if (hwnd_active == hWnd) {
			HWND hwndtest = GetWindow(hwnd_active, GW_HWNDPREV);
			SetWindowPos(Window, hwndtest, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}

		if (GetAsyncKeyState(0x23) & 1)
			exit(8);

		RECT rc;
		POINT xy;

		ZeroMemory(&rc, sizeof(RECT));
		ZeroMemory(&xy, sizeof(POINT));
		GetClientRect(hWnd, &rc);
		ClientToScreen(hWnd, &xy);
		rc.left = xy.x;
		rc.top = xy.y;

		ImGuiIO& io = ImGui::GetIO();
		io.ImeWindowHandle = hWnd;
		io.DeltaTime = 1.0f / 60.0f;

		POINT p;
		GetCursorPos(&p);
		io.MousePos.x = p.x - xy.x;
		io.MousePos.y = p.y - xy.y;

		if (GetAsyncKeyState(VK_LBUTTON)) {
			io.MouseDown[0] = true;
			io.MouseClicked[0] = true;
			io.MouseClickedPos[0].x = io.MousePos.x;
			io.MouseClickedPos[0].x = io.MousePos.y;
		}
		else
			io.MouseDown[0] = false;

		if (rc.left != old_rc.left || rc.right != old_rc.right || rc.top != old_rc.top || rc.bottom != old_rc.bottom)
		{
			old_rc = rc;

			Width = rc.right;
			Height = rc.bottom;

			d3d.BackBufferWidth = Width;
			d3d.BackBufferHeight = Height;
			SetWindowPos(Window, (HWND)0, xy.x, xy.y, Width, Height, SWP_NOREDRAW);
			D3DDevice->Reset(&d3d);
		}
		Render();
	}
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	DestroyWindow(Window);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam))
		return true;

	switch (Message)
	{
	case WM_DESTROY:
		ShutDown();
		PostQuitMessage(0);
		exit(4);
		break;
	case WM_SIZE:
		if (D3DDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			d3d.BackBufferWidth = LOWORD(lParam);
			d3d.BackBufferHeight = HIWORD(lParam);
			HRESULT hr = D3DDevice->Reset(&d3d);
			if (hr == D3DERR_INVALIDCALL)
				IM_ASSERT(0);
			ImGui_ImplDX9_CreateDeviceObjects();
		}
		break;
	default:
		return DefWindowProc(hWnd, Message, wParam, lParam);
		break;
	}
	return 0;
}


void ShutDown()
{
	VertBuff->Release();
	D3DDevice->Release();
	pObject->Release();

	DestroyWindow(Window);
	UnregisterClass(L"fgers", NULL);
}
