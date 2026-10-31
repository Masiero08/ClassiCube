import json, subprocess, sys
S=sys.argv[1]; c=json.load(open(f"{S}/cenas.json"))
escolha=[("mundo",2,0),("craft",1,0),("criaturas",3,0),("caverna",3,0),("ginasio",3,0),("monstros",1,0),("fim",1,5.5)]
lista=[]
def run(a): subprocess.run(a,check=True)
def card(nome,dur):
    out=f"{S}/seg/{nome}.mp4"
    run(["ffmpeg","-loglevel","error","-y","-loop","1","-t",str(dur),"-i",f"{S}/bg/{nome}.png","-vf",f"fade=in:0:12,fade=out:st={dur-0.4}:d=0.4,format=yuv420p","-r","30","-c:v","libx264","-crf","18",out]); lista.append(out)
card("titulo",3.5)
for k,take,maxd in escolha:
    a,b=c[str(take)][k]; a+=0.15; dur=(b-a-0.1) if not maxd else maxd
    out=f"{S}/seg/{k}.mp4"
    run(["ffmpeg","-loglevel","error","-y","-loop","1","-i",f"{S}/bg/{k}.png","-ss",str(a),"-t",str(dur),"-i",f"{S}/rec_{take}.mp4",
         "-filter_complex",f"[0][1]overlay=40:24:shortest=1,fade=in:0:9,fade=out:st={dur-0.3}:d=0.3,format=yuv420p","-t",str(dur),"-r","30","-c:v","libx264","-crf","18",out]); lista.append(out)
card("final",4.5)
open(f"{S}/seg/lista.txt","w").write("".join(f"file '{p}'\n" for p in lista))
run(["ffmpeg","-loglevel","error","-y","-f","concat","-safe","0","-i",f"{S}/seg/lista.txt","-c:v","libx264","-crf","20","-preset","slow","-pix_fmt","yuv420p","-movflags","+faststart",f"{S}/gameplay.mp4"])
