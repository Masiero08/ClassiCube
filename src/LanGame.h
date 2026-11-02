#ifndef CC_LANGAME_H
#define CC_LANGAME_H
#include "Core.h"
CC_BEGIN_HEADER

/* "Jogar juntos": multijogador local pelo Wi-Fi (mesma rede/roteador).
   Um console abre o proprio mundo (anfitriao) e funciona como um pequeno servidor
   do protocolo classico. Os outros entram como numa conexao direta normal. */
struct IGameComponent;
extern struct IGameComponent LanGame_Component;

#define LAN_PORT       25565
#define LAN_MAX_GUESTS 7
/* Opcao que o menu "Jogar juntos" liga para o proximo mundo abrir para amigos */
#define LAN_OPT_HOST   "lan-hospedar"
#define LAN_OPT_NAME   "lan-nome"
#define LAN_OPT_IP     "lan-ultimo-ip"

/* Se este jogo esta hospedando um mundo para amigos */
cc_bool LanGame_IsHosting(void);
/* Se este jogo entrou no mundo de um amigo (anfitriao Minicreft) */
cc_bool LanGame_IsGuest(void);
/* Se os blocos dos mods podem ser definidos localmente (um jogador ou mundo de amigo) */
cc_bool LanGame_LocalBlocks(void);

cc_bool LanGame_StartHosting(void);
void    LanGame_StopHosting(void);
/* Chamado pela conexao de um jogador a cada tick de rede */
void LanGame_Tick(void);
/* Chamado sempre que um bloco do mundo muda */
void LanGame_OnBlockChanged(int x, int y, int z, BlockID block);
/* Mensagem de chat digitada pelo anfitriao */
void LanGame_HostChat(const cc_string* text);

/* IP deste aparelho na rede local (ex: 192.168.0.12). false se nao estiver conectado */
cc_bool LanGame_GetLocalIP(cc_string* dst);

/* Procura mundos abertos na rede local (usado pela tela do menu) */
struct LanFoundHost { char ip[20]; char name[20]; int port; double lastSeen; };
void LanDiscovery_Start(void);
void LanDiscovery_Stop(void);
/* Retorna quantos mundos foram encontrados (no maximo 4) */
int  LanDiscovery_Poll(struct LanFoundHost** hosts);

CC_END_HEADER
#endif
