udg_QA_Count = 0
gg_rct_Region_000 = nil
gg_trg_Melee_Initialization = nil
gg_trg_QA_Custom_Text = nil
function InitGlobals()
	local i = 0
	udg_QA_Count = 5
end

function InitSounds()
end

function CreateAllDestructables()
	local d = nil
	local t = nil
	local life = 0
end

function CreateAllItems()
	BlzCreateItemWithSkin(FourCC("ratf"), -158.39673, -374.82422, FourCC("ratf"))
end

function CreateAllUnits()
	local u = nil
	local unitID = 0
	local t = nil
	local life = 0
end

function CreateRegions()
	local we = nil
	gg_rct_Region_000 = Rect(-416, -160, 128, 352)
end

function CreateCameras()
end

QA_HEADER_VALUE = 1
function QA_Header(x)
  return x + QA_HEADER_VALUE // 2
end

function Trig_Melee_Initialization_Actions()
TriggerSleepAction(2)
MeleeStartingVisibility()
MeleeStartingHeroLimit()
MeleeGrantHeroItems()
MeleeStartingResources()
MeleeClearExcessUnits()
MeleeStartingUnits()
MeleeStartingAI()
MeleeInitVictoryDefeat()
end

function InitTrig_Melee_Initialization()
	gg_trg_Melee_Initialization = CreateTrigger()
TriggerRegisterEnterRectSimple(gg_trg_Melee_Initialization, gg_rct_Region_000)
	TriggerAddAction(gg_trg_Melee_Initialization, Trig_Melee_Initialization_Actions)
end

function InitTrig_QA_Custom_Text()
end

function Trig_QA_Custom_Text_Actions()
  udg_QA_Count = udg_QA_Count + QA_Header(3)
end

function InitTrig_QA_Custom_Text()
  gg_trg_QA_Custom_Text = CreateTrigger()
  TriggerAddAction(gg_trg_QA_Custom_Text, Trig_QA_Custom_Text_Actions)
end
function InitCustomTriggers()
	InitTrig_Melee_Initialization()
	InitTrig_QA_Custom_Text()
end

function RunInitializationTriggers()
	ConditionalTriggerExecute(gg_trg_Melee_Initialization)
	ConditionalTriggerExecute(gg_trg_QA_Custom_Text)
end

function InitCustomPlayerSlots()
	SetPlayerStartLocation(Player(0), 0)
	SetPlayerColor(Player(0), ConvertPlayerColor(0))
	SetPlayerRacePreference(Player(0), RACE_PREF_HUMAN)
	SetPlayerRaceSelectable(Player(0), true)
	SetPlayerController(Player(0), MAP_CONTROL_USER)
	
end

function InitCustomTeams()
	SetPlayerTeam(Player(0), 0)
end

function main()
	SetCameraBounds(-3328.000000 + GetCameraMargin(CAMERA_MARGIN_LEFT), -3584.000000 + GetCameraMargin(CAMERA_MARGIN_BOTTOM), 3328.000000 - GetCameraMargin(CAMERA_MARGIN_RIGHT), 3072.000000 - GetCameraMargin(CAMERA_MARGIN_TOP), -3328.000000 + GetCameraMargin(CAMERA_MARGIN_LEFT), 3072.000000 - GetCameraMargin(CAMERA_MARGIN_TOP), 3328.000000 - GetCameraMargin(CAMERA_MARGIN_RIGHT), -3584.000000 + GetCameraMargin(CAMERA_MARGIN_BOTTOM))
	SetDayNightModels("Environment/DNC/DNCLordaeron/DNCLordaeronTerrain/DNCLordaeronTerrain.mdl", "Environment/DNC/DNCLordaeron/DNCLordaeronTerrain/DNCLordaeronTerrain.mdl")
	NewSoundEnvironment("Default")
	SetAmbientDaySound("LordaeronSummerDay")
	SetAmbientNightSound("LordaeronSummerNight")
	SetMapMusic("Music", true, 0)
	InitSounds()
	CreateRegions()
	CreateCameras()
	CreateAllDestructables()
	CreateAllItems()
	CreateAllUnits()
	InitBlizzard()
	InitGlobals()
	InitCustomTriggers()
	RunInitializationTriggers()
end

function config()
	SetMapName("Just another Warcraft III map")
	SetMapDescription("Nondescript")
	SetPlayers(1)
	SetTeams(1)
	SetGamePlacement(MAP_PLACEMENT_USE_MAP_SETTINGS)
	
	DefineStartLocation(0, -1856, -768)
	
	InitCustomPlayerSlots()
	SetPlayerSlotAvailable(Player(0), MAP_CONTROL_USER)
	InitGenericPlayerSlots()
end

