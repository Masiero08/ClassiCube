#ifndef CC_SONS_H
#define CC_SONS_H
#include "Core.h"
#include "Vectors.h"
/* Sons proprios do ClassiCube 3DS: efeitos com nome (audio/efeitos.zip),
   vozes das criaturas e monstros perto do jogador e o clima da musica
   (dia, noite, Nether, Fim, batalha, chefao).
   Os sons sao gerados por misc/sons/gerar_sons.py e as musicas por
   misc/sons/gerar_musica.py.
*/
CC_BEGIN_HEADER

struct IGameComponent;
struct Entity;
extern struct IGameComponent Sons_Component;

/* Toca um efeito sem posicao (interface, o proprio jogador) */
void Sons_Tocar(const char* nome);
/* Toca um efeito vindo de um ponto do mundo: mais baixo quanto mais longe */
void Sons_TocarEm(const char* nome, Vec3 pos);
/* Igual a Sons_TocarEm, com tom (100 = normal, 150 = mais agudo, 70 = mais grave) */
void Sons_TocarTom(const char* nome, Vec3 pos, int tom);
/* A voz da criatura/monstro/animal (pelo modelo dele). tom 100 = normal; 130 = "ai!" */
void Sons_Voz(struct Entity* e, int tom);

CC_END_HEADER
#endif
