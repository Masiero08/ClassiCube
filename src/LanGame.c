#include "LanGame.h"
#include "Game.h"
#include "Server.h"
#include "String_.h"
#include "Chat.h"
#include "Commands.h"
#include "Entity.h"
#include "World.h"
#include "Block.h"
#include "BlockPhysics.h"
#include "Options.h"
#include "Platform.h"
#include "Funcs.h"
#include "ExtMath.h"
#include "Deflate.h"
#include "Stream.h"
#include "Event.h"
#include "Errors.h"
#include "Protocol.h"
#include "Utils.h"
#include "PackedCol.h"

/* Multijogador local ("Jogar juntos")
   O anfitriao continua no modo um jogador (com todos os mods), mas abre uma porta TCP
   e fala o protocolo classico do Minecraft (+ extensao CustomBlocks) com quem entrar:
     - envia o mundo compactado (gzip), igual a um servidor normal
     - repassa posicoes dos jogadores, blocos colocados/quebrados e o chat
     - quando o anfitriao muda de mundo (Nether, Fim...), todos vao junto
   Os convidados usam a conexao de rede normal do ClassiCube, entao ate um ClassiCube
   de PC consegue entrar. Para achar o mundo sem digitar IP, o anfitriao anuncia o mundo
   por UDP (broadcast) e responde a quem perguntar. */

static cc_bool lan_hosting;

cc_bool LanGame_IsHosting(void) { return lan_hosting; }

cc_bool LanGame_IsGuest(void) {
	static const cc_string prefix = String_FromConst("Minicreft LAN");
	return !Server.IsSinglePlayer && String_CaselessStarts(&Server.Name, &prefix);
}

cc_bool LanGame_LocalBlocks(void) {
	return Server.IsSinglePlayer || LanGame_IsGuest();
}

#if defined CC_BUILD_3DS || defined CC_BUILD_LINUX || defined CC_BUILD_MACOS || defined CC_BUILD_FREEBSD
#define LAN_SUPPORTED
#endif

#ifdef LAN_SUPPORTED
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef CC_BUILD_3DS
#include <3ds.h>
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define LAN_DISC_PORT  25566 /* convidados escutam anuncios aqui */
#define LAN_QUERY_PORT 25567 /* anfitriao escuta perguntas aqui */
#define LAN_ANNOUNCE   "MCLAN1"
#define LAN_QUERY      "MCLAN?"

#define LAN_HOST_ID 0
#define LAN_IN_SIZE 2048

struct LanClient {
	int sock;
	cc_bool playing, cpe;
	EntityID id;
	char name[17];
	cc_uint8 in[LAN_IN_SIZE];
	int inLen;
	cc_uint8* out;
	cc_uint32 outLen, outCap;
	cc_uint8 pos[8]; /* x,y,z (16 bits cada), yaw, pitch */
};

static struct LanClient lan_clients[LAN_MAX_GUESTS];
static int lan_listenSock = -1, lan_udpSock = -1;
static int lan_ticks;
static cc_uint8 lan_hostPos[8];
static int lan_resendWorld; /* ticks ate reenviar o mundo (0 = nada pendente) */
static void ResendWorld(void);

/* Ceu, neblina, nuvens, borda do mapa e chuva do anfitriao (dia/noite, Nether, Fim) */
#define LAN_ENV_COLS  5
#define LAN_ENV_PROPS 5
struct LanEnv { PackedCol cols[LAN_ENV_COLS]; int props[LAN_ENV_PROPS]; int weather; };
static struct LanEnv lan_env;


/*########################################################################################################################*
*---------------------------------------------------------Helpers---------------------------------------------------------*
*#########################################################################################################################*/
static void SetNonBlocking(int s) {
	int flags = fcntl(s, F_GETFL, 0);
	if (flags == -1) flags = 0;
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

static void CloseSock(int s) {
#ifdef CC_BUILD_3DS
	closesocket(s);
#else
	close(s);
#endif
}

static void IPToString(cc_uint32 ip_net, cc_string* dst) {
	cc_uint8* b = (cc_uint8*)&ip_net;
	String_Format4(dst, "%b.%b.%b.%b", &b[0], &b[1], &b[2], &b[3]);
}

static cc_uint32 GetLocalIPRaw(void) {
#ifdef CC_BUILD_3DS
	return (cc_uint32)gethostid();
#else
	/* Truque: "conectar" um socket UDP nao envia nada, mas escolhe a interface de saida */
	struct sockaddr_in addr = { 0 };
	socklen_t len = sizeof(addr);
	cc_uint32 ip = 0;
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1) return 0;

	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(53);
	addr.sin_addr.s_addr = inet_addr("192.168.0.1");
	if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0 &&
		getsockname(s, (struct sockaddr*)&addr, &len) == 0) {
		ip = addr.sin_addr.s_addr;
	}
	CloseSock(s);
	return ip;
#endif
}

cc_bool LanGame_GetLocalIP(cc_string* dst) {
	cc_uint32 ip = GetLocalIPRaw();
	if (!ip || ip == 0xFFFFFFFF) return false;
	IPToString(ip, dst);
	return true;
}

/* Strings do protocolo classico: 64 bytes, completados com espacos */
static void WriteStr(cc_uint8* dst, const cc_string* src) {
	int i;
	for (i = 0; i < 64; i++) {
		dst[i] = i < src->length ? (cc_uint8)src->buffer[i] : ' ';
	}
}

static void ReadStr(const cc_uint8* src, cc_string* dst) {
	int i, len = 64;
	while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\0')) len--;
	for (i = 0; i < len; i++) String_Append(dst, (char)src[i]);
}

static void SendUdp(int s, cc_uint32 ip_net, int port, const char* msg, int len) {
	struct sockaddr_in addr = { 0 };
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(port);
	addr.sin_addr.s_addr = ip_net;
	sendto(s, msg, len, 0, (struct sockaddr*)&addr, sizeof(addr));
}

/* Envia para 255.255.255.255 e para o broadcast da sub-rede (x.y.z.255) */
static void BroadcastUdp(int s, int port, const char* msg, int len) {
	cc_uint32 ip = GetLocalIPRaw();
	SendUdp(s, htonl(INADDR_BROADCAST), port, msg, len);
	if (ip) SendUdp(s, ip | htonl(0xFF), port, msg, len);
}

static int OpenUdp(int port) {
	struct sockaddr_in addr = { 0 };
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	int yes = 1;
	if (s == -1) return -1;

	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&yes, sizeof(yes));
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		CloseSock(s); return -1;
	}
	SetNonBlocking(s);
	return s;
}


/*########################################################################################################################*
*------------------------------------------------------Envio de pacotes---------------------------------------------------*
*#########################################################################################################################*/
static void Client_Queue(struct LanClient* c, const cc_uint8* data, cc_uint32 len) {
	if (c->sock == -1) return;

	if (c->outLen + len > c->outCap) {
		cc_uint32 cap = c->outCap ? c->outCap : 4096;
		while (c->outLen + len > cap) cap *= 2;
		c->out = (cc_uint8*)Mem_TryRealloc(c->out, cap, 1);
		if (!c->out) { c->outLen = 0; c->outCap = 0; CloseSock(c->sock); c->sock = -1; return; }
		c->outCap = cap;
	}
	Mem_Copy(c->out + c->outLen, data, len);
	c->outLen += len;
}

static void Client_Flush(struct LanClient* c) {
	int sent;
	cc_uint32 total = 0;

	while (c->sock != -1 && total < c->outLen) {
		sent = send(c->sock, c->out + total, c->outLen - total, MSG_NOSIGNAL);
		if (sent > 0) { total += sent; continue; }
		if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS)) break;
		/* conexao caiu */
		CloseSock(c->sock); c->sock = -1;
	}

	if (total && total < c->outLen) {
		Mem_Move(c->out, c->out + total, c->outLen - total);
	}
	c->outLen = total >= c->outLen ? 0 : c->outLen - total;
}

static void Broadcast(const cc_uint8* data, cc_uint32 len, struct LanClient* except) {
	int i;
	for (i = 0; i < LAN_MAX_GUESTS; i++) {
		struct LanClient* c = &lan_clients[i];
		if (c->sock == -1 || !c->playing || c == except) continue;
		Client_Queue(c, data, len);
	}
}

static void MakeMessage(cc_uint8* pkt, const cc_string* text) {
	pkt[0] = OPCODE_MESSAGE; pkt[1] = 0;
	WriteStr(pkt + 2, text);
}

/* Mostra no proprio anfitriao e envia para todos (quebrando em pedacos de 64) */
static void SayToAll(const cc_string* text, struct LanClient* except) {
	cc_uint8 pkt[66];
	cc_string part, left = *text;
	char col = 'f';

	Chat_Add(text);
	while (left.length) {
		char buf[64];
		cc_string tmp = String_FromArray(buf);
		/* linhas seguintes continuam com a mesma cor */
		if (left.length != text->length) { String_Append(&tmp, '&'); String_Append(&tmp, col); }

		part = String_UNSAFE_Substring(&left, 0, min(left.length, 64 - tmp.length));
		String_AppendString(&tmp, &part);
		left = String_UNSAFE_SubstringAt(&left, part.length);
		if (part.length >= 2 && part.buffer[part.length - 2] == '&') col = part.buffer[part.length - 1];

		MakeMessage(pkt, &tmp);
		Broadcast(pkt, sizeof(pkt), except);
	}
}

static void MakeAddEntity(cc_uint8* pkt, EntityID id, const cc_string* name, const cc_uint8* pos) {
	pkt[0] = OPCODE_ADD_ENTITY;
	pkt[1] = id;
	WriteStr(pkt + 2, name);
	Mem_Copy(pkt + 66, pos, 8);
}

static void MakeTeleport(cc_uint8* pkt, EntityID id, const cc_uint8* pos) {
	pkt[0] = OPCODE_ENTITY_TELEPORT;
	pkt[1] = id;
	Mem_Copy(pkt + 2, pos, 8);
}

static void PackPosition(cc_uint8* dst, Vec3 pos, float yaw, float pitch) {
	int x = (int)(pos.x * 32), y = (int)(pos.y * 32) + 51, z = (int)(pos.z * 32);
	Mem_WriteU16_BE(dst + 0, (cc_uint16)x);
	Mem_WriteU16_BE(dst + 2, (cc_uint16)y);
	Mem_WriteU16_BE(dst + 4, (cc_uint16)z);
	dst[6] = Math_Deg2Packed(yaw);
	dst[7] = Math_Deg2Packed(pitch);
}

static void HostPosition(cc_uint8* dst) {
	struct Entity* e = &Entities.CurPlayer->Base;
	PackPosition(dst, e->Position, e->Yaw, e->Pitch);
}

static cc_bool IsFree(int x, int y, int z) {
	return !World_Contains(x, y, z) || Blocks.Collide[World_GetBlock(x, y, z)] != COLLIDE_SOLID;
}
static cc_bool IsSafeFloor(int x, int y, int z) {
	BlockID b;
	if (!World_Contains(x, y, z)) return false;
	b = World_GetBlock(x, y, z);
	return Blocks.Collide[b] == COLLIDE_SOLID && b != BLOCK_LAVA && b != BLOCK_STILL_LAVA;
}

/* Lugar para o convidado nascer: do lado do anfitriao (nao dentro dele) */
static void GuestSpawn(cc_uint8* dst, int first) {
	static const float offsets[12][2] = {
		{ 1.5f, 0 }, { -1.5f, 0 }, { 0, 1.5f }, { 0, -1.5f },
		{ 1.5f, 1.5f }, { -1.5f, 1.5f }, { 1.5f, -1.5f }, { -1.5f, -1.5f },
		{ 3, 0 }, { -3, 0 }, { 0, 3 }, { 0, -3 }
	};
	struct Entity* e = &Entities.CurPlayer->Base;
	Vec3 pos;
	int j, i, x, y, z;

	/* cada amigo comeca tentando um lado diferente */
	for (j = 0; j < Array_Elems(offsets); j++) {
		i = j < 4 ? (j + first) % 4 : j;
		pos   = e->Position;
		pos.x += offsets[i][0]; pos.z += offsets[i][1];
		x = Math_Floor(pos.x); y = Math_Floor(pos.y + 0.01f); z = Math_Floor(pos.z);

		if (IsFree(x, y, z) && IsFree(x, y + 1, z) && IsSafeFloor(x, y - 1, z)) {
			/* olhando para o anfitriao */
			float yaw = Math_Atan2f(offsets[i][1], -offsets[i][0]) * MATH_RAD2DEG;
			PackPosition(dst, pos, yaw, 0); return;
		}
	}
	HostPosition(dst);
}


/*########################################################################################################################*
*-------------------------------------------------Jogadores no anfitriao--------------------------------------------------*
*#########################################################################################################################*/
static void ApplyLocation(EntityID id, const cc_uint8* pos, cc_uint8 flags) {
	struct LocationUpdate update;
	struct Entity* e = Entities.List[id];
	if (!e) return;

	update.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH | flags;
	update.pos.x = (cc_int16)Mem_ReadU16_BE(pos + 0) / 32.0f;
	update.pos.y = ((cc_int16)Mem_ReadU16_BE(pos + 2) - 51) / 32.0f;
	update.pos.z = (cc_int16)Mem_ReadU16_BE(pos + 4) / 32.0f;
	update.yaw   = Math_Packed2Deg(pos[6]);
	update.pitch = Math_Packed2Deg(pos[7]);
	e->VTABLE->SetLocation(e, &update);
}

static void AddLocalEntity(struct LanClient* c) {
	static const cc_string group = String_FromConst("Amigos");
	cc_string name = String_FromReadonly(c->name);
	struct Entity* e;

	Entities_Remove(c->id);
	e = &NetPlayers_List[c->id].Base;
	NetPlayer_Init((struct NetPlayer*)e);
	Entities.List[c->id] = e;
	Event_RaiseInt(&EntityEvents.Added, c->id);

	Entity_SetSkin(e, &name);
	Entity_SetName(e, &name);
	ApplyLocation(c->id, c->pos, LU_POS_ABSOLUTE_INSTANT);

	TabList_Set(c->id, &name, &name, &group, 0);
	TabList_EntityLinked_Set(c->id);
}

static void RemoveLocalEntity(struct LanClient* c) {
	Entities_Remove(c->id);
	TabList_Remove(c->id);
}


static void GetEnv(struct LanEnv* e) {
	e->cols[0]  = Env.SkyCol;    e->cols[1] = Env.CloudsCol; e->cols[2] = Env.FogCol;
	e->cols[3]  = Env.ShadowCol; e->cols[4] = Env.SunCol;
	e->props[0] = Env.SidesBlock;   e->props[1] = Env.EdgeBlock; e->props[2] = Env.EdgeHeight;
	e->props[3] = Env.CloudsHeight; e->props[4] = Env.SidesOffset;
	e->weather  = Env.Weather;
}

/* Envia so o que mudou em relacao a 'old' (ou tudo se old for NULL) */
static void SendEnv(struct LanClient* target, const struct LanEnv* now, const struct LanEnv* old) {
	static const cc_uint8 propIds[LAN_ENV_PROPS] = { 0, 1, 2, 3, 9 };
	cc_uint8 pkt[8];
	int i;

	for (i = 0; i < LAN_ENV_COLS; i++) {
		if (old && old->cols[i] == now->cols[i]) continue;
		pkt[0] = OPCODE_ENV_SET_COLOR; pkt[1] = i;
		pkt[2] = 0; pkt[3] = PackedCol_R(now->cols[i]);
		pkt[4] = 0; pkt[5] = PackedCol_G(now->cols[i]);
		pkt[6] = 0; pkt[7] = PackedCol_B(now->cols[i]);
		if (target) Client_Queue(target, pkt, 8); else Broadcast(pkt, 8, NULL);
	}
	for (i = 0; i < LAN_ENV_PROPS; i++) {
		if (old && old->props[i] == now->props[i]) continue;
		pkt[0] = OPCODE_ENV_SET_MAP_PROPERTY; pkt[1] = propIds[i];
		Mem_WriteU32_BE(pkt + 2, (cc_uint32)now->props[i]);
		if (target) Client_Queue(target, pkt, 6); else Broadcast(pkt, 6, NULL);
	}
	if (!old || old->weather != now->weather) {
		pkt[0] = OPCODE_ENV_SET_WEATHER; pkt[1] = (cc_uint8)now->weather;
		if (target) Client_Queue(target, pkt, 2); else Broadcast(pkt, 2, NULL);
	}
}

static void SyncEnv(void) {
	struct LanEnv now;
	GetEnv(&now);
	if (Mem_Equal(&now, &lan_env, sizeof(now))) return;
	SendEnv(NULL, &now, &lan_env);
	lan_env = now;
}


/*########################################################################################################################*
*--------------------------------------------------------Envio do mundo---------------------------------------------------*
*#########################################################################################################################*/
/* Stream que vai escrevendo os dados compactados direto em pacotes LevelDataChunk */
static struct LanClient* map_target;
static cc_uint8 map_chunk[1028];
static int map_chunkLen;

static void MapChunk_Emit(void) {
	if (!map_chunkLen) return;
	map_chunk[0] = OPCODE_LEVEL_DATA;
	Mem_WriteU16_BE(map_chunk + 1, (cc_uint16)map_chunkLen);
	Mem_Set(map_chunk + 3 + map_chunkLen, 0, 1024 - map_chunkLen);
	map_chunk[1027] = 0;
	Client_Queue(map_target, map_chunk, sizeof(map_chunk));
	map_chunkLen = 0;
}

static cc_result MapChunk_Write(struct Stream* s, const cc_uint8* data, cc_uint32 count, cc_uint32* modified) {
	cc_uint32 left = count;
	while (left) {
		cc_uint32 n = min(left, (cc_uint32)(1024 - map_chunkLen));
		Mem_Copy(map_chunk + 3 + map_chunkLen, data, n);
		map_chunkLen += n; data += n; left -= n;
		if (map_chunkLen == 1024) MapChunk_Emit();
	}
	*modified = count;
	return 0;
}

static cc_result SendMapData(struct LanClient* c) {
	struct Stream chunks, gz;
	struct GZipState* state;
	cc_uint8 vol[4];
	cc_result res;

	state = (struct GZipState*)Mem_TryAlloc(1, sizeof(struct GZipState));
	if (!state) return ERR_OUT_OF_MEMORY;

	Stream_Init(&chunks);
	chunks.Write = MapChunk_Write;
	map_target   = c;
	map_chunkLen = 0;

	GZip_MakeStream(&gz, state, &chunks);
	Mem_WriteU32_BE(vol, (cc_uint32)World.Volume);
	if (!(res = Stream_Write(&gz, vol, 4))) {
		if (!(res = Stream_Write(&gz, World.Blocks, World.Volume))) {
			res = gz.Close(&gz);
		}
	}
	MapChunk_Emit();
	Mem_Free(state);
	return res;
}

static void SendWorld(struct LanClient* c) {
	cc_uint8 pkt[131];
	cc_uint8 pos[8];
	cc_string name, hostName;
	int i;

	pkt[0] = OPCODE_LEVEL_BEGIN;
	Client_Queue(c, pkt, 1);
	if (SendMapData(c)) {
		static const cc_string reason = String_FromConst("O anfitriao nao conseguiu enviar o mundo");
		pkt[0] = OPCODE_KICK; WriteStr(pkt + 1, &reason);
		Client_Queue(c, pkt, 65);
		return;
	}

	pkt[0] = OPCODE_LEVEL_END;
	Mem_WriteU16_BE(pkt + 1, (cc_uint16)World.Width);
	Mem_WriteU16_BE(pkt + 3, (cc_uint16)World.Height);
	Mem_WriteU16_BE(pkt + 5, (cc_uint16)World.Length);
	Client_Queue(c, pkt, 7);

	if (c->cpe) {
		struct LanEnv now;
		GetEnv(&now);
		SendEnv(c, &now, NULL);
	}

	/* O convidado nasce do lado do anfitriao */
	GuestSpawn(pos, c->id - 1);
	name = String_FromReadonly(c->name);
	MakeAddEntity(pkt, ENTITIES_SELF_ID, &name, pos);
	Client_Queue(c, pkt, 74);
	Mem_Copy(c->pos, pos, 8);

	hostName = Game_Username;
	HostPosition(pos);
	MakeAddEntity(pkt, LAN_HOST_ID, &hostName, pos);
	Client_Queue(c, pkt, 74);

	for (i = 0; i < LAN_MAX_GUESTS; i++) {
		struct LanClient* o = &lan_clients[i];
		if (o == c || o->sock == -1 || !o->playing) continue;
		name = String_FromReadonly(o->name);
		MakeAddEntity(pkt, o->id, &name, o->pos);
		Client_Queue(c, pkt, 74);
	}
}


/*########################################################################################################################*
*---------------------------------------------------------Conexoes--------------------------------------------------------*
*#########################################################################################################################*/
static void UpdateStatus(void) {
	cc_string msg; char msgBuffer[STRING_SIZE];
	int i, count = 0;
	String_InitArray(msg, msgBuffer);
	if (!lan_hosting) { Chat_AddOf(&String_Empty, MSG_TYPE_STATUS_3); return; }

	for (i = 0; i < LAN_MAX_GUESTS; i++) {
		if (lan_clients[i].sock != -1 && lan_clients[i].playing) count++;
	}
	String_Format1(&msg, "&aJogar juntos: &f%i amigo(s)  &7IP ", &count);
	if (!LanGame_GetLocalIP(&msg)) String_AppendConst(&msg, "?");
	Chat_AddOf(&msg, MSG_TYPE_STATUS_3);
}

static void Client_Drop(struct LanClient* c, const char* reason) {
	cc_string msg; char msgBuffer[STRING_SIZE];
	cc_uint8 pkt[65];
	cc_bool wasPlaying = c->playing;

	if (c->sock != -1) {
		if (reason) {
			cc_string r = String_FromReadonly(reason);
			pkt[0] = OPCODE_KICK; WriteStr(pkt + 1, &r);
			Client_Queue(c, pkt, 65);
			Client_Flush(c);
		}
		if (c->sock != -1) CloseSock(c->sock);
	}
	c->sock    = -1;
	c->playing = false;
	c->inLen   = 0;
	c->outLen  = 0;

	if (!wasPlaying) return;
	RemoveLocalEntity(c);
	pkt[0] = OPCODE_REMOVE_ENTITY; pkt[1] = c->id;
	Broadcast(pkt, 2, NULL);

	String_InitArray(msg, msgBuffer);
	String_Format1(&msg, "&e%c saiu do mundo", c->name);
	SayToAll(&msg, NULL);
	UpdateStatus();
}

static cc_bool NameTaken(const cc_string* name, struct LanClient* self) {
	int i;
	if (String_CaselessEquals(name, &Game_Username)) return true;
	for (i = 0; i < LAN_MAX_GUESTS; i++) {
		struct LanClient* o = &lan_clients[i];
		if (o == self || o->sock == -1 || !o->playing) continue;
		{
			cc_string other = String_FromReadonly(o->name);
			if (String_CaselessEquals(name, &other)) return true;
		}
	}
	return false;
}

static void HandleHandshake(struct LanClient* c, const cc_uint8* data) {
	static const cc_string appName = String_FromConst("Minicreft LAN");
	static const char* const exts[4] = { "CustomBlocks", "EnvColors", "EnvMapAspect", "EnvWeatherType" };
	cc_string raw; char rawBuffer[STRING_SIZE];
	cc_string name, msg; char msgBuffer[STRING_SIZE];
	cc_uint8 pkt[131];
	int i;

	if (c->playing) return;
	String_InitArray(raw, rawBuffer);
	ReadStr(data + 2, &raw);

	/* So letras, numeros e _ (no maximo 12, sobra espaco para numero repetido) */
	name = String_Init(c->name, 0, sizeof(c->name) - 1);
	for (i = 0; i < raw.length && name.length < 12; i++) {
		char ch = raw.buffer[i];
		if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
			String_Append(&name, ch);
	}
	if (!name.length) String_AppendConst(&name, "Amigo");
	for (i = 2; NameTaken(&name, c) && i < 10; i++) {
		if (i > 2) name.length--;
		String_Append(&name, (char)('0' + i));
	}
	c->name[name.length] = '\0';
	c->cpe = data[130] == 0x42;

	if (c->cpe) {
		pkt[0] = OPCODE_EXT_INFO;
		WriteStr(pkt + 1, &appName);
		Mem_WriteU16_BE(pkt + 65, Array_Elems(exts));
		Client_Queue(c, pkt, 67);

		for (i = 0; i < Array_Elems(exts); i++) {
			cc_string ext = String_FromReadonly(exts[i]);
			pkt[0] = OPCODE_EXT_ENTRY;
			WriteStr(pkt + 1, &ext);
			Mem_WriteU32_BE(pkt + 65, 1);
			Client_Queue(c, pkt, 69);
		}

		pkt[0] = OPCODE_CUSTOM_BLOCK_LEVEL; pkt[1] = 1;
		Client_Queue(c, pkt, 2);
	}

	/* Handshake do servidor: o nome comeca com "Minicreft LAN" para o convidado saber */
	String_InitArray(msg, msgBuffer);
	String_Format1(&msg, "Minicreft LAN - mundo de %s", &Game_Username);
	pkt[0] = OPCODE_HANDSHAKE; pkt[1] = 7;
	WriteStr(pkt + 2, &msg);
	msg.length = 0;
	String_AppendConst(&msg, "Jogar juntos pelo Wi-Fi local");
	WriteStr(pkt + 66, &msg);
	pkt[130] = 0x64; /* operador: pode usar agua, lava e rocha-mae */
	Client_Queue(c, pkt, 131);

	SendWorld(c);
	c->playing = true;

	/* Avisa os outros e cria o boneco no anfitriao */
	name = String_FromReadonly(c->name);
	MakeAddEntity(pkt, c->id, &name, c->pos);
	Broadcast(pkt, 74, c);
	AddLocalEntity(c);

	msg.length = 0;
	String_Format1(&msg, "&e%s entrou no mundo!", &name);
	SayToAll(&msg, NULL);
	UpdateStatus();
}

static void HandleSetBlock(struct LanClient* c, const cc_uint8* data) {
	int x = Mem_ReadU16_BE(data + 1);
	int y = Mem_ReadU16_BE(data + 3);
	int z = Mem_ReadU16_BE(data + 5);
	BlockID block = data[7] ? data[8] : BLOCK_AIR;
	BlockID old;
	cc_uint8 pkt[8];

	if (!c->playing || !World.Loaded || !World_Contains(x, y, z)) return;
	if (block > BLOCK_MAX_DEFINED) block = BLOCK_AIR;
	old = World_GetBlock(x, y, z);

	if (old != block) {
		Game_UpdateBlock(x, y, z, block); /* LanGame_OnBlockChanged repassa para todos */
		Physics_OnBlockChanged(x, y, z, old, block);
	} else {
		/* nada mudou: confirma o bloco certo para quem pediu */
		pkt[0] = OPCODE_SET_BLOCK;
		Mem_WriteU16_BE(pkt + 1, x); Mem_WriteU16_BE(pkt + 3, y); Mem_WriteU16_BE(pkt + 5, z);
		pkt[7] = (cc_uint8)old;
		Client_Queue(c, pkt, 8);
	}
}

static void HandlePosition(struct LanClient* c, const cc_uint8* data) {
	cc_uint8 pkt[10];
	if (!c->playing) return;
	if (Mem_Equal(c->pos, data + 2, 8)) return;

	Mem_Copy(c->pos, data + 2, 8);
	ApplyLocation(c->id, c->pos, LU_POS_ABSOLUTE_SMOOTH | LU_ORI_INTERPOLATE);
	MakeTeleport(pkt, c->id, c->pos);
	Broadcast(pkt, 10, c);
}

static void HandleMessage(struct LanClient* c, const cc_uint8* data) {
	cc_string text; char textBuffer[STRING_SIZE];
	cc_string msg;  char msgBuffer[STRING_SIZE * 2];
	if (!c->playing) return;

	String_InitArray(text, textBuffer);
	ReadStr(data + 2, &text);
	if (!text.length) return;

	String_InitArray(msg, msgBuffer);
	String_Format2(&msg, "&b%c:&f %s", c->name, &text);
	SayToAll(&msg, NULL);
}

/* Tamanho dos pacotes que um cliente pode mandar */
static int PacketSize(cc_uint8 opcode) {
	switch (opcode) {
	case OPCODE_HANDSHAKE:          return 131;
	case OPCODE_PING:               return 1;
	case OPCODE_SET_BLOCK_CLIENT:   return 9;
	case OPCODE_ENTITY_TELEPORT:    return 10;
	case OPCODE_MESSAGE:            return 66;
	case OPCODE_EXT_INFO:           return 67;
	case OPCODE_EXT_ENTRY:          return 69;
	case OPCODE_CUSTOM_BLOCK_LEVEL: return 2;
	}
	return 0;
}

static void Client_Read(struct LanClient* c) {
	int got, i, size;

	for (;;) {
		if (c->sock == -1) return;
		got = recv(c->sock, c->in + c->inLen, LAN_IN_SIZE - c->inLen, 0);
		if (got == 0) { Client_Drop(c, NULL); return; }
		if (got < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) break;
			Client_Drop(c, NULL); return;
		}
		c->inLen += got;

		for (i = 0; i < c->inLen; i += size) {
			size = PacketSize(c->in[i]);
			if (!size) { Client_Drop(c, "Pacote desconhecido"); return; }
			if (i + size > c->inLen) break;

			switch (c->in[i]) {
			case OPCODE_HANDSHAKE:        HandleHandshake(c, c->in + i); break;
			case OPCODE_SET_BLOCK_CLIENT: HandleSetBlock(c,  c->in + i); break;
			case OPCODE_ENTITY_TELEPORT:  HandlePosition(c,  c->in + i); break;
			case OPCODE_MESSAGE:          HandleMessage(c,   c->in + i); break;
			}
			if (c->sock == -1) return;
		}
		Mem_Move(c->in, c->in + i, c->inLen - i);
		c->inLen -= i;
	}
}

static void AcceptClients(void) {
	struct sockaddr_in addr;
	socklen_t len;
	int s, i;

	for (;;) {
		len = sizeof(addr);
		s   = accept(lan_listenSock, (struct sockaddr*)&addr, &len);
		if (s == -1) return;
		SetNonBlocking(s);

		for (i = 0; i < LAN_MAX_GUESTS; i++) {
			if (lan_clients[i].sock == -1) break;
		}
		if (i == LAN_MAX_GUESTS) {
			static const cc_string full = String_FromConst("O mundo esta cheio");
			cc_uint8 pkt[65];
			pkt[0] = OPCODE_KICK; WriteStr(pkt + 1, &full);
			send(s, pkt, 65, MSG_NOSIGNAL);
			CloseSock(s);
			continue;
		}

		lan_clients[i].sock    = s;
		lan_clients[i].playing = false;
		lan_clients[i].inLen   = 0;
		lan_clients[i].outLen  = 0;
		lan_clients[i].id      = (EntityID)(1 + i);
	}
}

static void HandleQueries(void) {
	struct sockaddr_in addr;
	socklen_t len;
	char buf[64];
	char reply[64];
	cc_string msg = String_FromArray(reply);
	int got;

	static const int port = LAN_PORT;
	String_Format2(&msg, "%c %i ", LAN_ANNOUNCE, &port);
	String_AppendString(&msg, &Game_Username);

	for (;;) {
		len = sizeof(addr);
		got = recvfrom(lan_udpSock, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &len);
		if (got <= 0) break;
		if (got < 6 || Mem_Equal(buf, LAN_QUERY, 6) == false) continue;
		SendUdp(lan_udpSock, addr.sin_addr.s_addr, LAN_DISC_PORT, reply, msg.length);
	}

	/* Anuncia o mundo a cada 2 segundos */
	if ((lan_ticks % 120) == 0) BroadcastUdp(lan_udpSock, LAN_DISC_PORT, reply, msg.length);
}


/*########################################################################################################################*
*-------------------------------------------------------Ciclo principal---------------------------------------------------*
*#########################################################################################################################*/
cc_bool LanGame_StartHosting(void) {
	struct sockaddr_in addr = { 0 };
	cc_string msg; char msgBuffer[STRING_SIZE];
	int yes = 1, i;
	if (lan_hosting) return true;

	lan_listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (lan_listenSock == -1) {
		Chat_AddRaw("&cNao deu para abrir o mundo: rede indisponivel");
		return false;
	}
	setsockopt(lan_listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(LAN_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(lan_listenSock, (struct sockaddr*)&addr, sizeof(addr)) == -1 || listen(lan_listenSock, 4) == -1) {
		Chat_AddRaw("&cNao deu para abrir o mundo (porta 25565 ocupada?)");
		CloseSock(lan_listenSock); lan_listenSock = -1;
		return false;
	}
	SetNonBlocking(lan_listenSock);
	lan_udpSock = OpenUdp(LAN_QUERY_PORT);

	for (i = 0; i < LAN_MAX_GUESTS; i++) lan_clients[i].sock = -1;
	lan_hosting = true;
	lan_ticks   = 0;
	lan_resendWorld = 0;
	HostPosition(lan_hostPos);
	GetEnv(&lan_env);

	String_InitArray(msg, msgBuffer);
	Chat_AddRaw("&aMundo aberto para amigos no Wi-Fi!");
	if (LanGame_GetLocalIP(&msg)) {
		cc_string tmp; char tmpBuffer[STRING_SIZE];
		String_InitArray(tmp, tmpBuffer);
		String_Format1(&tmp, "&eNo outro 3DS: Jogar juntos > Procurar ou IP &f%s", &msg);
		Chat_Add(&tmp);
	} else {
		Chat_AddRaw("&cEste 3DS nao parece estar conectado no Wi-Fi");
	}
	UpdateStatus();
	return true;
}

static void StopHosting(cc_bool quiet) {
	int i;
	if (!lan_hosting) return;

	for (i = 0; i < LAN_MAX_GUESTS; i++) {
		struct LanClient* c = &lan_clients[i];
		if (c->sock != -1) {
			if (quiet) c->playing = false; /* o jogo esta fechando: so desconecta */
			Client_Drop(c, "O anfitriao fechou o mundo");
		}
		Mem_Free(c->out);
		c->out = NULL; c->outCap = 0; c->playing = false;
	}
	if (lan_listenSock != -1) CloseSock(lan_listenSock);
	if (lan_udpSock    != -1) CloseSock(lan_udpSock);
	lan_listenSock = -1; lan_udpSock = -1;
	lan_hosting = false;
	if (quiet) return;

	UpdateStatus();
	Chat_AddRaw("&eMundo fechado para amigos");
}
void LanGame_StopHosting(void) { StopHosting(false); }

void LanGame_Tick(void) {
	cc_uint8 pkt[10];
	int i;
	if (!lan_hosting) return;
	lan_ticks++;

	AcceptClients();
	if (lan_udpSock != -1) HandleQueries();

	for (i = 0; i < LAN_MAX_GUESTS; i++) {
		struct LanClient* c = &lan_clients[i];
		if (c->sock == -1) continue;
		Client_Read(c);
	}

	/* Posicao do anfitriao 20 vezes por segundo */
	if ((lan_ticks % 3) == 0) {
		HostPosition(pkt + 2);
		if (!Mem_Equal(pkt + 2, lan_hostPos, 8)) {
			Mem_Copy(lan_hostPos, pkt + 2, 8);
			pkt[0] = OPCODE_ENTITY_TELEPORT; pkt[1] = LAN_HOST_ID;
			Broadcast(pkt, 10, NULL);
		}
	}
	if (lan_resendWorld && --lan_resendWorld == 0) ResendWorld();
	if ((lan_ticks % 15) == 0 && !lan_resendWorld) SyncEnv();

	/* Ping a cada segundo, para a conexao nao parecer parada */
	if ((lan_ticks % 60) == 0) {
		pkt[0] = OPCODE_PING;
		Broadcast(pkt, 1, NULL);
	}

	for (i = 0; i < LAN_MAX_GUESTS; i++) {
		struct LanClient* c = &lan_clients[i];
		if (c->sock != -1) Client_Flush(c);
		/* conexao caiu enquanto enviava */
		if (c->sock == -1 && c->playing) Client_Drop(c, NULL);
	}
}

void LanGame_OnBlockChanged(int x, int y, int z, BlockID block) {
	cc_uint8 pkt[8];
	/* durante a troca de mundo o mapa inteiro vai ser reenviado */
	if (!lan_hosting || lan_resendWorld) return;

	pkt[0] = OPCODE_SET_BLOCK;
	Mem_WriteU16_BE(pkt + 1, x); Mem_WriteU16_BE(pkt + 3, y); Mem_WriteU16_BE(pkt + 5, z);
	pkt[7] = (cc_uint8)block;
	Broadcast(pkt, 8, NULL);
}

void LanGame_HostChat(const cc_string* text) {
	cc_string msg; char msgBuffer[STRING_SIZE * 2];
	String_InitArray(msg, msgBuffer);
	String_Format2(&msg, "&a%s:&f %s", &Game_Username, text);
	SayToAll(&msg, NULL);
}

/* O anfitriao mudou de mundo (carregou outro, Nether, Fim...): leva todo mundo junto. */
/* Espera alguns ticks, porque os mods ainda mexem no mundo e na posicao do jogador logo depois. */
static void OnNewMapLoaded(void) {
	if (lan_hosting) lan_resendWorld = 20;
}

static void ResendWorld(void) {
	int i;

	for (i = 0; i < LAN_MAX_GUESTS; i++) {
		struct LanClient* c = &lan_clients[i];
		if (c->sock == -1 || !c->playing) continue;
		SendWorld(c);
		Client_Flush(c);
	}
	HostPosition(lan_hostPos);
	for (i = 0; i < LAN_MAX_GUESTS; i++) {
		struct LanClient* c = &lan_clients[i];
		if (c->sock == -1 || !c->playing) continue;
		Mem_Copy(c->pos, lan_hostPos, 8);
		ApplyLocation(c->id, c->pos, LU_POS_ABSOLUTE_INSTANT);
	}
	GetEnv(&lan_env);
}


/*########################################################################################################################*
*-----------------------------------------------------Procurar mundos-----------------------------------------------------*
*#########################################################################################################################*/
static int disc_sock = -1, disc_ticks;
static struct LanFoundHost disc_hosts[4];
static int disc_count;

void LanDiscovery_Start(void) {
	disc_count = 0;
	disc_ticks = 0;
	if (disc_sock == -1) disc_sock = OpenUdp(LAN_DISC_PORT);
}

void LanDiscovery_Stop(void) {
	if (disc_sock != -1) CloseSock(disc_sock);
	disc_sock = -1;
}

int LanDiscovery_Poll(struct LanFoundHost** hosts) {
	struct sockaddr_in addr;
	socklen_t len;
	char buf[80];
	int got, i, port;
	*hosts = disc_hosts;
	if (disc_sock == -1) return disc_count;

	/* Pergunta "tem alguem ai?" a cada ~1 segundo */
	if ((disc_ticks++ % 30) == 0) BroadcastUdp(disc_sock, LAN_QUERY_PORT, LAN_QUERY, 6);

	for (;;) {
		cc_string ip, name;
		char ipBuf[20];
		cc_string parts[3];
		cc_string raw;

		len = sizeof(addr);
		got = recvfrom(disc_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&addr, &len);
		if (got <= 0) break;
		if (got < 8 || !Mem_Equal(buf, LAN_ANNOUNCE, 6)) continue;

		raw = String_Init(buf, got, got);
		if (String_UNSAFE_Split(&raw, ' ', parts, 3) < 3) continue;
		if (!Convert_ParseInt(&parts[1], &port)) continue;

		ip = String_Init(ipBuf, 0, sizeof(ipBuf) - 1);
		IPToString(addr.sin_addr.s_addr, &ip);
		for (i = 0; i < disc_count; i++) {
			cc_string known = String_FromReadonly(disc_hosts[i].ip);
			if (String_Equals(&known, &ip)) break;
		}
		if (i == disc_count) {
			if (disc_count == Array_Elems(disc_hosts)) continue;
			disc_count++;
		}

		Mem_Copy(disc_hosts[i].ip, ip.buffer, ip.length);
		disc_hosts[i].ip[ip.length] = '\0';

		name = String_Init(disc_hosts[i].name, 0, sizeof(disc_hosts[i].name) - 1);
		String_AppendString(&name, &parts[2]);
		String_UNSAFE_TrimEnd(&name);
		disc_hosts[i].name[name.length] = '\0';
		disc_hosts[i].port = port;
		disc_hosts[i].lastSeen = 0;
	}
	return disc_count;
}

#else
/* Plataformas sem suporte: tudo desligado */
cc_bool LanGame_StartHosting(void) {
	Chat_AddRaw("&cJogar juntos nao funciona nesta plataforma");
	return false;
}
void LanGame_StopHosting(void) { }
void LanGame_Tick(void) { }
void LanGame_OnBlockChanged(int x, int y, int z, BlockID block) { }
void LanGame_HostChat(const cc_string* text) { }
cc_bool LanGame_GetLocalIP(cc_string* dst) { return false; }
void LanDiscovery_Start(void) { }
void LanDiscovery_Stop(void) { }
int  LanDiscovery_Poll(struct LanFoundHost** hosts) { *hosts = NULL; return 0; }
static void OnNewMapLoaded(void) { }
#endif


/*########################################################################################################################*
*---------------------------------------------------------Comando---------------------------------------------------------*
*#########################################################################################################################*/
static void LanCommand_Execute(const cc_string* args, int argsCount) {
	cc_string ip; char ipBuffer[32];
	if (!Server.IsSinglePlayer) {
		Chat_AddRaw("&cSo o anfitriao pode abrir o mundo"); return;
	}

	if (lan_hosting) {
		LanGame_StopHosting();
		Options_SetBool(LAN_OPT_HOST, false);
	} else if (LanGame_StartHosting()) {
		Options_SetBool(LAN_OPT_HOST, true);
		String_InitArray(ip, ipBuffer);
		if (LanGame_GetLocalIP(&ip)) Chat_Add1("&eSeu IP: &f%s", &ip);
	}
}

static struct ChatCommand LanCommand = {
	"Lan", LanCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client lan",
		"&eAbre (ou fecha) este mundo para amigos no mesmo Wi-Fi.",
		"&eNo outro 3DS: Jogar juntos > Procurar mundos.",
	}
};

static void OnInit(void) {
	Commands_Register(&LanCommand);
}

static void OnMapLoadedMaybeHost(void) {
	/* O menu "Jogar juntos > Criar mundo" pede para abrir o mundo assim que ele carregar */
	if (Server.IsSinglePlayer && !lan_hosting && Options_GetBool(LAN_OPT_HOST, false)) {
		LanGame_StartHosting();
	}
	OnNewMapLoaded();
}

static void OnFree(void) {
#ifdef LAN_SUPPORTED
	StopHosting(true);
#endif
}

struct IGameComponent LanGame_Component = {
	OnInit,              /* Init  */
	OnFree,              /* Free  */
	NULL,                /* Reset */
	NULL,                /* OnNewMap */
	OnMapLoadedMaybeHost /* OnNewMapLoaded */
};
