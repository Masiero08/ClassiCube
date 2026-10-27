# Criaturas do Bioma

Mod de criaturas capturáveis para o modo um jogador do ClassiCube (feito pensando no 3DS).
Todas as criaturas, nomes e texturas são originais.

## Como jogar
- `/client inicial` mostra as criaturas iniciais; `/client inicial Faiscol` escolhe uma.
- **Quebrar bloco (R no 3DS)** numa criatura selvagem: ataca. A sua criatura entra na briga.
- **Colocar bloco (L no 3DS)** numa criatura selvagem: tenta capturar (mais fácil com pouca vida).
- **Colocar bloco** na sua criatura: dá uma fruta-cura (recarrega em 20 s). Se ela já está
  com a vida cheia, troca pela próxima criatura da equipe.
- `/client equipe` lista a equipe (até 6); `/client equipe 2` solta a nº 2; `/client equipe guardar`.
- `/client criatura [nome|todas] [nivel]` faz criaturas aparecerem na frente (para testar).

As criaturas ganham nível ajudando a derrotar selvagens e evoluem em certos níveis.

## Batalha
- A selvagem atacada **revida** contra a sua criatura; quando fica fraca (menos de 30% da vida), foge.
- Cada golpe solta partículas do tipo (fogo, água, folhas, raio, vento, pedra, inseto).
- O chat avisa quando o tipo ajuda ("golpe em cheio") ou atrapalha ("golpe fraco").
- Se a sua criatura chega a 0 de vida, ela **desmaia**, volta para o casulo e a próxima da equipe entra.
- As criaturas guardadas no casulo **descansam** e recuperam 10% da vida a cada 3 s.
- No canto de cima à direita aparecem a sua criatura, o adversário (com barra de vida) e a equipe
  (amarelo = fora do casulo, verde = pronta, cinza = desmaiada).

Tipos: fogo > planta/inseto; água > fogo/pedra; planta > água/pedra; elétrico > água/vento;
vento > planta/inseto; pedra > fogo/vento; inseto > planta.
A equipe fica salva nas opções (`criaturas-equipe`).

## Especies
| Linha | Estágios | Tipo | Onde nasce |
|---|---|---|---|
| Raposas | Faiscol → Labareda (nv 5) → Fornalobo (nv 10) | Fogo | areia |
| Bolhas | Gotinha → Marolha (nv 5) → Tsunamor (nv 10) | Água | água |
| Porquinhos | Brotinho → Musgao (nv 5) → Florestauro (nv 10) | Planta | grama |
| Esquilos | Choquilo → Trovesquilo (nv 8) | Elétrico | grama |
| Passarinhos | Penape → Ventania (nv 7) | Vento | lugares altos |
| Pedras | Seixo → Pedregulho (nv 8) | Pedra | pedra/cascalho |
| Lagartas | Larvito → Casulim (nv 4) → Asalume (nv 7) | Inseto | grama |
| Caranguejos | Pincudo → Tenazor (nv 9) | Água | areia |
| Toupeiras | Toupito → Cavucao (nv 9) | Pedra | pedra/cascalho |

## Editar
Modelos, pinturas e status ficam em `criaturas.py`. Depois de mudar, rode na raiz do repositório:

    python3 misc/criaturas/criaturas.py

Isso regrava `misc/criaturas/texturas/*.png` e `src/CriaturasDados.h` (as texturas vão embutidas no
executável, então não precisa mexer no texture pack). Um texture pack pode trocar a textura de uma
criatura com um arquivo de mesmo nome, por exemplo `faiscol.png` (64x64).
