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
#include "../World.h"
#include "../TexturePack.h"
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
// On the Old 3DS the Circle Pad Pro is an infrared accessory, talked to through the ir:USER service.
// The protocol (as used by official games, and by Red Viper / tristangnl's homebrew example):
//  1) InitializeIrNopShared, then RequireConnection(1) and wait on the connection status event
//  2) ask for the calibration data (request 0x02 -> response 0x11)
//  3) then, over and over: send an input request (0x01) and wait for ONE input response (0x10).
//     The accessory answers each request once; it does not stream on its own.
// Everything runs on a background thread with blocking waits, so the game loop never stalls.
// Protocol details: https://www.3dbrew.org/wiki/Circle_Pad_Pro and https://www.3dbrew.org/wiki/IR_Services
#define CPP_MS 1000000LL
#define CPP_SHAREDMEM_SIZE 0x1000
#define CPP_RECV_DATA      2000
#define CPP_RECV_PACKETS   0xA0
#define CPP_RECV_SIZE      (CPP_RECV_DATA + CPP_RECV_PACKETS * 8)
#define CPP_SEND_PACKETS   0x20
#define CPP_SEND_SIZE      (0x200 + CPP_SEND_PACKETS * 8)
#define CPP_RECV_TABLE     0x20 // packet info table (offset, size) in shared memory

static Handle cpp_handle, cpp_memHandle, cpp_exitEvent;
static u8* cpp_mem;
static Thread cpp_thread;
static int cpp_packetId;

static volatile cc_bool cpp_connected; // calibrated and answering
static volatile u32 cpp_buttons;
static volatile s16 cpp_dx, cpp_dy;
static volatile int cpp_stage; // for the chat messages
enum { CPP_STAGE_NONE, CPP_STAGE_SEARCHING, CPP_STAGE_FOUND, CPP_STAGE_CONNECTED, CPP_STAGE_LOST };

static int cpp_xOffset = 0x800, cpp_yOffset = 0x800;
static float cpp_xScale = 1.0f, cpp_yScale = 1.0f;

// Diagnostics shown in chat, so a test on the console tells us how far the connection got
static Result cpp_initError;
static const char* cpp_initStep;
static cc_bool cpp_toldInit, cpp_announced;
static int cpp_toldStage;

static const u8 crc8_table[256] = {
	0x00,0x07,0x0E,0x09,0x1C,0x1B,0x12,0x15,0x38,0x3F,0x36,0x31,0x24,0x23,0x2A,0x2D,
	0x70,0x77,0x7E,0x79,0x6C,0x6B,0x62,0x65,0x48,0x4F,0x46,0x41,0x54,0x53,0x5A,0x5D,
	0xE0,0xE7,0xEE,0xE9,0xFC,0xFB,0xF2,0xF5,0xD8,0xDF,0xD6,0xD1,0xC4,0xC3,0xCA,0xCD,
	0x90,0x97,0x9E,0x99,0x8C,0x8B,0x82,0x85,0xA8,0xAF,0xA6,0xA1,0xB4,0xB3,0xBA,0xBD,
	0xC7,0xC0,0xC9,0xCE,0xDB,0xDC,0xD5,0xD2,0xFF,0xF8,0xF1,0xF6,0xE3,0xE4,0xED,0xEA,
	0xB7,0xB0,0xB9,0xBE,0xAB,0xAC,0xA5,0xA2,0x8F,0x88,0x81,0x86,0x93,0x94,0x9D,0x9A,
	0x27,0x20,0x29,0x2E,0x3B,0x3C,0x35,0x32,0x1F,0x18,0x11,0x16,0x03,0x04,0x0D,0x0A,
	0x57,0x50,0x59,0x5E,0x4B,0x4C,0x45,0x42,0x6F,0x68,0x61,0x66,0x73,0x74,0x7D,0x7A,
	0x89,0x8E,0x87,0x80,0x95,0x92,0x9B,0x9C,0xB1,0xB6,0xBF,0xB8,0xAD,0xAA,0xA3,0xA4,
	0xF9,0xFE,0xF7,0xF0,0xE5,0xE2,0xEB,0xEC,0xC1,0xC6,0xCF,0xC8,0xDD,0xDA,0xD3,0xD4,
	0x69,0x6E,0x67,0x60,0x75,0x72,0x7B,0x7C,0x51,0x56,0x5F,0x58,0x4D,0x4A,0x43,0x44,
	0x19,0x1E,0x17,0x10,0x05,0x02,0x0B,0x0C,0x21,0x26,0x2F,0x28,0x3D,0x3A,0x33,0x34,
	0x4E,0x49,0x40,0x47,0x52,0x55,0x5C,0x5B,0x76,0x71,0x78,0x7F,0x6A,0x6D,0x64,0x63,
	0x3E,0x39,0x30,0x37,0x22,0x25,0x2C,0x2B,0x06,0x01,0x08,0x0F,0x1A,0x1D,0x14,0x13,
	0xAE,0xA9,0xA0,0xA7,0xB2,0xB5,0xBC,0xBB,0x96,0x91,0x98,0x9F,0x8A,0x8D,0x84,0x83,
	0xDE,0xD9,0xD0,0xD7,0xC2,0xC5,0xCC,0xCB,0xE6,0xE1,0xE8,0xEF,0xFA,0xFD,0xF4,0xF3,
};

static Result CPP_Call(int argc) {
	Result res;
	if ((res = svcSendSyncRequest(cpp_handle))) return res;
	return (Result)getThreadCommandBuffer()[1];
}

static Result CPP_Simple(u32 header, int argc, u32 arg) {
	u32* cmd = getThreadCommandBuffer();
	cmd[0] = header;
	if (argc) cmd[1] = arg;
	return CPP_Call(argc);
}

#define CPP_Finalize()          CPP_Simple(IPC_MakeHeader(0x02, 0, 0), 0, 0)
#define CPP_ClearRecv()         CPP_Simple(IPC_MakeHeader(0x03, 0, 0), 0, 0)
#define CPP_ClearSend()         CPP_Simple(IPC_MakeHeader(0x04, 0, 0), 0, 0)
#define CPP_RequireConnection() CPP_Simple(IPC_MakeHeader(0x06, 1, 0), 1, 1) // device 1 = Circle Pad Pro
#define CPP_Disconnect()        CPP_Simple(IPC_MakeHeader(0x09, 0, 0), 0, 0)
#define CPP_Release(count)      CPP_Simple(IPC_MakeHeader(0x19, 1, 0), 1, count)

static Result CPP_GetEvent(int cmdId, Handle* out) {
	u32* cmd = getThreadCommandBuffer();
	Result res;
	cmd[0] = IPC_MakeHeader(cmdId, 0, 0); // 0x0A = receive event, 0x0C = connection status event
	*out   = 0;
	if ((res = svcSendSyncRequest(cpp_handle))) return res;
	*out = (Handle)cmd[3];
	return (Result)cmd[1];
}

static Result CPP_InitShared(void) {
	u32* cmd = getThreadCommandBuffer();
	cmd[0] = IPC_MakeHeader(0x18, 6, 2); // InitializeIrNopShared
	cmd[1] = CPP_SHAREDMEM_SIZE;
	cmd[2] = CPP_RECV_SIZE;
	cmd[3] = CPP_RECV_PACKETS;
	cmd[4] = CPP_SEND_SIZE;
	cmd[5] = CPP_SEND_PACKETS;
	cmd[6] = 4; // baud rate used by the Circle Pad Pro
	cmd[7] = IPC_Desc_SharedHandles(1);
	cmd[8] = cpp_memHandle;
	return CPP_Call(6);
}

static Result CPP_Send(const void* data, u32 size) {
	u32* cmd = getThreadCommandBuffer();
	cmd[0] = IPC_MakeHeader(0x0D, 1, 2); // SendIrNop
	cmd[1] = size;
	cmd[2] = IPC_Desc_StaticBuffer(size, 0);
	cmd[3] = (u32)data;
	return CPP_Call(1);
}

static void CPP_NextPacket(void) {
	CPP_Release(1);
	cpp_packetId = (cpp_packetId + 1) % CPP_RECV_PACKETS;
}

// Reads the payload of the next received packet into 'out'. Returns payload length, 0 if none/bad
static int CPP_ReadPacket(u8* out, int maxLen) {
	u8* table = cpp_mem + CPP_RECV_TABLE;
	u8* data  = table + CPP_RECV_PACKETS * 8;
	int dataSize = CPP_RECV_SIZE - CPP_RECV_PACKETS * 8;
	u32 count = *(volatile u32*)(cpp_mem + 0x18);
	u32 offset, size, i, pos;
	u8 crc = 0, hdr[4];
	int len, beg;

	if (!count) return 0;
	offset = *(volatile u32*)(table + cpp_packetId * 8 + 0);
	size   = *(volatile u32*)(table + cpp_packetId * 8 + 4);

	// the last byte is a CRC8 of everything before it, so the CRC of the whole packet is 0
	for (i = 0; i < size; i++) crc = crc8_table[crc ^ data[(offset + i) % dataSize]];
	if (crc || size < 4) { CPP_NextPacket(); return 0; }

	for (i = 0; i < 4; i++) hdr[i] = data[(offset + i) % dataSize];
	if (hdr[2] & 0x40) {
		len = ((hdr[2] & 0x3F) << 8) | hdr[3]; beg = 4;
	} else {
		len = hdr[2] & 0x3F; beg = 3;
	}
	if (len > maxLen) len = maxLen;

	for (i = 0; i < (u32)len; i++) 
	{
		pos    = offset + beg + i;
		out[i] = data[pos % dataSize];
	}
	// release it only after copying, so the buffer never fills up
	CPP_NextPacket();
	return len;
}

// Counters shown in the diagnostics line, to see on the console how far the Circle Pad Pro got
static volatile int cpp_dbgTimeouts, cpp_dbgBad, cpp_dbgOk, cpp_dbgHead = -1, cpp_dbgLen, cpp_dbgCal;

// Sends a request and waits for the reply that starts with 'head' and has at least 'minLen' bytes.
//  Returns the reply length (> 0), 0 = exit, -1 = no answer
static int CPP_Exchange(const void* req, int reqLen, u8* resp, int respLen, int minLen, Handle recvEvent, u8 head) {
	Handle events[2] = { recvEvent, cpp_exitEvent };
	s32 which;
	Result res;
	int i, n, tries;

	for (tries = 0; tries < 4; tries++)
	{
		CPP_Send(req, reqLen);
		res = svcWaitSynchronizationN(&which, events, 2, false, 20 * CPP_MS);
		if (R_DESCRIPTION(res) == RD_TIMEOUT) { cpp_dbgTimeouts++; continue; }
		if (which == 1) return 0;

		// several packets may be waiting (e.g. late answers): use the one we asked for
		for (i = 0; i < 8; i++) 
		{
			n = CPP_ReadPacket(resp, respLen);
			if (n > 0) { cpp_dbgHead = resp[0]; cpp_dbgLen = n; }
			if (n >= minLen && resp[0] == head) { cpp_dbgOk++; return n; }
			if (n > 0) cpp_dbgBad++;
			if (!n && !*(volatile u32*)(cpp_mem + 0x18)) break;
		}
	}
	return -1;
}

// Calibration candidates are 16 bytes: pad, 12 bit x offset, 12 bit y offset, float x scale,
//  float y scale, 3 pad bytes, CRC8 of the first 15 bytes
static cc_bool CPP_TryCalibration(const u8* c) {
	u8 crc = 0;
	int i;
	for (i = 0; i < 15; i++) crc = crc8_table[crc ^ c[i]];
	if (crc != c[15]) return false;

	cpp_xOffset = c[1] | ((c[2] & 0x0F) << 8);
	cpp_yOffset = (c[2] >> 4) | (c[3] << 4);
	Mem_Copy((void*)&cpp_xScale, c + 4, 4);
	Mem_Copy((void*)&cpp_yScale, c + 8, 4);
	// guard against odd values, the stick must keep working
	if (!(cpp_xScale > 0.2f && cpp_xScale < 5.0f)) cpp_xScale = 1.0f;
	if (!(cpp_yScale > 0.2f && cpp_yScale < 5.0f)) cpp_yScale = 1.0f;
	return true;
}

static void CPP_ResetState(void) {
	cpp_connected = false;
	cpp_buttons   = 0;
	cpp_dx = 0; cpp_dy = 0;
}

static void CPP_ThreadFunc(void* arg) {
	Handle connEvent = 0, recvEvent = 0, events[2];
	s32 which;
	Result res;
	int i, j, ret;

	for (;;)
	{
		// ---- 1) look for the accessory ----
		cpp_stage = CPP_STAGE_SEARCHING;
		for (;;)
		{
			cpp_packetId = 0;
			CPP_InitShared();
			CPP_GetEvent(0x0C, &connEvent);
			CPP_GetEvent(0x0A, &recvEvent);
			events[0] = connEvent; events[1] = cpp_exitEvent;

			// a few quick tries, then wait a second
			for (i = 0; i < 4; i++)
			{
				CPP_RequireConnection();
				res = svcWaitSynchronizationN(&which, events, 2, false, 14 * CPP_MS);
				if (R_DESCRIPTION(res) != RD_TIMEOUT) {
					if (which == 0) goto connected;
					goto exit;
				}
				CPP_Disconnect();
			}
			svcCloseHandle(connEvent); svcCloseHandle(recvEvent);
			connEvent = recvEvent = 0;
			CPP_Disconnect(); CPP_ClearRecv(); CPP_ClearSend(); CPP_Finalize();

			if (svcWaitSynchronization(cpp_exitEvent, 1000 * CPP_MS) == 0) goto exit;
		}

	connected:
		cpp_stage = CPP_STAGE_FOUND;
		{
			// ---- 2) calibration ----
			u8 calReq[6] = { 0x02, 100, 0x00, 0x00, 0x40, 0x00 }; // head, response time, offset (LE), size (LE)
			u8 calResp[5 + 4 * 16];
			cc_bool calibrated = false;

			ret = CPP_Exchange(calReq, 6, calResp, sizeof(calResp), 5 + 16, recvEvent, 0x11);
			if (ret == 0) goto exit;
			if (ret > 0) {
				for (i = 0; i < 4 && 5 + i * 16 + 16 <= ret && !calibrated; i++) 
					calibrated = CPP_TryCalibration(calResp + 5 + i * 16);
			}
			// like Red Viper: the calibration may also be stored at 0x400 + i
			for (j = 0; ret > 0 && !calibrated && j < 0x40; j++)
			{
				calReq[2] = (u8)(0x400 + j); calReq[3] = (u8)((0x400 + j) >> 8);
				ret = CPP_Exchange(calReq, 6, calResp, sizeof(calResp), 5 + 16, recvEvent, 0x11);
				if (ret == 0) goto exit;
				for (i = 0; i < 4 && 5 + i * 16 + 16 <= ret && !calibrated; i++) 
					calibrated = CPP_TryCalibration(calResp + 5 + i * 16);
			}
			cpp_dbgCal = calibrated ? 1 : (ret > 0 ? 2 : 3);
			// without calibration the stick still works, using the resting position as the centre
			if (!calibrated) { cpp_xOffset = -1; cpp_xScale = cpp_yScale = 1.0f; }
		}

		// ---- 3) read the inputs ----
		{
			u8 inReq[3] = { 0x01, 0x20, 0x87 }; // head, response time, unknown (same as official games)
			u8 in[6];
			int x, y, fails = 0;

			while (*(volatile u8*)(cpp_mem + 0x08) == 2) // connection status: 2 = connected
			{
				ret = CPP_Exchange(inReq, 3, in, 6, 5, recvEvent, 0x10);
				if (ret == 0) goto exit;
				if (ret < 0) { if (++fails >= 3) break; continue; }
				fails = 0;

				x = in[1] | ((in[2] & 0x0F) << 8);
				y = (in[2] >> 4) | (in[3] << 4);
				if (cpp_xOffset < 0) { cpp_xOffset = x; cpp_yOffset = y; } // first reading = centre

				{
					int dx = (int)((x - cpp_xOffset) * cpp_xScale) / 8;
					int dy = (int)((y - cpp_yOffset) * cpp_yScale) / 8;
					u32 keys = 0;
					if (!(in[4] & 0x20)) keys |= KEY_ZL; // bits are 1 when NOT pressed
					if (!(in[4] & 0x40)) keys |= KEY_ZR;
					if (!(in[4] & 0x80)) keys |= KEY_R;
					cpp_dx = (s16)dx; cpp_dy = (s16)dy;
					cpp_buttons   = keys;
					cpp_connected = true;
					cpp_stage     = CPP_STAGE_CONNECTED;
				}
			}
		}

		// lost it: tidy up and start searching again
		CPP_ResetState();
		cpp_stage = CPP_STAGE_LOST;
		svcCloseHandle(connEvent); svcCloseHandle(recvEvent);
		connEvent = recvEvent = 0;
		CPP_Disconnect(); CPP_ClearRecv(); CPP_ClearSend(); CPP_Finalize();
		if (svcWaitSynchronization(cpp_exitEvent, 500 * CPP_MS) == 0) goto exit;
	}

exit:
	CPP_ResetState();
	if (connEvent) svcCloseHandle(connEvent);
	if (recvEvent) svcCloseHandle(recvEvent);
	CPP_Disconnect(); CPP_ClearRecv(); CPP_ClearSend(); CPP_Finalize();
}

static void CPP_Init(void) {
	Result res;
	cpp_mem = (u8*)memalign(0x1000, CPP_SHAREDMEM_SIZE);
	if (!cpp_mem) { cpp_initStep = "memoria"; return; }
	Mem_Set(cpp_mem, 0, CPP_SHAREDMEM_SIZE);

	if ((res = srvGetServiceHandle(&cpp_handle, "ir:USER"))) {
		cpp_initError = res; cpp_initStep = "servico"; return;
	}
	if ((res = svcCreateMemoryBlock(&cpp_memHandle, (u32)cpp_mem, CPP_SHAREDMEM_SIZE,
									MEMPERM_READ, MEMPERM_READWRITE))) {
		cpp_initError = res; cpp_initStep = "bloco"; return;
	}
	if ((res = svcCreateEvent(&cpp_exitEvent, RESET_ONESHOT))) {
		cpp_initError = res; cpp_initStep = "evento"; return;
	}
	// higher priority than the game (0x30) so the stick is read on time; it sleeps while waiting
	cpp_thread = threadCreate(CPP_ThreadFunc, NULL, 0x2000, 0x28, -2, false);
	if (!cpp_thread) { cpp_initStep = "thread"; return; }
}

static void CPP_Free(void) {
	if (!cpp_thread) return;
	svcSignalEvent(cpp_exitEvent);
	threadJoin(cpp_thread, 200 * CPP_MS);
	threadFree(cpp_thread);
	cpp_thread = NULL;
	svcCloseHandle(cpp_exitEvent);
	svcCloseHandle(cpp_memHandle);
	svcCloseHandle(cpp_handle);
}

static void CPP_Announce(const char* msg) {
	cc_string str = String_FromReadonly(msg);
	Chat_AddOf(&str, MSG_TYPE_SMALLANNOUNCEMENT);
}

// Shows in chat how far the connection got (called from the game thread)
static void CPP_Update(void) {
	int stage = cpp_stage;
	if (!Gui.ScreensCount) return; // still in the launcher

	if (!cpp_toldInit) {
		cpp_toldInit = true;
		if (cpp_initStep) {
			cc_string msg; char buf[96];
			String_InitArray(msg, buf);
			String_Format2(&msg, "&cCircle Pad Pro: erro no infravermelho (%c %h)", cpp_initStep, &cpp_initError);
			Chat_AddOf(&msg, MSG_TYPE_SMALLANNOUNCEMENT);
		}
	}
	if (stage == cpp_toldStage) return;
	cpp_toldStage = stage;

	if (stage == CPP_STAGE_FOUND) {
		CPP_Announce("&eCircle Pad Pro encontrado, calibrando...");
	} else if (stage == CPP_STAGE_CONNECTED) {
		CPP_Announce("&aCircle Pad Pro conectado: analogico direito gira a camera, ZL coloca, ZR quebra, L/R trocam o item");
		cpp_announced = true;
	} else if (stage == CPP_STAGE_LOST && cpp_announced) {
		CPP_Announce("&eCircle Pad Pro desconectado, procurando de novo...");
	}
}

// Fills in the Circle Pad Pro stick, in the same units as the normal Circle Pad (about -156 to 156)
static void CPP_GetStick(circlePosition* pos) {
	int dx = cpp_dx, dy = cpp_dy;
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

// Diagnostics shown on the top line of the HUD (next to the FPS), so any photo of
//  the console shows how the terrain atlases were loaded and how far the Circle Pad Pro got
char Gfx_3DS_TexKind(GfxResourceID texId);
cc_uint32 Gfx_3DS_TexPhysKB(GfxResourceID texId);
cc_uint32 Gfx_3DS_ReadTexel(GfxResourceID texId, int x, int y);

void Window_3DS_DiagText(cc_string* str) {
	int i, count = min(Atlas1D.Count, 4);
	int linFree  = (int)(linearSpaceFree() / 1024), vramFree = (int)(vramSpaceFree() / 1024);
	int size     = Atlas2D.TileSize, per = Atlas1D.TilesPerAtlas;
	int stage = cpp_stage, cal = cpp_dbgCal, ok = cpp_dbgOk, to = cpp_dbgTimeouts;
	int bad = cpp_dbgBad, head = cpp_dbgHead, len = cpp_dbgLen;

	String_Format2(str, "%ix%i", &Atlas1D.Count, &per);
	for (i = 0; i < count; i++)
	{
		GfxResourceID id = Atlas1D.TexIds[i];
		int row  = per * 3 / 4; /* e.g. tile 176 (tall grass) in atlas 2 */
		int tile = i * per + row;
		int mb   = (int)(Gfx_3DS_TexPhysKB(id) >> 10);
		cc_uint32 want = 0, got;

		if (Atlas2D.Bmp.scan0 && Atlas2D_TileY(tile) < Atlas2D.RowsCount) {
			want = Bitmap_GetPixel(&Atlas2D.Bmp, Atlas2D_TileX(tile) * size + size / 2,
									Atlas2D_TileY(tile) * size + size / 2);
		}
		got = Gfx_3DS_ReadTexel(id, size / 2, row * size + size / 2);
		String_Append(str, i ? ',' : ' ');
		String_Append(str, Gfx_3DS_TexKind(id));
		String_Format1(str, "%i", &mb);
		String_Append(str, got == want ? '+' : 'x');
	}
	String_Format2(str, " L%iK V%iK", &linFree, &vramFree);
	String_Format4(str, " P%i c%i o%i t%i", &stage, &cal, &ok, &to);
	String_Format3(str, " b%i h%i/%i", &bad, &head, &len);
	if (cpp_mem) { int conn = cpp_mem[0x08]; String_Format1(str, " s%i", &conn); }
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
		CPP_Update();
		if (cpp_connected) {
			mods |= cpp_buttons;
			CPP_GetStick(&stk_pos);
		}
		rightStickSeen = cpp_connected;
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

