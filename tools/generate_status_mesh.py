"""Author the two-triangle HUD plane in the installed compiler's LWO2 format.
Format: LightWave SDK Object Files, https://documentation.help/LightWave/lwo2.html
No game assets are copied. Geometry is 100 by 50 file units before actor scale.
"""
import pathlib,struct

def make(path):
 def be(fmt,*values):return struct.pack('>'+fmt,*values)
 def name(s):
  data=s.encode('ascii')+b'\0';return data+b'\0'*(len(data)%2)
 def chunk(tag,data):return tag.encode('ascii')+be('I',len(data))+data+b'\0'*(len(data)%2)
 def sub(tag,data):return tag.encode('ascii')+be('H',len(data))+data+b'\0'*(len(data)%2)
 # Installed importer maps LightWave (x,y,z) to Unreal (x,-z,-y).
 vertices=[(0,-25,-50),(0,-25,50),(0,25,50),(0,25,-50)]
 uv=[(0,0),(1,0),(1,1),(0,1)]
 data=b'LWO2'+chunk('TAGS',name('DefaultTexture'))
 data+=chunk('LAYR',be('HHfff',0,0,0,0,0)+name('Panel'))
 data+=chunk('PNTS',b''.join(be('fff',*v) for v in vertices))
 data+=chunk('VMAP',b'TXUV'+be('H',2)+name('UVTexture')+b''.join(be('Hff',i,*v) for i,v in enumerate(uv)))
 data+=chunk('POLS',b'FACE'+be('HHHH',3,0,1,2)+be('HHHH',3,0,2,3))
 data+=chunk('PTAG',b'SURF'+be('HHHH',0,0,1,0))
 image=sub('IMAP',name('1')+sub('CHAN',b'COLR'))+sub('PROJ',be('H',5))+sub('VMAP',name('UVTexture'))
 surface=name('DefaultTexture')+name('')+sub('COLR',be('fffH',1,1,1,0))+sub('SIDE',be('H',3))+sub('BLOK',image)
 data+=chunk('SURF',surface)
 path=pathlib.Path(path);path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(chunk('FORM',data))
if __name__=='__main__':make(pathlib.Path(__file__).resolve().parents[1]/'unrealscript/KFVRGameplay/Models/KFVRStatusPanel.lwo')
