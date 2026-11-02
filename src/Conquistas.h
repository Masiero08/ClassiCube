#ifndef CC_CONQUISTAS_H
#define CC_CONQUISTAS_H
#include "Core.h"
CC_BEGIN_HEADER

/* Conquistas (achievements) e o botao de salvar o mundo, para o modo um jogador.
   As conquistas valem para o jogador (nao para um mundo so) e ficam guardadas nas opcoes.
   Quando uma e ganha aparece um aviso no alto da tela de cima.
*/
struct IGameComponent;
extern struct IGameComponent Conquistas_Component;

/* Coisas que os outros mods avisam quando acontecem */
enum ConquistaEvento {
	CONQ_EV_MONSTRO,   /* derrotou um monstro */
	CONQ_EV_CHEFAO,    /* derrotou um chefao do Minerios & Monstros */
	CONQ_EV_ENDER,     /* derrotou o Ender Dragon */
	CONQ_EV_DOMAR,     /* domou um dragao */
	CONQ_EV_DORMIR,    /* dormiu numa cama ate de manha */
	CONQ_EV_MORTE,     /* o jogador morreu */
	CONQ_EV_CAPTURA,   /* capturou uma criatura */
	CONQ_EV_LENDARIA,  /* capturou uma criatura lendaria */
	CONQ_EV_EVOLUCAO,  /* uma criatura evoluiu */
	CONQ_EV_SALVAR     /* salvou o mundo */
};
void Conquistas_Evento(int evento);
/* Abre a lista de conquistas (botao "Conquistas..." do menu de pausa) */
void Conquistas_Mostrar(void);


/*########################################################################################################################*
*----------------------------------------------------------Salvar---------------------------------------------------------*
*#########################################################################################################################*/
/* O mundo do modo um jogador fica sempre neste arquivo (um "slot" so, como no Minecraft de console) */
#define SALVAR_ARQUIVO        "maps/meu-mundo.cw"
/* Ao comecar um mundo novo, o salvo anterior vira este (da para abrir em "Load level...") */
#define SALVAR_ANTERIOR       "maps/meu-mundo-anterior.cw"
/* true: o proximo "Jogar" abre o mundo salvo em vez de gerar um novo (botao Continuar do inicio) */
#define SALVAR_OPT_CONTINUAR  "mundo-continuar"
/* De quantos em quantos minutos o jogo salva sozinho (0 = nunca) */
#define SALVAR_OPT_AUTO       "mundo-autosalvar"

/* Se existe um mundo salvo para continuar */
cc_bool Salvar_ExisteMundo(void);
/* Chamado quando o modo um jogador comeca. Retorna true se abriu o mundo salvo. */
cc_bool Salvar_IniciarMundo(void);
/* Salva o mundo atual agora. automatico = salvamento automatico (avisa menos). */
cc_bool Salvar_Agora(cc_bool automatico);

CC_END_HEADER
#endif
