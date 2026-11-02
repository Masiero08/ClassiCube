#ifndef CC_CRIATURAS_H
#define CC_CRIATURAS_H
#include "Core.h"
CC_BEGIN_HEADER

/* Criaturas capturaveis para o modo um jogador (mod "Criaturas do Bioma").
   As criaturas nascem pelo mundo, podem ser atacadas, capturadas,
   seguem o jogador e ganham nivel/evoluem.
*/
struct IGameComponent;
extern struct IGameComponent Criaturas_Component;

/* Chamado quando o jogador aperta colocar (place = true) ou quebrar bloco. */
/* Retorna true se o clique foi numa criatura (e o bloco nao deve ser mexido). */
cc_bool Criaturas_HandleClick(cc_bool place);

/* Se o mod esta ligado no mundo atual (modo um jogador) */
cc_bool Criaturas_Ativo(void);
/* Abre o menu das criaturas (Diario, equipe, casulos, Mestres) */
void Criaturas_AbrirMenu(void);
/* Quantas especies ja foram capturadas e as insignias ganhas (um bit por Mestre) */
void Criaturas_Progresso(int* especies, int* insignias);
/* Tipo (fogo, agua...) da especie com esse nome de modelo, ou -1 (para a voz dela) */
int Criaturas_TipoDoModelo(const char* modelo);
/* 0 = sem batalha, 1 = criatura do jogador lutando, 2 = luta contra uma lendaria */
int Criaturas_Batalha(void);

CC_END_HEADER
#endif
