# Monta uma previa em MP3 com um pedaco de cada musica e os efeitos (para ouvir no celular)
import os, sys, io, zipfile, subprocess, wave
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sintese import SR, salvar_wav

AQUI = os.path.dirname(os.path.abspath(__file__))
S = os.path.join(AQUI, 'saida')

def ler_ogg(caminho):
    pcm = subprocess.run(['ffmpeg', '-v', 'error', '-i', caminho, '-f', 's16le', '-ac', '1', '-ar', str(SR), '-'],
                         capture_output=True, check=True).stdout
    return np.frombuffer(pcm, dtype='<i2').astype(float) / 32768

def ler_wav(data):
    w = wave.open(io.BytesIO(data))
    return np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(float) / 32768

partes, roteiro, t = [], [], 0.0
def add(x, nome=None, pausa=0.4):
    global t
    if nome: roteiro.append('%d:%02d  %s' % (int(t) // 60, int(t) % 60, nome))
    partes.append(x); partes.append(np.zeros(int(pausa * SR)))
    t += len(x) / SR + pausa

for nome, titulo in [('dia_1', 'Musica: Manha no Campo (dia)'), ('dia_2', 'Musica: Riacho (dia)'),
                     ('dia_3', 'Musica: Colinas (dia, valsa)'), ('noite_1', 'Musica: Ceu Estrelado (noite)'),
                     ('noite_2', 'Musica: Vagalumes (noite)'), ('nether_1', 'Musica: Brasas (Nether)'),
                     ('fim_1', 'Musica: Vazio (o Fim)'), ('batalha_1', 'Musica: Batalha de Criaturas'),
                     ('batalha_2', 'Musica: Duelo do Ginasio'), ('chefao_1', 'Musica: Chefao')]:
    x = ler_ogg(os.path.join(S, 'musica', nome + '.ogg'))[:int(16 * SR)]
    x[-int(1.5 * SR):] *= np.linspace(1, 0, int(1.5 * SR))
    add(x, titulo, 0.8)

grupos = [
    ('Blocos: grama, pedra, madeira, areia, neve, vidro', 'classicube.zip',
     ['dig_grass1', 'step_grass2', 'dig_stone1', 'step_stone2', 'dig_wood1', 'step_wood3', 'dig_sand1', 'dig_snow2', 'dig_glass1']),
    ('Criaturas: fogo, agua, planta, eletrico, vento, pedra, inseto, fantasma, gelo', 'efeitos.zip',
     ['fx_cri_fogo1', 'fx_cri_agua1', 'fx_cri_planta1', 'fx_cri_eletrico1', 'fx_cri_vento1', 'fx_cri_pedra1',
      'fx_cri_inseto1', 'fx_cri_fantasma1', 'fx_cri_gelo1']),
    ('Batalha: golpe e os 9 especiais, super golpe', 'efeitos.zip',
     ['fx_golpe1', 'fx_esp_fogo1', 'fx_esp_agua1', 'fx_esp_planta1', 'fx_esp_eletrico1', 'fx_esp_vento1',
      'fx_esp_pedra1', 'fx_esp_inseto1', 'fx_esp_fantasma1', 'fx_esp_gelo1', 'fx_super1']),
    ('Captura: jogar casulo, balancar, pegou! / escapou, subir de nivel, evolucao', 'efeitos.zip',
     ['fx_casulo1', 'fx_balanca1', 'fx_captura1', 'fx_escapou1', 'fx_nivel1', 'fx_evolucao1']),
    ('Mestres: desafio, insignia, lendaria apareceu', 'efeitos.zip', ['fx_desafio1', 'fx_insignia1', 'fx_lendaria1']),
    ('Monstros: zumbi, esqueleto, aranha, pavio do creeper, explosao', 'efeitos.zip',
     ['fx_zumbi1', 'fx_esqueleto1', 'fx_aranha1', 'fx_chiado1', 'fx_explosao1']),
    ('Animais: porco, ovelha, galinha', 'efeitos.zip', ['fx_porco1', 'fx_ovelha1', 'fx_galinha1']),
    ('Minerios & Monstros: mosquito, golem, chefao, dragao, sopro de fogo e gelo, portal, sorte, azar', 'efeitos.zip',
     ['fx_mosquito1', 'fx_golem1', 'fx_chefao1', 'fx_dragao1', 'fx_sopro_fogo1', 'fx_sopro_gelo1', 'fx_portal1', 'fx_sorte1', 'fx_azar1']),
    ('Sobrevivencia: dano, comer, craft, bau, fornalha, ferramenta quebrou, dormir, morte', 'efeitos.zip',
     ['fx_dano1', 'fx_comer1', 'fx_criar1', 'fx_bau_abrir1', 'fx_bau_fechar1', 'fx_fogo1', 'fx_quebrou1', 'fx_dormir1', 'fx_morte1']),
]
for titulo, z, nomes in grupos:
    zf = zipfile.ZipFile(os.path.join(S, z))
    primeiro = True
    for n in nomes:
        add(ler_wav(zf.read(n + '.wav')) * 0.8, titulo if primeiro else None, 0.35)
        primeiro = False
    add(np.zeros(int(0.5 * SR)))

x = np.concatenate(partes)
destino = sys.argv[1] if len(sys.argv) > 1 else os.path.join(S, 'previa.mp3')
tmp = destino + '.wav'
salvar_wav(tmp, x, pico=0.9, alinhar=False)
subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', tmp, '-c:a', 'libmp3lame', '-b:a', '96k', destino], check=True)
os.remove(tmp)
open(os.path.splitext(destino)[0] + '-roteiro.txt', 'w').write('\n'.join(roteiro) + '\n')
print('\n'.join(roteiro))
print('%.0f s' % (len(x) / SR))
