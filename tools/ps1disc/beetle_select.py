"""psx-beetle (debug port 4380): from boot to the GAME MODE music select, then
list the CD seeks (Setloc) past sector 26000 and screenshot the screen.

usage: beetle_select.py SHOT_PATH_PREFIX SECTOR
  SECTOR: the seek that means the music select was reached (its pack's
  sector, e.g. a moved READ_DT entry 201).  Needs scripts/recomp-feasibility/dq.py.
"""
import sys,time
sys.path.insert(0,'/home/user/work/scripts/recomp-feasibility')
from dq import q, press
from PIL import Image
def shot(n): q(4380,{'cmd':'screenshot','path':n+'.png'}); return n+'.png'
def lang(im):
    w,h=im.size; return any(im.getpixel((w//2,y))[1]>110 and im.getpixel((w//2,y))[1]>im.getpixel((w//2,y))[0]+40 for y in range(int(h*.18),int(h*.32),2))
while not lang(Image.open(shot(sys.argv[1] + '_boot')).convert('RGB')): time.sleep(1)
def lba(e):
    f=lambda v:(v>>4)*10+(v&15); b=lambda x:int(x,16)
    return (f(b(e['a0']))*60+f(b(e['a1'])))*75+f(b(e['a2']))-150
want=int(sys.argv[2])
press(4380,'cross',8); time.sleep(15)
for i in range(8):
    r=q(4380,{'cmd':'cdrom_cmd_dump','count':4000})
    if any(e['cmd']=='0x02' and lba(e)==want for e in r['entries']): break
    press(4380,'start',8); time.sleep(4)
    press(4380,'circle',8); time.sleep(6)
time.sleep(3); shot(sys.argv[1])
r=q(4380,{'cmd':'cdrom_cmd_dump','count':4000})
print('cd commands',r.get('total'))
for e in r['entries']:
    if e['cmd']=='0x02' and lba(e)>=26000:
        l=lba(e); print(e['seq'],'Setloc',l,'<- EXTRA.BIN (the moved music select pack)' if l==105486 else ('<- the original pack' if l==26825 else ''))
