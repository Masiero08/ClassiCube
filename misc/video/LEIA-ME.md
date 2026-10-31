# Video de gameplay (gravado no PC)

1. `make linux` e rode sob Xvfb 800x480 (`options.txt`: window-width=800, window-height=480, gui-showfps=false).
2. `CC_DEMO=misc/video/roteiro.txt ./ClassiCube Felipe` executa o roteiro (src/Demo.c) e escreve `DEMO-MARK <hora> <cena>` no stderr.
3. Grave com `ffmpeg -f x11grab -framerate 30 -video_size 800x480 -i :7 rec_N.mp4` (salve a hora de inicio em rec_N.start).
4. `telas.py` desenha a moldura do 3DS com a legenda da tela de baixo; `montar.py` corta cada cena da melhor tomada e junta tudo.

Comandos do roteiro: `w seg`, `c /comando`, `k Tecla 1|0`, `t dYaw dPitch seg`, `l yaw pitch seg`, `tap x y`,
`give bloco qtd`, `craft`, `fechar`, `aberto [raio]`, `caverna`, `ginasio`, `ginasio2`, `mirar modelo seg`, `recua n`, `limpa`, `mark nome`, `q`.
