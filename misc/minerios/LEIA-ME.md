# Minerios & Monstros

Mod inspirado no OreSpawn, so para o modo um jogador. Nomes e texturas sao originais.

- Blocos (IDs 100-106): minerios de rubi, titanio, uranio (brilha) e ovo; blocos de rubi, titanio e uranio.
- Itens (IDs 107-110 e 119-122, 127, blocos "sprite" que nao sao colocados): Espada de titanio (7 de dano), Espada suprema (14), Big Bertha (30, acerta todos os monstros num arco na frente) e Armadura de titanio (botao de colocar para vestir; leva metade do dano).
- Dimensoes (IDs 111-117): Portal da Utopia e Portal de Cristal. Entrar no portal leva para a dimensao; entrar no portal de dentro dela volta ao mundo normal. Cada mundo fica salvo em maps/orespawn_*.cw, e a vida e os blocos do modo sobrevivencia vao junto.
  - Utopia: ceu turquesa, muitas arvores e flores, minerios raros de sobra.
  - Cristal: pedra, grama, troncos e folhas de cristal, cristais que brilham, ceu roxo.
- Minerio de ovo choca uma criatura; as raras sao gigantes bravos (Aranha Gigante, Zumbi Titan, Creeper Colosso, Galinha Monstro) que deixam minerios ao serem derrotados.
- Bloco da sorte (ID 118): aparece no chao (bem mais na Utopia) ou e feito com 1 minerio de rubi + 2 minerios de ouro. Ao quebrar sai uma surpresa: minerios, espadas, armadura, ate a Big Bertha (rara), torre de ouro, flores, filhotes, ovos; ou azar: mosquitos, monstro gigante, explosao ou gaiola de vidro.
- Mosquitos: enxames aparecem de vez em quando (mais na Utopia), voam em volta da cabeca e picam. As vezes vem a Rainha dos Mosquitos (chefao, 80 de vida), que chama mais mosquitos e deixa a Espada suprema.
- Chefoes em ordem de dificuldade, chamados colocando um altar. Cada um deixa um trofeu que faz o altar do proximo:
  1. Rainha dos Mosquitos (80 de vida; tambem aparece sozinha com os enxames) - Ferrao da Rainha. Altar: 3 minerios de rubi + 1 minerio de ovo.
  2. Golem de Titanio (180; pisao que joga para cima) - Nucleo de titanio. Altar: Ferrao + 2 blocos de titanio.
  3. Aranha de Cristal (300; da botes rapidos) - Olho de cristal. Altar: Nucleo + 4 pedras de cristal.
  4. Rei Creeper (450; explode tudo em volta) - Coracao de uranio. Altar: Olho + 2 blocos de uranio.
- Espada Devastadora (50 de dano, acerta todos os monstros em volta): Coracao de uranio + Big Bertha.
- Dragao: nasce do Ovo de dragao (dimensao de Cristal, raro na Utopia e no bloco da sorte). Domado com 3 rubis (segure minerio ou bloco de rubi e aperte colocar nele). Domado ele segue o jogador, briga com os monstros bravos, vem junto para todo mundo e da para montar (colocar nele; pular sobe, agachar desce, colocar de novo desce do dragao).
- Skins proprias para todos os monstros (galinha, leitao, carneiro, aranhas, zumbi, creeper e chefoes), desenhadas pelo gen_textures.py.
- Receitas novas no crafting do modo sobrevivencia (blocos, espadas, armadura e portais).
- `/client minerios [ovo [numero|todos] | mosquitos | sorte | rainha | chefe 1-4 | dragao | ir normal|utopia|cristal | limpar]`.
- Liga/desliga pela opcao `mod-minerios` (padrao ligado), usada pela tela Mods do launcher.

Texturas e modelo do mosquito: `python3 misc/minerios/gen_textures.py src/Minerios_tex.h [previa.png]`. Ficam em tiles livres do atlas (27, 43-46, 59-63, 87-95, 127 e 160-169) e nao sobrescrevem um pacote de texturas que ja use esses tiles.
