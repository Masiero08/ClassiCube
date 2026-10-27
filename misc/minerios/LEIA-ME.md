# Minerios & Monstros

Mod pequeno inspirado no OreSpawn, so para o modo um jogador. Nomes e texturas sao originais.

- Blocos (IDs 100-106): minerio de rubi, minerio de titanio, minerio de uranio (brilha), minerio de ovo, bloco de rubi, bloco de titanio, bloco de uranio (brilha).
- Mundos novos ganham veios desses minerios na pedra (src/Generator.c).
- Quebrar um minerio de ovo choca uma criatura: galinha, leitao, carneiro, aranha da caverna ou, mais raro, um gigante bravo (Aranha Gigante, Zumbi Titan, Creeper Colosso, Galinha Monstro) que persegue e empurra o jogador. Os bravos somem depois de 90 s.
- `/client minerios` lista tudo; `/client minerios ovo [numero|todos]` choca um ovo na sua frente; `/client minerios limpar` remove os monstros.
- Liga/desliga pela opcao `mod-minerios` (padrao ligado), usada pela tela Mods do launcher.

Texturas: `python3 misc/minerios/gen_textures.py src/Minerios_tex.h [previa.png]`. Ficam nos tiles 160-166 do atlas e nao sobrescrevem um pacote de texturas que ja use esses tiles. Os minerios sao desenhados por cima da pedra do pacote.
