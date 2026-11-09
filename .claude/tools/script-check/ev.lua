__evs={}
local function reg(t,ev) if t then __evs[#__evs+1]={t=t,ev=ev} end return __H("event") end
function TriggerRegisterPlayerUnitEvent(t,p,ev,f) return reg(t,ev) end
function TriggerRegisterAnyUnitEventBJ(t,ev) return reg(t,ev) end
function TriggerRegisterUnitEvent(t,u,ev) return reg(t,ev) end
__ctx={}
function GetManipulatedItem() return __ctx.item end
function GetItemTypeId(i) return i and i.tid or 0 end
function GetTriggerUnit() return __ctx.unit end
function GetBuyingUnit() return __ctx.unit end
function GetSellingUnit() return __ctx.seller end
function GetSoldItem() return __ctx.item end
function GetSpellAbilityId() return __ctx.abil or 0 end
function __fire(ev,label)
  local n=0
  for _,e in ipairs(__evs) do if e.ev==ev and e.t.on and TriggerEvaluate(e.t) then n=n+1 __runacts(e.t,label) end end
  __logf("FIRED "..label.." on "..n.." trigger(s) @t="..__now)
end
function __trace(names)
  for _,nm in ipairs(names) do local f=rawget(_G,nm) if f then rawset(_G,nm,function(...) __logf("CALL "..nm.." @t="..string.format("%.2f",__now)) return f(...) end) else __logf("MISSING "..nm) end end
end
function BlzLoadTOCFile() return true end
bj_mapInitialPlayableArea=Rect(-7680.,-7680.,7680.,7680.)
bj_mapInitialCameraBounds=Rect(-7000.,-7000.,7000.,7000.)
__dests={}
local function mk(id,x,y) local d=__H("destructable",{tid=id,x=x,y=y}) __dests[#__dests+1]=d return d end
function BlzCreateDestructableWithSkinPitchRollColor(id,x,y) return mk(id,x,y) end
function BlzCreateDestructableZWithSkinPitchRollColor(id,x,y) return mk(id,x,y) end
function BlzCreateDestructableZWithSkin(id,x,y) return mk(id,x,y) end
function BlzCreateDestructableWithSkin(id,x,y) return mk(id,x,y) end
function SetDestructableVertexColor() end
function EnumDestructablesInRect(r,f,cb) for _,d in ipairs(__dests) do __ed=d cb() end end
function GetEnumDestructable() return __ed end
function GetDestructableTypeId(d) return d and d.tid or 0 end
function GetWidgetX(w) return w and w.x or 0.0 end function GetWidgetY(w) return w and w.y or 0.0 end
function GetUnitX(u) return 0.0 end function GetUnitY(u) return 0.0 end
