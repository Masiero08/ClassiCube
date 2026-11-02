# Sons e músicas próprios (ClassiCube 3DS)

Todos os sons e músicas daqui são **sintetizados do zero** por estes scripts
(só `numpy` + `ffmpeg`). Nenhum som foi gravado ou copiado de outro jogo.

| Script | O que gera |
|---|---|
| `gerar_sons.py` | `saida/classicube.zip` (passos e quebrar blocos) e `saida/efeitos.zip` (56 efeitos `fx_*.wav`) |
| `gerar_musica.py` | `saida/musica/*.ogg` (10 músicas) |
| `previa.py` | uma prévia em MP3 com um pedaço de cada coisa |
| `sintese.py` | osciladores, filtros, reverb usados pelos outros |

Rodar de novo: `python3 misc/sons/gerar_sons.py && python3 misc/sons/gerar_musica.py`.

## No cartão SD

Tudo vai em `sd:/3ds/ClassiCube/audio/`:

- `classicube.zip` – passos/blocos (troca o pacote antigo)
- `efeitos.zip` – efeitos do jogo; se ele existir o jogo usa os sons próprios e o
  launcher não pede para baixar as músicas/sons do Minecraft
- `dia_*.ogg`, `noite_*.ogg`, `nether_*.ogg`, `fim_*.ogg`, `batalha_*.ogg`, `chefao_*.ogg`

## Como o jogo escolhe a música (src/Sons.c)

- dia / noite (só no modo sobrevivência), Nether, Fim
- batalha: sua criatura lutando ou duelo com um Mestre
- chefão: barra de vida de chefão na tela, Ender Dragon vivo, ou luta com lendária

Trocar para batalha/chefão/dimensão corta a faixa atual com um fade curtinho;
dia ↔ noite espera a faixa terminar. Entre faixas calmas a pausa é de 20 s a 1,5 min.

## Efeitos (`Sons_Tocar`, `Sons_TocarEm`, `Sons_Voz`)

As vozes saem pelo nome do modelo (zumbi, porco, dragão...) ou, para as
criaturas, pelo tipo delas (fogo, água...). Criatura/monstro maior = voz mais grossa.
Para criar um som novo: acrescente uma função `fx_...` e uma linha em `EFEITOS`
no `gerar_sons.py`, gere de novo e chame `Sons_Tocar("nome")` no código.
