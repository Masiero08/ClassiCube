# Minerios & Monstros

Mod inspirado no OreSpawn, so para o modo um jogador. Nomes e texturas sao originais.

- Blocos (IDs 100-106): minerios de rubi, titanio, uranio (brilha) e ovo; blocos de rubi, titanio e uranio.
- Itens (IDs 107-110, blocos "sprite" que nao sao colocados): Espada de titanio (7 de dano), Espada suprema (14), Big Bertha (30, acerta todos os monstros num arco na frente) e Armadura de titanio (botao de colocar para vestir; leva metade do dano).
- Dimensoes (IDs 111-117): Portal da Utopia e Portal de Cristal. Entrar no portal leva para a dimensao; entrar no portal de dentro dela volta ao mundo normal. Cada mundo fica salvo em maps/orespawn_*.cw, e a vida e os blocos do modo sobrevivencia vao junto.
  - Utopia: ceu turquesa, muitas arvores e flores, minerios raros de sobra.
  - Cristal: pedra, grama, troncos e folhas de cristal, cristais que brilham, ceu roxo.
- Minerio de ovo choca uma criatura; as raras sao gigantes bravos (Aranha Gigante, Zumbi Titan, Creeper Colosso, Galinha Monstro) que deixam minerios ao serem derrotados.
- Mosquitos: enxames aparecem de vez em quando (mais na Utopia), voam em volta da cabeca e picam.
- Receitas novas no crafting do modo sobrevivencia (blocos, espadas, armadura e portais).
- `/client minerios [ovo [numero|todos] | mosquitos | ir normal|utopia|cristal | limpar]`.
- Liga/desliga pela opcao `mod-minerios` (padrao ligado), usada pela tela Mods do launcher.

Texturas e modelo do mosquito: `python3 misc/minerios/gen_textures.py src/Minerios_tex.h [previa.png]`. Ficam em tiles livres do atlas (27, 43-46, 59-62, 87-95, 127) e nao sobrescrevem um pacote de texturas que ja use esses tiles.
