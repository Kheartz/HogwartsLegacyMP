-- PlayerDumper: logs player class hierarchy, position, and rotation.
-- Press F6 in-game to trigger a dump to UE4SS.log.

local function dump_player()
    print("[HogwartsMP] === Player State Dump ===")

    local pc = FindFirstOf("PlayerController")
    if not pc or not pc:IsValid() then
        print("[HogwartsMP] No PlayerController found yet.")
        return
    end

    print(string.format("[HogwartsMP] PlayerController: %s",
        pc:GetClass():GetFName():ToString()))

    -- GetPawn() can return nullptr if called too early; fall back to the specific player class
    local pawn = nil
    local ok = pcall(function() pawn = pc:GetPawn() end)
    if not ok or not pawn or not pawn:IsValid() then
        print("[HogwartsMP] GetPawn() nil, falling back to FindFirstOf BP_Biped_Player_C...")
        pawn = FindFirstOf("BP_Biped_Player_C")
    end

    if not pawn or not pawn:IsValid() then
        print("[HogwartsMP] No Pawn found.")
        return
    end

    -- Class name
    print(string.format("[HogwartsMP] Pawn class: %s",
        pawn:GetClass():GetFName():ToString()))

    -- Class hierarchy (so we know what to hook against)
    local hierarchy = {}
    local c = pawn:GetClass()
    while c and c:IsValid() do
        table.insert(hierarchy, c:GetFName():ToString())
        c = c:GetSuperStruct()
    end
    print("[HogwartsMP] Hierarchy: " .. table.concat(hierarchy, " -> "))

    -- K2_GetActorLocation/Rotation are the Blueprint-exposed versions that work in UE4SS Lua
    local lok, loc = pcall(function() return pawn:K2_GetActorLocation() end)
    local rok, rot = pcall(function() return pawn:K2_GetActorRotation() end)
    if lok and loc then
        print(string.format("[HogwartsMP] Location : X=%.2f  Y=%.2f  Z=%.2f",
            loc.X, loc.Y, loc.Z))
    else
        print("[HogwartsMP] Location : failed")
    end
    if rok and rot then
        print(string.format("[HogwartsMP] Rotation : Pitch=%.2f  Yaw=%.2f  Roll=%.2f",
            rot.Pitch, rot.Yaw, rot.Roll))
    else
        print("[HogwartsMP] Rotation : failed")
    end

    print("[HogwartsMP] ==============================")
end

-- ClientRestart fires before pawn is assigned, so wait 1s before dumping
RegisterHook("/Script/Engine.PlayerController:ClientRestart", function(self)
    print("[HogwartsMP] ClientRestart fired — waiting for pawn...")
    ExecuteWithDelay(1000, function()
        dump_player()
    end)
end)

-- Also allow manual trigger via F6
RegisterKeyBind(Key.F6, {}, function()
    dump_player()
end)

print("[HogwartsMP] PlayerDumper loaded. F6 = manual dump.")
