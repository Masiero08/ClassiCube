#include "../Window.h"
#include "../Platform.h"
#include "../Input.h"
#include "../Event.h"
#include "../Graphics.h"
#include "../String_.h"
#include "../Funcs.h"
#include "../Bitmap.h"
#include "../Errors.h"
#include "../ExtMath.h"
#include "../Gui.h"
#include "../Chat.h"
#include <malloc.h>

#include <3ds.h>

static u16 top_width, top_height;
static u16 btm_width, btm_height;
#include "../VirtualKeyboard.h"

struct _DisplayData DisplayInfo;
struct cc_window WindowInfo;
struct cc_window Window_Alt;
cc_bool launcherTop;

void Window_PreInit(void) {
	gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false);
}

// Note from https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/gfx.h
//  * Please note that the 3DS uses *portrait* screens rotated 90 degrees counterclockwise.
//  * Width/height refer to the physical dimensions of the screen; that is, the top screen
//  * is 240 pixels wide and 400 pixels tall; while the bottom screen is 240x320.
void Window_Init(void) {
	// deliberately swapped
	gfxGetFramebuffer(GFX_TOP,    GFX_LEFT, &top_height, &top_width);
	gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &btm_height, &btm_width);
	
	DisplayInfo.Width  = top_width; 
	DisplayInfo.Height = top_height;
	DisplayInfo.ScaleX = 0.5f;
	DisplayInfo.ScaleY = 0.5f;
	
	Window_Main.Width    = top_width;
	Window_Main.Height   = top_height;
	Window_Main.Focused  = true;
	
	Window_Main.Exists   = true;
	Window_Main.UIScaleX = DEFAULT_UI_SCALE_X;
	Window_Main.UIScaleY = DEFAULT_UI_SCALE_Y;

	Window_Main.SoftKeyboard = SOFT_KEYBOARD_RESIZE;
	Input_SetTouchMode(true);
	Gui_SetTouchUI(true);
	
	Window_Alt.Width  = btm_width;
	Window_Alt.Height = btm_height;
}

static Result irrst_result;
static void CPP_Free(void);

// libctru would otherwise start ir:rst in hidInit, which blocks ir:USER (Circle Pad Pro on Old 3DS)
bool hidShouldUseIrrst(void) { return false; }

void Window_Free(void) {
	if (irrst_result == 0) irrstExit();
	CPP_Free();
}

void Window_Create2D(int width, int height) {
	Window_Main.Is3D  = false;
	Window_Alt.Is3D   = false;

	DisplayInfo.Width = btm_width;
	Window_Main.Width = btm_width;
	Window_Alt.Width  = top_width;
}

void Window_Create3D(int width, int height) { 
	Window_Main.Is3D  = true;
	Window_Alt.Is3D   = true;

	DisplayInfo.Width = top_width;
	Window_Main.Width = top_width;
	Window_Alt.Width  = btm_width;
}

void Window_Destroy(void) { }

void Window_SetTitle(const cc_string* title) { }
void Clipboard_GetText(cc_string* value) { }
void Clipboard_SetText(const cc_string* value) { }

int Window_GetWindowState(void) { return WINDOW_STATE_FULLSCREEN; }
cc_result Window_EnterFullscreen(void) { return 0; }
cc_result Window_ExitFullscreen(void)  { return 0; }
int Window_IsObscured(void)            { return 0; }

void Window_Show(void) { }
void Window_SetSize(int width, int height) { }

void Window_RequestClose(void) {
	Event_RaiseVoid(&WindowEvents.Closing);
}

/*########################################################################################################################*
*----------------------------------------------------Input processing-----------------------------------------------------*
*#########################################################################################################################*/
static void ProcessTouchInput(int mods) {
	touchPosition touch;
	hidTouchRead(&touch);

	if (mods & KEY_TOUCH) {
		Input_AddTouch(0,    touch.px,      touch.py);
	} else if (hidKeysUp() & KEY_TOUCH) {
		Input_RemoveTouch(0, Pointers[0].x, Pointers[0].y);
	}
}

void Window_ProcessEvents(float delta) {
	hidScanInput();

	if (!aptMainLoop()) {
		Window_Main.Exists = false;
		Window_RequestClose();
		return;
	}
	
	u32 mods = hidKeysDown() | hidKeysHeld();
	ProcessTouchInput(mods);
}

void Cursor_SetPosition(int x, int y) { } // Makes no sense for 3DS

void Window_EnableRawMouse(void)  { Input.RawMode = true;  }
void Window_DisableRawMouse(void) { Input.RawMode = false; }

void Window_UpdateRawMouse(void)  { }


/*########################################################################################################################*
*-------------------------------------------------------Gamepads----------------------------------------------------------*
*#########################################################################################################################*/
static const BindMapping defaults_3ds[BIND_COUNT] = {
	[BIND_FORWARD]      = { CCPAD_UP    },  
	[BIND_BACK]         = { CCPAD_DOWN  },
	[BIND_LEFT]         = { CCPAD_LEFT  },  
	[BIND_RIGHT]        = { CCPAD_RIGHT },
	[BIND_JUMP]         = { CCPAD_1     },
	[BIND_SET_SPAWN]    = { CCPAD_START }, 
	[BIND_CHAT]         = { CCPAD_4     },
	[BIND_INVENTORY]    = { CCPAD_3     },
	[BIND_SEND_CHAT]    = { CCPAD_START },
	[BIND_PLACE_BLOCK]  = { CCPAD_L     },
	[BIND_DELETE_BLOCK] = { CCPAD_R     },
	[BIND_SPEED]        = { CCPAD_2, CCPAD_L },
	[BIND_FLY]          = { CCPAD_2, CCPAD_R },
	[BIND_NOCLIP]       = { CCPAD_2, CCPAD_3 },
	[BIND_FLY_UP]       = { CCPAD_2, CCPAD_UP },
	[BIND_FLY_DOWN]     = { CCPAD_2, CCPAD_DOWN },
	[BIND_HOTBAR_LEFT]  = { CCPAD_ZL    }, 
	[BIND_HOTBAR_RIGHT] = { CCPAD_ZR    }
};

static Result irrst_result = -1;
static cc_bool isNew3DS;

/*########################################################################################################################*
*---------------------------------------------Circle Pad Pro (Old 3DS, ir:USER)-------------------------------------------*
*#########################################################################################################################*/
// On the Old 3DS the Circle Pad Pro is an infrared accessory. Games such as Resident Evil Revelations
//  talk to it through the ir:USER service: connect to device 1, ask it to send its inputs every few ms,
//  then read the 6 byte input packets it sends back from shared memory.
// ir:USER and ir:rst cannot be used at the same time, so ir:rst is only used on the New 3DS (C-Stick).
// Protocol details: https://www.3dbrew.org/wiki/Circle_Pad_Pro and https://www.3dbrew.org/wiki/IR_Services
#define CPP_RECV_PACKETS  16
#define CPP_PACKET_SIZE   32
#define CPP_RECV_SIZE     (CPP_RECV_PACKETS * (8 + CPP_PACKET_SIZE))
#define CPP_SEND_PACKETS  4
#define CPP_SEND_SIZE     (CPP_SEND_PACKETS * (8 + CPP_PACKET_SIZE))
#define CPP_SHAREDMEM_SIZE 0x1000
#define CPP_RECV_OFFSET   0x20

enum CPP_STATE { CPP_NONE, CPP_IDLE, CPP_CONNECTING, CPP_CONNECTED };
static Handle cpp_handle, cpp_memHandle;
static u8* cpp_mem;
static int cpp_state;
static float cpp_timer, cpp_lastPacket, cpp_lastRequest;
static cc_bool cpp_hasInput, cpp_centred, cpp_announced;
static int cpp_rawX, cpp_rawY, cpp_centreX = 0x800, cpp_centreY = 0x800;
static u32 cpp_buttons;
// Diagnostics shown in chat, so a test on the console tells us how far the connection got
static Result cpp_initError;
static const char* cpp_initStep;
static cc_bool cpp_toldInit, cpp_toldLink;

static Result CPP_Simple(u32 header, int argc, u32 arg) {
	u32* cmd = getThreadCommandBuffer();
	Result res;
	cmd[0] = header;
	if (argc) cmd[1] = arg;

	if ((res = svcSendSyncRequest(cpp_handle))) return res;
	return (Result)cmd[1];
}

static Result CPP_InitShared(void) {
	u32* cmd = getThreadCommandBuffer();
	Result res;
	cmd[0] = IPC_MakeHeader(0x18, 6, 2); // InitializeIrNopShared
	cmd[1] = CPP_SHAREDMEM_SIZE;
	cmd[2] = CPP_RECV_SIZE;
	cmd[3] = CPP_RECV_PACKETS;
	cmd[4] = CPP_SEND_SIZE;
	cmd[5] = CPP_SEND_PACKETS;
	cmd[6] = 4; // 96000 bps, what the Circle Pad Pro uses
	cmd[7] = IPC_Desc_SharedHandles(1);
	cmd[8] = cpp_memHandle;

	if ((res = svcSendSyncRequest(cpp_handle))) return res;
	return (Result)cmd[1];
}

static Result CPP_Send(const u8* data, u32 size) {
	u32* cmd = getThreadCommandBuffer();
	Result res;
	cmd[0] = IPC_MakeHeader(0x0D, 1, 2); // SendIrNop
	cmd[1] = size;
	cmd[2] = IPC_Desc_StaticBuffer(size, 0);
	cmd[3] = (u32)data;

	if ((res = svcSendSyncRequest(cpp_handle))) return res;
	return (Result)cmd[1];
}

#define CPP_RequireConnection() CPP_Simple(IPC_MakeHeader(0x06, 1, 0), 1, 1) // device 1 = Circle Pad Pro
#define CPP_Disconnect()        CPP_Simple(IPC_MakeHeader(0x09, 0, 0), 0, 0)
#define CPP_Finalize()          CPP_Simple(IPC_MakeHeader(0x02, 0, 0), 0, 0)
#define CPP_Release(count)      CPP_Simple(IPC_MakeHeader(0x19, 1, 0), 1, count)

static void CPP_Init(void) {
	Result res;
	cpp_mem = (u8*)memalign(0x1000, CPP_SHAREDMEM_SIZE);
	if (!cpp_mem) return;
	Mem_Set(cpp_mem, 0, CPP_SHAREDMEM_SIZE);

	if ((res = srvGetServiceHandle(&cpp_handle, "ir:USER"))) {
		cpp_initError = res; cpp_initStep = "servico";
		Platform_Log1("ir:USER unavailable: %e", &res); return;
	}
	if ((res = svcCreateMemoryBlock(&cpp_memHandle, (u32)cpp_mem, CPP_SHAREDMEM_SIZE,
									MEMPERM_READ, MEMPERM_READWRITE))) {
		cpp_initError = res; cpp_initStep = "memoria";
		Platform_Log1("ir:USER memblock failed: %e", &res); return;
	}
	if ((res = CPP_InitShared())) {
		cpp_initError = res; cpp_initStep = "iniciar";
		Platform_Log1("ir:USER init failed: %e", &res); return;
	}
	cpp_state = CPP_IDLE;
}

static void CPP_Free(void) {
	if (cpp_state == CPP_NONE) return;
	CPP_Disconnect();
	CPP_Finalize();
	svcCloseHandle(cpp_memHandle);
	svcCloseHandle(cpp_handle);
	cpp_state = CPP_NONE;
}

static void CPP_RequestInputs(void) {
	// Request 1 = read inputs: [1, period in ms, unknown (what official games send)]
	static u8 request[3] = { 1, 8, (8 + 2) << 2 };
	CPP_Send(request, 3);
	cpp_lastRequest = 0;
}

static void CPP_Announce(const char* msg) {
	cc_string str = String_FromReadonly(msg);
	if (!Gui.ScreensCount) return; // still in the launcher
	Chat_AddOf(&str, MSG_TYPE_SMALLANNOUNCEMENT);
}

// Parses the payload of an IR packet that starts at 'offset' in the data area
static void CPP_ParsePacket(u8* data, u32 dataSize, u32 offset, u32 length) {
	u8 hdr[4], p[6];
	u32 i, payloadBeg, payloadLen;
	if (length < 4) return;

	for (i = 0; i < 4; i++) hdr[i] = data[(offset + i) % dataSize];
	if (hdr[0] != 0xA5) return;

	if (hdr[2] & 0x40) {
		payloadBeg = 4;
		payloadLen = ((hdr[2] & 0x3F) << 8) | hdr[3];
	} else {
		payloadBeg = 3;
		payloadLen = hdr[2];
	}
	if (payloadLen != 6 || length < payloadBeg + payloadLen) return;

	for (i = 0; i < 6; i++) p[i] = data[(offset + payloadBeg + i) % dataSize];
	if (p[0] != 0x10) return; // not an input response

	cpp_rawX    = p[1] | ((p[2] & 0x0F) << 8);
	cpp_rawY    = (p[2] >> 4) | (p[3] << 4);
	cpp_buttons = 0;
	// Button bits are 1 when NOT pressed
	if (!(p[4] & 0x20)) cpp_buttons |= KEY_ZL;
	if (!(p[4] & 0x40)) cpp_buttons |= KEY_ZR;
	if (!(p[4] & 0x80)) cpp_buttons |= KEY_R;

	// The stick rests near 0x800, but every unit is slightly off, so
	//  use the first reading as the centre when the stick is not being held
	if (!cpp_centred) {
		if (Math_AbsI(cpp_rawX - 0x800) < 0x180 && Math_AbsI(cpp_rawY - 0x800) < 0x180) {
			cpp_centreX = cpp_rawX; cpp_centreY = cpp_rawY;
		}
		cpp_centred = true;
	}
	cpp_hasInput   = true;
	cpp_lastPacket = 0;
}

static void CPP_ReadPackets(void) {
	u8* recv  = cpp_mem + CPP_RECV_OFFSET;
	u8* data  = recv + CPP_RECV_PACKETS * 8;
	u32 dataSize = CPP_RECV_SIZE - CPP_RECV_PACKETS * 8;
	u32 start = *(volatile u32*)(cpp_mem + 0x10);
	u32 count = *(volatile u32*)(cpp_mem + 0x18);
	u32 i, idx, offset, length;

	if (!count) return;
	if (count > CPP_RECV_PACKETS) count = CPP_RECV_PACKETS;

	for (i = 0; i < count; i++)
	{
		idx    = (start + i) % CPP_RECV_PACKETS;
		offset = *(u32*)(recv + idx * 8 + 0);
		length = *(u32*)(recv + idx * 8 + 4);
		CPP_ParsePacket(data, dataSize, offset % dataSize, length);
	}
	// Free up the receive buffer, otherwise new packets stop arriving
	CPP_Release(count);
}

static void CPP_Reset(void) {
	CPP_Disconnect();
	cpp_state    = CPP_IDLE;
	cpp_timer    = 0;
	cpp_hasInput = false;
	cpp_buttons  = 0;
}

static void CPP_Update(float delta) {
	u8 status;
	if (!cpp_toldInit && Gui.ScreensCount) {
		cpp_toldInit = true;
		if (cpp_initStep) {
			cc_string msg; char buf[96];
			String_InitArray(msg, buf);
			String_Format2(&msg, "&cCircle Pad Pro: erro no infravermelho (%c %h)", cpp_initStep, &cpp_initError);
			Chat_AddOf(&msg, MSG_TYPE_SMALLANNOUNCEMENT);
		}
	}
	if (cpp_state == CPP_NONE) return;
	status = *(volatile u8*)(cpp_mem + 0x08); // 0 = disconnected, 1 = connecting, 2 = connected
	cpp_timer += delta;

	switch (cpp_state)
	{
	case CPP_IDLE:
		// Look for the accessory every 1.5 seconds
		if (cpp_timer < 1.5f) break;
		cpp_timer = 0;
		if (!CPP_RequireConnection()) cpp_state = CPP_CONNECTING;
		break;

	case CPP_CONNECTING:
		if (status == 2) {
			if (!cpp_toldLink) CPP_Announce("&eCircle Pad Pro encontrado, lendo os botoes...");
			cpp_toldLink = true;
			cpp_state      = CPP_CONNECTED;
			cpp_timer      = 0;
			cpp_lastPacket = 0;
			cpp_centred    = false;
			CPP_RequestInputs();
		} else if (cpp_timer > 3.0f) {
			// the connection handshake can take a while on real hardware
			CPP_Reset(); // nothing answered, try again later
		}
		break;

	case CPP_CONNECTED:
		cpp_lastPacket  += delta;
		cpp_lastRequest += delta;
		CPP_ReadPackets();

		if (status != 2 || cpp_lastPacket > 2.0f) {
			if (cpp_announced) CPP_Announce("&eCircle Pad Pro desconectado");
			cpp_announced = false;
			CPP_Reset();
			break;
		}
		if (cpp_hasInput && !cpp_announced) {
			CPP_Announce("&aCircle Pad Pro conectado: analogico direito gira a camera, ZL coloca, ZR quebra, L/R trocam o item");
			cpp_announced = true;
		}
		// Keep reminding it that we are listening (also retries the first request)
		if (cpp_lastRequest > (cpp_hasInput ? 0.5f : 0.1f)) CPP_RequestInputs();
		break;
	}
}

// Fills in the Circle Pad Pro stick, in the same units as the normal Circle Pad (about -156 to 156)
static void CPP_GetStick(circlePosition* pos) {
	int dx = (cpp_rawX - cpp_centreX) / 8;
	int dy = (cpp_rawY - cpp_centreY) / 8;
	Math_Clamp(dx, -156, 156);
	Math_Clamp(dy, -156, 156);
	pos->dx = (s16)dx;
	pos->dy = (s16)dy;
}


/*########################################################################################################################*
*-------------------------------------------------------Gamepads----------------------------------------------------------*
*#########################################################################################################################*/
void Gamepads_PreInit(void) {
	bool isNew = false;
	APT_CheckNew3DS(&isNew);
	isNew3DS = isNew;

	if (isNew3DS) {
		// C-Stick and ZL/ZR on the New 3DS come from ir:rst
		irrst_result = irrstInit();
	} else {
		CPP_Init();
	}
}

void Gamepads_Init(void) { }

static void HandleButtons(int port, u32 mods) {
	Gamepad_SetButton(port, CCPAD_L, mods & KEY_L);
	Gamepad_SetButton(port, CCPAD_R, mods & KEY_R);
	
	Gamepad_SetButton(port, CCPAD_1, mods & KEY_A);
	Gamepad_SetButton(port, CCPAD_2, mods & KEY_B);
	Gamepad_SetButton(port, CCPAD_3, mods & KEY_X);
	Gamepad_SetButton(port, CCPAD_4, mods & KEY_Y);
	
	Gamepad_SetButton(port, CCPAD_START,  mods & KEY_START);
	Gamepad_SetButton(port, CCPAD_SELECT, mods & KEY_SELECT);
	
	Gamepad_SetButton(port, CCPAD_LEFT,   mods & KEY_DLEFT);
	Gamepad_SetButton(port, CCPAD_RIGHT,  mods & KEY_DRIGHT);
	Gamepad_SetButton(port, CCPAD_UP,     mods & KEY_DUP);
	Gamepad_SetButton(port, CCPAD_DOWN,   mods & KEY_DDOWN);
	
	Gamepad_SetButton(port, CCPAD_ZL, mods & KEY_ZL);
	Gamepad_SetButton(port, CCPAD_ZR, mods & KEY_ZR);
}

#define AXIS_SCALE 8.0f
#define AXIS_DEADZONE 20
#define AXIS_MAX 156.0f
static void ProcessCircleInput(int port, int axis, circlePosition* pos, float delta) {
	// May not be exactly 0 on actual hardware
	float x = Math_AbsI(pos->dx) <= AXIS_DEADZONE ? 0 : pos->dx;
	float y = Math_AbsI(pos->dy) <= AXIS_DEADZONE ? 0 : pos->dy;
	Gamepad_SetAxis(port, axis, x / AXIS_SCALE, -y / AXIS_SCALE, delta);
}

// Camera stick: slow near the centre for precise aiming, full speed at the edge
//  (like the camera in Resident Evil Revelations / Luigi's Mansion with the Circle Pad Pro)
static float CameraCurve(int v) {
	float n, a = (float)Math_AbsI(v);
	if (a <= AXIS_DEADZONE) return 0;

	n = (a - AXIS_DEADZONE) / (AXIS_MAX - AXIS_DEADZONE);
	if (n > 1.0f) n = 1.0f;
	n = n * (0.35f + 0.65f * n);
	return (v < 0 ? -n : n) * (AXIS_MAX / AXIS_SCALE) * 1.2f;
}

static void ProcessCameraStick(int port, circlePosition* pos, float delta) {
	Gamepad_SetAxis(port, PAD_AXIS_RIGHT, CameraCurve(pos->dx), -CameraCurve(pos->dy), delta);
}

// Set once the C-Stick (New 3DS) has been used or the Circle Pad Pro (Old 3DS) is connected
static cc_bool rightStickSeen;

// Without a right stick, the D-Pad turns the camera instead (the Circle Pad always walks)
static void DPadToCamera(int port, u32* mods, float delta) {
	circlePosition pos = { 0, 0 };
	u32 keys = *mods;
	// B + Up/Down still flies up/down
	if (keys & KEY_B) return;

	if (keys & KEY_DLEFT)  pos.dx -= 110;
	if (keys & KEY_DRIGHT) pos.dx += 110;
	if (keys & KEY_DUP)    pos.dy += 110;
	if (keys & KEY_DDOWN)  pos.dy -= 110;
	*mods &= ~(KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN);
	ProcessCameraStick(port, &pos, delta);
}

void Gamepads_Process(float delta) {
	u32 mods = hidKeysDown() | hidKeysHeld();
	circlePosition hid_pos, stk_pos = { 0 };
	int port = Gamepad_Connect(0x3D5, defaults_3ds);

	if (isNew3DS && irrst_result == 0) {
		irrstScanInput();
		mods |= irrstKeysHeld();
		irrstCstickRead(&stk_pos);
		if (Math_AbsI(stk_pos.dx) > 24 || Math_AbsI(stk_pos.dy) > 24) rightStickSeen = true;
	} else if (!isNew3DS) {
		CPP_Update(delta);
		if (cpp_state == CPP_CONNECTED && cpp_hasInput) {
			mods |= cpp_buttons;
			CPP_GetStick(&stk_pos);
		}
		rightStickSeen = cpp_announced;
	}
	hidCircleRead(&hid_pos);

	// Circle Pad (left) always walks, like Resident Evil Revelations / Monster Hunter
	ProcessCircleInput(port, PAD_AXIS_LEFT, &hid_pos, delta);

	if (rightStickSeen) {
		// Right stick (C-Stick or Circle Pad Pro) turns the camera, D-Pad works as normal
		ProcessCameraStick(port, &stk_pos, delta);
	} else {
		DPadToCamera(port, &mods, delta);
	}

	// With a right stick (Circle Pad Pro / New 3DS) ZL and ZR exist: then, like
	//  console shooters, ZL places/uses, ZR breaks/attacks and L/R change the hotbar slot.
	//  Without one, L/R keep placing/breaking (otherwise nothing could be broken)
	if (rightStickSeen) {
		u32 shoulders = mods & (KEY_L | KEY_R | KEY_ZL | KEY_ZR);
		mods &= ~(KEY_L | KEY_R | KEY_ZL | KEY_ZR);
		if (shoulders & KEY_ZL) mods |= KEY_L;  // place / use / eat
		if (shoulders & KEY_ZR) mods |= KEY_R;  // break / attack
		if (shoulders & KEY_L)  mods |= KEY_ZL; // hotbar left
		if (shoulders & KEY_R)  mods |= KEY_ZR; // hotbar right
	}
	HandleButtons(port, mods);
}


/*########################################################################################################################*
*------------------------------------------------------Framebuffer--------------------------------------------------------*
*#########################################################################################################################*/
void Window_AllocFramebuffer(struct Bitmap* bmp, int width, int height) {
	bmp->scan0  = (BitmapCol*)Mem_Alloc(width * height, BITMAPCOLOR_SIZE, "window pixels");
	bmp->width  = width;
	bmp->height = height;
}

void Window_DrawFramebuffer(Rect2D r, struct Bitmap* bmp) {
	u16 width, height;
	gfxScreen_t screen = launcherTop ? GFX_TOP : GFX_BOTTOM;
	
	gfxSetDoubleBuffering(screen, false);
	u8* fb = gfxGetFramebuffer(screen, GFX_LEFT, &width, &height);
	// SRC y = 0 to 240
	// SRC x = 0 to 400
	// DST X = 0 to 240
	// DST Y = 0 to 400

	for (int y = r.y; y < r.y + r.height; y++)
		for (int x = r.x; x < r.x + r.width; x++)
	{
		BitmapCol color = Bitmap_GetPixel(bmp, x, y);
		int addr   = (width - 1 - y + x * width) * 3; // TODO -1 or not
		fb[addr+0] = BitmapCol_B(color);
		fb[addr+1] = BitmapCol_G(color);
		fb[addr+2] = BitmapCol_R(color);
	}
	// TODO gspWaitForVBlank();
	gfxFlushBuffers();
	//gfxSwapBuffers();
	// TODO: tearing??
	/*
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxScreenSwapBuffers(GFX_TOP, true);
	gfxSetDoubleBuffering(GFX_TOP, true);
	gfxScreenSwapBuffers(GFX_BOTTOM, true);
	*/
}

void Window_FreeFramebuffer(struct Bitmap* bmp) {
	Mem_Free(bmp->scan0);
}


/*########################################################################################################################*
*------------------------------------------------------Soft keyboard------------------------------------------------------*
*#########################################################################################################################*/
void OnscreenKeyboard_Open(struct OpenKeyboardArgs* args) {
	kb_tileWidth  = 20;
	kb_tileHeight = 20;

	VirtualKeyboard_Open(args);
}

void OnscreenKeyboard_SetText(const cc_string* text) {
	VirtualKeyboard_SetText(text);
}

void OnscreenKeyboard_Close(void) {
	VirtualKeyboard_Close();
}


/*########################################################################################################################*
*-------------------------------------------------------Misc/Other--------------------------------------------------------*
*#########################################################################################################################*/
void Window_ShowDialog(const char* title, const char* msg) {
	/* TODO implement */
	Platform_LogConst(title);
	Platform_LogConst(msg);
}

cc_result Window_OpenFileDialog(const struct OpenFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}

cc_result Window_SaveFileDialog(const struct SaveFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}

