import sys
from PIL import Image, ImageDraw, ImageFont
S=sys.argv[1]
B="/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"; R="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
W,H=880,1050
def base():
    im=Image.new("RGB",(W,H),(18,18,22)); d=ImageDraw.Draw(im)
    d.rounded_rectangle((8,8,W-8,516),28,fill=(52,54,62))
    d.rounded_rectangle((8,526,W-8,H-8),28,fill=(46,48,56))
    d.rectangle((36,26,844,498),fill=(10,10,12))
    d.rectangle((116,544,764,1032),fill=(10,10,12))
    # hinge
    d.rectangle((60,516,W-60,526),fill=(30,30,34))
    return im,d
def tela(d,titulo,linhas,dica,cor=(120,200,90),num=None):
    d.rectangle((120,548,760,1028),fill=(236,234,226))
    d.rectangle((120,548,760,628),fill=cor)
    f=ImageFont.truetype(B,40); d.text((440,588),titulo,font=f,fill="white",anchor="mm")
    if num: d.text((140,560),num,font=ImageFont.truetype(B,20),fill=(255,255,255))
    f=ImageFont.truetype(R,30); y=680
    for l in linhas:
        d.text((440,y),l,font=f,fill=(40,40,44),anchor="mm"); y+=52
    d.rectangle((140,930,740,1010),fill=(60,62,72))
    f=ImageFont.truetype(B,24); yy=970-(14 if "\n" in dica else 0)
    d.multiline_text((440,970),dica,font=f,fill=(255,230,120),anchor="mm",align="center",spacing=8)
    d.text((750,1018),"gravado no PC",font=ImageFont.truetype(R,16),fill=(140,140,140),anchor="rd")
cenas={
 "mundo":("Mundo vivo",["Cerejeiras, pinheiros, flores,","abóboras e bichos andando"],"Circle Pad: câmera  •  D-Pad: andar",(110,180,80)),
 "craft":("Mesa de trabalho",["Grade 3x3 como no Minecraft:","toque na receita e no resultado"],"SELECT abre o crafting",(170,120,70)),
 "criaturas":("Criaturas",["29 espécies para achar e capturar","Batalha por toque: Atacar,","Especial, Defender, Capturar..."],"L: jogar casulo  •  R: atacar",(220,110,60)),
 "caverna":("Cavernas",["Túneis cheios de minérios,","lava e água lá embaixo"],"R: quebrar  •  L: colocar bloco",(100,100,110)),
 "ginasio":("Ginásio",["Cada bioma tem um Mestre","Vença os 6 e ganhe as insígnias"],"Entre e toque em Atacar para desafiar",(200,70,60)),
 "monstros":("Noite de monstros",["Dragão Esmeralda (dá pra domar!)","e a Rainha dos Mosquitos"],"Chefões têm barra de vida no topo",(90,60,150)),
 "fim":("O Fim",["Ilha do Fim com pilares de obsidiana","O Ender Dragon acordou!"],"Portal: 4 pérolas do Fim + diamante",(70,40,90)),
}
for i,(k,(t,l,h,c)) in enumerate(cenas.items()):
    im,d=base(); tela(d,t,l,h,c,f"{i+1}/7"); im.save(f"{S}/bg/{k}.png")
# title & end: whole thing
for k,top,sub,bot in [("titulo","ClassiCube 3DS","com os mods do Felipe",["Minérios & Monstros","Criaturas  •  Sobrevivência","Mundo vivo  •  Nether e Fim"]),
                      ("final","Valeu, Felipe!","Gameplay gravado no PC",["Mesmo código do .cia do 3DS,","rodando na versão de computador.","No console a imagem é menor."])]:
    im,d=base()
    d.rectangle((40,30,840,494),fill=(90,150,220))
    d.rectangle((40,370,840,494),fill=(100,170,70)); d.rectangle((40,400,840,494),fill=(120,85,55))
    d.text((440,190),top,font=ImageFont.truetype(B,72),fill="white",anchor="mm",stroke_width=4,stroke_fill=(30,40,60))
    d.text((440,280),sub,font=ImageFont.truetype(B,34),fill=(255,240,140),anchor="mm",stroke_width=3,stroke_fill=(30,40,60))
    d.rectangle((120,548,760,1028),fill=(236,234,226)); y=700
    for l in bot:
        d.text((440,y),l,font=ImageFont.truetype(B if k=="titulo" else R,32),fill=(40,40,44),anchor="mm"); y+=70
    im.save(f"{S}/bg/{k}.png")
