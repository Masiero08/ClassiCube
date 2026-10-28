# Criaturas do Bioma

Mod de criaturas capturáveis para o modo um jogador do ClassiCube (feito pensando no 3DS).
Todas as criaturas, nomes e texturas são originais.

## Como jogar
- `/client inicial` mostra as criaturas iniciais; `/client inicial Faiscol` escolhe uma.
- **Quebrar bloco (R no 3DS)** numa criatura selvagem: ataca. A sua criatura entra na briga.
- **Colocar bloco (L no 3DS)** numa criatura selvagem: joga um casulo para capturar (mais fácil com pouca vida).
- **Colocar bloco** na sua criatura: dá uma fruta-cura (recarrega em 20 s). Se ela já está
  com a vida cheia, troca pela próxima criatura da equipe.
- `/client equipe` lista a equipe (até 6); `/client equipe 2` solta a nº 2; `/client equipe guardar`.
- `/client criatura [nome|todas] [nivel]` faz criaturas aparecerem na frente (para testar).

As criaturas ganham nível ajudando a derrotar selvagens e evoluem em certos níveis.
A equipe fica salva nas opções (`criaturas-equipe`).

## Bau das Criaturas (kit do começo)
Na primeira vez que você entra num mundo com o mod ligado, aparece um **Baú das Criaturas** na sua frente.
Bata nele (R) ou toque nele (L): ele abre a tampa e dá 10 Casulos Reforçados, 5 Casulos Maré, 1 Casulo
Mestre e o Diário. Em seguida abre a escolha da criatura inicial. O baú só aparece uma vez (opção
`criaturas-bau`); `/client criatura bau` faz ele aparecer de novo para testar.

## Batalha
- A selvagem atacada **revida** contra a sua criatura; quando fica fraca (menos de 30% da vida), foge.
- Cada golpe solta partículas do tipo (fogo, água, folhas, raio, vento, pedra, inseto).
- O chat avisa quando o tipo ajuda ("golpe em cheio") ou atrapalha ("golpe fraco").
- Se a sua criatura chega a 0 de vida, ela **desmaia**, volta para o casulo e a próxima da equipe entra.
- As criaturas guardadas no casulo **descansam** e recuperam 10% da vida a cada 3 s.
- No canto de cima à direita aparecem a sua criatura, o adversário (com barra de vida) e a equipe
  (amarelo = fora do casulo, verde = pronta, cinza = desmaiada).

Tipos: fogo > planta/inseto; água > fogo/pedra; planta > água/pedra; elétrico > água/vento;
vento > planta/inseto; pedra > fogo/vento/gelo; inseto > planta; fantasma > fantasma/elétrico;
gelo > planta/vento; fogo também > gelo.

## Casulos de captura
| Casulo | Como ganhar | Efeito |
|---|---|---|
| Comum | sempre tem | chance normal (15% com vida cheia, quase 100% quase sem vida) |
| Reforçado | selvagens derrotadas às vezes deixam; 3 por Mestre vencido | chance bem maior |
| Maré | selvagens derrotadas às vezes deixam | muito forte contra criaturas de água |
| Mestre | 1 por Mestre vencido | captura sempre |

Cada casulo tem modelo e textura próprios (uma bolota com tampa): o Comum é marrom com faixa vermelha,
o Reforçado é de metal com rebites, o Maré é azul com ondas e o Mestre é dourado com tampa roxa.
O casulo voa até a criatura, prende ela e balança: três balanços e ela é sua; se parar antes, ela escapa.

No automático o jogo usa o melhor casulo que você tem (sem gastar o Mestre).
`/client casulo` mostra quantos você tem; `/client casulo mestre` (ou reforcado, mare, comum, auto) escolhe.
Criaturas de um Mestre não podem ser capturadas.

## Mestres de Bioma
Seis treinadores originais aparecem pelo mundo, um de cada vez, na ordem abaixo. Bata neles (R) para
desafiar: eles mandam as criaturas uma por uma contra a sua. Vencendo a equipe toda você ganha a insígnia,
3 Casulos Reforçados e 1 Casulo Mestre. Se toda a sua equipe desmaiar, dá para tentar de novo depois.

| Mestre | Tipo | Equipe | Insígnia |
|---|---|---|---|
| Mestre Tronco | Planta | Larvito 6, Brotinho 7, Musgao 10 | Folha |
| Mestra Maré | Água | Gotinha 9, Pincudo 10, Marolha 12 | Onda |
| Mestre Cascalho | Pedra | Seixo 12, Toupito 12, Pedregulho 15 | Rocha |
| Mestra Faísca | Elétrico | Choquilo 14, Choquilo 15, Trovesquilo 17 | Raio |
| Mestre Vendaval | Vento | Penape 16, Asalume 18, Ventania 20 | Pena |
| Mestra Brasa | Fogo | Labareda 20, Labareda 22, Fornalobo 25 | Chama |

`/client mestre` lista os Mestres e as insígnias; `/client mestre chamar` chama o próximo na sua frente.
As insígnias aparecem como estrelas no canto de cima.

## Diário das Criaturas
No 3DS, aperte Start e toque em **Criaturas...** para abrir o menu do mod (Diário, equipe,
criatura inicial, escolha do casulo, chamar Mestre e insígnias) sem digitar comandos.
`/client diario` abre o Diário: a lista das 26 espécies com as que você já viu e as que já capturou.
Tocar numa criatura mostra tipo, onde vive, status, evolução e uma descrição. Espécies que você ainda
não viu aparecem como `???`. O Diário fica salvo nas opções (`criaturas-vistas` e `criaturas-capturadas`).

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
| Fantasminhas | Vultinho → Assombrao (nv 9) | Fantasma | à noite, ou em cavernas fundas |
| Focas | Friozinho → Geleirao (nv 9) | Gelo | neve, gelo ou montanhas bem altas |

## Editar
Modelos, pinturas e status ficam em `criaturas.py`. Depois de mudar, rode na raiz do repositório:

    python3 misc/criaturas/criaturas.py

Isso regrava `misc/criaturas/texturas/*.png` e `src/CriaturasDados.h` (as texturas vão embutidas no
executável, então não precisa mexer no texture pack). Um texture pack pode trocar a textura de uma
criatura com um arquivo de mesmo nome, por exemplo `faiscol.png` (64x64).
