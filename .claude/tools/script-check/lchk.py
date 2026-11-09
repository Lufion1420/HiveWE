import sys,re
sys.path.insert(0,sys.argv[3])
import lupa
new=open(sys.argv[1],encoding='utf-8',errors='replace').read(); old=open(sys.argv[2],encoding='utf-8',errors='replace').read()
L=lupa.LuaRuntime()
for n,s in(('new',new),('orig',old)):
    r=L.eval('function(s) local f,e=load(s) return {f~=nil,e} end')(s); f,e=r[1],r[2]
    print(n,'load:', 'OK' if f else e, len(s.splitlines()),'lines')
def sets(s):
    g=set(re.findall(r'^\s*gg_trg_\w+\s*=?\s*nil|^\s*gg_trg_\w+',s,re.M))
    decl=set(re.findall(r'gg_trg_\w+',s.split('function InitGlobals')[0] if False else s))
    calls=set(re.findall(r'InitTrig_(\w+)\(\)\s*$',s,re.M))
    defs=set(re.findall(r'function InitTrig_(\w+)\(',s))
    return decl,calls,defs
dn,cn,fn=sets(new); do,co,fo=sets(old)
print('gg_trg_ ids',len(dn),len(do),'diff',sorted(dn^do))
print('InitTrig calls',len(cn),len(co),'diff',sorted(cn^co))
print('defs',len(fn),len(fo),'diff',sorted(fn^fo))
# declared = in "trigger gg_trg_X" global declaration style
decl=lambda s:set(re.findall(r'^\s*(?:gg_trg_\w+)\s*=\s*nil',s,re.M))
print('decl form new/orig',len(decl(new)),len(decl(old)))
# call within InitCustomTriggers
m=re.search(r'function InitCustomTriggers\(\)(.*?)\nend',new,re.S); mo=re.search(r'function InitCustomTriggers\(\)(.*?)\nend',old,re.S)
cn2=set(re.findall(r'InitTrig_(\w+)\(\)',m.group(1))); co2=set(re.findall(r'InitTrig_(\w+)\(\)',mo.group(1)))
print('ICT calls',len(cn2),len(co2),'diff',sorted(cn2^co2))
print('calls w/o def',sorted(cn2-fn)); 
used=set(re.findall(r'gg_trg_\w+',new)); dd=set(x.strip().split()[0] for x in decl(new)) if False else set(re.findall(r'^\s*(gg_trg_\w+)\s*=\s*nil',new,re.M))
print('used not declared',sorted(used-dd)); print('declared count',len(dd))
for line in new.splitlines():
    pass
print('trailing comma:',[l for l in new.splitlines() if re.search(r',\s*\)',l)][:5])
print('GameLoaded:',[l.strip() for l in new.splitlines() if 'TriggerRegisterGameLoadedEventBJ' in l][:3])
print('quoted ops:',[l.strip() for l in new.splitlines() if re.search(r'"(OperatorCompareBoolean|OperatorCompare\w*|OperatorEqualENE|OperatorEqualNENE|OPERATOR\w*|EQUAL|NOT_EQUAL|GREATER\w*|LESS\w*)"|"(==|!=|<=|>=|<|>)"',l)][:5])
print('null:',[l.strip() for l in new.splitlines() if re.search(r'\bnull\b',l)][:5])
