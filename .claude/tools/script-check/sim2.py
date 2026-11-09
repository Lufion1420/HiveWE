import sys,re
sys.path.insert(0,sys.argv[1])
import lupa.lua53 as lupa
import os
J=(sys.argv[5] if len(sys.argv)>5 else os.path.join(os.path.dirname(os.path.abspath(__file__)),"..","..","..","data","tools"))+"/"
script=open(sys.argv[2],encoding="utf-8",errors="replace").read()
tag=sys.argv[3]; here=sys.argv[4]
cj=open(J+"common.j",encoding="latin1").read(); bj=open(J+"blizzard.j",encoding="latin1").read()
sig=re.compile(r"^\s*(?:constant\s+)?(native|function)\s+(\w+)\s+takes\s+(.*?)\s+returns\s+(\w+)",re.M)
L=lupa.LuaRuntime(unpack_returned_tuples=True)
lua=L.globals()
L.execute(open(here+"/pre.lua",encoding="utf-8").read())
for src in (cj,bj):
    for m in sig.finditer(src):
        ps="" if m.group(3).strip()=="nothing" else ",".join(p.split()[0] for p in m.group(3).split(","))
        lua.__mkfn(m.group(2),m.group(4),ps)
for src in (cj,bj):
    for blk in re.findall(r"^globals(.*?)^endglobals",src,re.S|re.M):
        for line in blk.splitlines():
            line=line.split("//")[0].strip()
            m=re.match(r"(?:constant\s+)?(\w+)\s+(array\s+)?(\w+)\s*(?:=\s*(.*))?$",line)
            if m and m.group(1)!="native": lua.__mkglob(m.group(1),bool(m.group(2)),m.group(3),(m.group(4) or "").strip())
L.execute(open(here+"/impl.lua",encoding="utf-8").read())
L.execute(open(here+"/ev.lua",encoding="utf-8").read())
lf=L.eval("function(s) local f,e=load(s,\"=war3map.lua\") return {f=f,e=e} end")
r=lf(script); f,e=r.f,r.e
if not f: print(tag,"LOAD ERROR",e); sys.exit()
lua.__run(f,"chunk")
lua.__run(lua.config,"config")
lua.__run(lua.main,"main")
L.execute('__trace({"Trig_Game_Starts_Actions","InitFullScreenFrame","StartAfterStart","Point3DGetVec3","PostGenerationPass","Trig_Init_Actions"}) if RPGMinimap then local f=RPGMinimap.Init RPGMinimap.Init=function(...) __logf("CALL RPGMinimap.Init") local r=f(...) __logf("RET RPGMinimap.Init") return r end end')
L.execute('local p=print print=function(...) __logf("PRINT "..table.concat({tostring((...))}," ")) end')
lua.__loop(10.0)
lua.__logf("==== INIT DONE, firing pickup I004 ====")
L.execute('__trace({"StartGame","StartSpecialUI","StartGameTimer","ResetKillCounter","UIFadeOut","EnableMask","StartCustomMusic","EnableDefaultConfirmation","SetUnitPosition"})')
L.execute('if PlayerObject and PlayerObject.setUnit then local f=PlayerObject.setUnit PlayerObject.setUnit=function(...) __logf("CALL PlayerObject.setUnit @t="..__now) return f(...) end end')
L.execute('local f=UITimer.callDelayed UITimer.callDelayed=function(self,t,cb,...) local a=table.pack(...) return f(self,t,function(...) local ok,e=pcall(cb,...) if not ok then __logf("CB ERROR "..tostring(e)) error(e) end end,...) end')
L.execute("__ticks=0")
L.execute('__ctx.item=__H("item",{tid=FourCC("I004")}) __ctx.unit=__H("unit",{}) __fire(EVENT_PLAYER_UNIT_PICKUP_ITEM,"pickup")')
lua.__loop(25.0)
L.execute("__logf(\"END t=\"..__now..\" ticks=\"..__ticks)")
log=lua.__log
for i in range(1,len(log)+1): print(tag,log[i])
