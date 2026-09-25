"""Read-only UE2 v128 package table/source inspector. Never modifies game files."""
import argparse, hashlib, json, pathlib, struct

class Reader:
    def __init__(self, data, at=0): self.data, self.at = data, at
    def take(self, n):
        if n < 0 or self.at + n > len(self.data): raise ValueError('Truncated package')
        result = self.data[self.at:self.at+n]; self.at += n; return result
    def u8(self): return self.take(1)[0]
    def u32(self): return struct.unpack('<I',self.take(4))[0]
    def i32(self): return struct.unpack('<i',self.take(4))[0]
    def compact(self):
        first = self.u8(); value = first & 0x3f; more = first & 0x40; shift = 6
        for i in range(4):
            if not more: break
            byte = self.u8()
            if i == 3 and byte & 0xe0: raise ValueError('Invalid compact index')
            value |= (byte & 0x7f) << shift; shift += 7; more = byte & 0x80
        if more or value > 0x7fffffff: raise ValueError('Compact index overflow')
        return -value if first & 0x80 else value
    def string(self):
        n = self.compact()
        if abs(n) > 16000000: raise ValueError('Oversized string')
        data = self.take(abs(n) * (2 if n < 0 else 1))
        if not n: return ''
        terminator = b'\x00\x00' if n < 0 else b'\x00'
        if not data.endswith(terminator): raise ValueError('Unterminated string')
        return data[:-len(terminator)].decode('utf-16le' if n < 0 else 'cp1252',errors='replace')

class Package:
    def __init__(self, path):
        self.path=pathlib.Path(path); self.data=self.path.read_bytes(); r=Reader(self.data)
        if r.u32()!=0x9e2a83c2: raise ValueError('Not an Unreal package')
        packed=r.u32(); self.version=packed&65535; self.licensee=packed>>16
        if self.version!=128: raise ValueError('Only audited KF package version 128 is supported')
        self.flags=r.u32(); nc,no,ec,eo,ic,io=[r.i32() for _ in range(6)]
        for count, offset in [(nc,no),(ec,eo),(ic,io)]:
            if not 0<=count<=1000000 or not 0<=offset<len(self.data): raise ValueError('Invalid table bounds')
        r=Reader(self.data,no); self.names=[]
        for _ in range(nc): self.names.append(r.string()); r.u32()
        r=Reader(self.data,io); self.imports=[]
        for _ in range(ic): self.imports.append(dict(class_package=r.compact(),class_name=r.compact(),outer=r.i32(),name=r.compact()))
        r=Reader(self.data,eo); self.exports=[]
        for _ in range(ec):
            item=dict(class_ref=r.compact(),super_ref=r.compact(),outer=r.i32(),name=r.compact(),flags=r.u32(),size=r.compact())
            item['offset']=r.compact() if item['size'] else 0
            if item['size']<0 or item['offset']<0 or item['offset']+item['size']>len(self.data): raise ValueError('Invalid export bounds')
            self.exports.append(item)
        for item in self.imports+self.exports: self.name(item['name'])
    def name(self,index):
        if not 0<=index<len(self.names): raise ValueError('Invalid name reference')
        return self.names[index]
    def ref(self,index,depth=0):
        if not index: return 'Class'
        if depth>64: raise ValueError('Cyclic object reference')
        table=self.exports if index>0 else self.imports; i=index-1 if index>0 else -index-1
        if i>=len(table): raise ValueError('Invalid object reference')
        item=table[i]; name=self.name(item['name'])
        return self.ref(item['outer'],depth+1)+'.'+name if item['outer'] else name
    def source(self,item):
        r=Reader(self.data[item['offset']:item['offset']+item['size']])
        # UTextBuffer is UObject property terminator, cursor position, top line, FString.
        if self.name(r.compact())!='None': raise ValueError('Unexpected TextBuffer property header')
        r.i32();r.i32();text=r.string()
        if r.at!=len(r.data): raise ValueError('Unexpected trailing TextBuffer data')
        return text
    def inspect(self,source_dir=None):
        classes=[]; extracted=[]; failures=[]
        for i,item in enumerate(self.exports):
            typename=self.ref(item['class_ref'])
            if not item['class_ref']: classes.append(dict(name=self.name(item['name']),parent=self.ref(item['super_ref']) if item['super_ref'] else None))
            if typename.endswith('TextBuffer'):
                try:
                    text=self.source(item); outer=self.ref(item['outer']); name=outer.split('.')[-1]
                    if text and source_dir:
                        if not name.replace('_','').isalnum(): raise ValueError('Unsafe class filename')
                        dest=pathlib.Path(source_dir)/self.path.stem/'Classes'/(name+'.uc');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(text.replace('\r\n','\n').replace('\r','\n'),encoding='utf-8',newline='\n')
                    extracted.append(dict(owner=outer,characters=len(text),sha256=hashlib.sha256(text.encode()).hexdigest()))
                except (ValueError,IndexError) as e: failures.append(dict(export=i+1,error=str(e)))
        return dict(file=self.path.name,sha256=hashlib.sha256(self.data).hexdigest(),version=self.version,licensee=self.licensee,names=len(self.names),exports=len(self.exports),imports=len(self.imports),classes=classes,text_buffers=extracted,source_errors=failures)

def main():
    p=argparse.ArgumentParser();p.add_argument('system',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sources',type=pathlib.Path);a=p.parse_args()
    packages=[]
    for name in ['Core','Engine','UnrealGame','XGame','ROEngine','KFMod','KFChar','KFGui','KFMutators','KFStoryGame','IpDrv']:
        path=a.system/(name+'.u')
        if path.exists():
            report=Package(path).inspect(a.sources);packages.append(report)
            print(name, 'classes',len(report['classes']),'sources',len(report['text_buffers']),'errors',len(report['source_errors']))
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(packages,indent=2),encoding='utf-8')
if __name__=='__main__': main()

