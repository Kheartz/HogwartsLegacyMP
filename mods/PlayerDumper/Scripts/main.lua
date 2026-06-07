-- PlayerDumper: reads player position and forwards it to the server via HogwartsMPNet.dll
-- F6 = manual state dump | F7 = connect to server | F8 = disconnect

local SERVER_IP   = "127.0.0.1"
local SERVER_PORT = 7777
local PLAYER_ID   = 1   -- change to 2 on the second client for local two-client testing
local SEND_RATE   = 0.1 -- seconds between position updates

local UEHelpers = require("UEHelpers")

local net       = nil
local connected = false

-- Load the native networking module
local function load_net()
    local dll_path = [[D:\Program Files (x86)\Steam\steamapps\common\Hogwarts Legacy\Phoenix\Binaries\Win64\HogwartsMPNet.dll]]
    local ok, result = pcall(function()
        return package.loadlib(dll_path, "luaopen_HogwartsMPNet")()
    end)
    if ok then
        print("[HogwartsMP] HogwartsMPNet.dll loaded.")
        return result
    else
        print("[HogwartsMP] Failed to load HogwartsMPNet.dll: " .. tostring(result))
        return nil
    end
end

local function get_pawn()
    local pc = FindFirstOf("PlayerController")
    if not pc or not pc:IsValid() then return nil end

    local pawn = nil
    local ok = pcall(function() pawn = pc:GetPawn() end)
    if not ok or not pawn or not pawn:IsValid() then
        pawn = FindFirstOf("BP_Biped_Player_C")
    end
    if not pawn or not pawn:IsValid() then return nil end
    return pawn
end

local function dump_player()
    print("[HogwartsMP] === Player State Dump ===")
    local pawn = get_pawn()
    if not pawn then
        print("[HogwartsMP] No pawn found.")
        return
    end

    print(string.format("[HogwartsMP] Pawn: %s", pawn:GetClass():GetFName():ToString()))

    local lok, loc = pcall(function() return pawn:K2_GetActorLocation() end)
    local rok, rot = pcall(function() return pawn:K2_GetActorRotation() end)
    if lok and loc then
        print(string.format("[HogwartsMP] Location : X=%.2f  Y=%.2f  Z=%.2f", loc.X, loc.Y, loc.Z))
    end
    if rok and rot then
        print(string.format("[HogwartsMP] Rotation : Pitch=%.2f  Yaw=%.2f  Roll=%.2f", rot.Pitch, rot.Yaw, rot.Roll))
    end
    print("[HogwartsMP] ==============================")
end

-- ---------------------------------------------------------------------------
-- Remote player proxy management
-- Proxies are StaticMeshActors spawned to represent other players visually.
-- proxies[id] = actor  (valid proxy)
--             = false  (spawn previously failed — don't retry)
-- ---------------------------------------------------------------------------

local proxies        = {}   -- id -> actor, or false if spawn previously failed
local remote_targets = {}   -- id -> {x, y, z, yaw} last received position
local log_ticks      = {}   -- id -> tick count for 1-Hz logging

local GS        = nil  -- UGameplayStatics CDO
local SMA_Class = nil  -- StaticMeshActor class

local function ensure_spawn_deps()
    if not GS or not GS:IsValid() then
        GS = StaticFindObject("/Script/Engine.Default__GameplayStatics")
    end
    if not SMA_Class or not SMA_Class:IsValid() then
        SMA_Class = StaticFindObject("/Script/Engine.StaticMeshActor")
    end
    return GS and GS:IsValid() and SMA_Class and SMA_Class:IsValid()
end

local function spawn_proxy(id, x, y, z, yaw)
    if not ensure_spawn_deps() then
        print(string.format("[HogwartsMP] Cannot spawn proxy for P%d: deps unavailable", id))
        return false
    end

    local world = UEHelpers.GetWorld()
    if not world or not world:IsValid() then return false end

    -- FTransform: Rotation as quaternion, Translation, Scale3D
    local half_yaw = yaw * math.pi / 360.0
    local transform = {
        Rotation    = {X=0.0, Y=0.0, Z=math.sin(half_yaw), W=math.cos(half_yaw)},
        Translation = {X=x, Y=y, Z=z},
        Scale3D     = {X=1.0, Y=1.0, Z=1.0}
    }

    local ok, actor = pcall(function()
        return GS:BeginSpawningActorFromClass(world, SMA_Class, transform, false, nil)
    end)

    if ok and actor and actor:IsValid() then
        pcall(function() GS:FinishSpawningActor(actor, transform) end)
        print(string.format("[HogwartsMP] Spawned proxy for P%d at %.0f,%.0f,%.0f", id, x, y, z))
        return actor
    end

    print(string.format("[HogwartsMP] Proxy spawn failed for P%d", id))
    return false
end

local function update_proxy(actor, x, y, z, yaw)
    pcall(function()
        actor:K2_SetActorLocation({X=x, Y=y, Z=z}, false, {}, false)
        actor:K2_SetActorRotation({Pitch=0.0, Yaw=yaw, Roll=0.0}, false)
    end)
end

local function cleanup_proxies()
    for id, actor in pairs(proxies) do
        if actor and actor ~= false and actor:IsValid() then
            pcall(function() actor:K2_DestroyActor() end)
        end
    end
    proxies        = {}
    remote_targets = {}
end

local function on_remote_move(id, x, y, z, yaw)
    -- Proxy actor spawning is pending a working UE4SS spawn API.
    -- Log at 1Hz (every 10 ticks) so we can confirm the broadcast pipeline works.
    remote_targets[id] = {x=x, y=y, z=z, yaw=yaw}
    local ticks = (proxies[id] or 0) + 1
    proxies[id] = ticks
    if ticks == 1 or ticks % 10 == 0 then
        print(string.format("[HogwartsMP] Remote P%d  X=%.0f Y=%.0f Z=%.0f Yaw=%.1f",
            id, x, y, z, yaw))
    end
end

-- ---------------------------------------------------------------------------
-- Connection
-- ---------------------------------------------------------------------------

local function connect()
    if not net then net = load_net() end
    if not net then return end

    print(string.format("[HogwartsMP] Connecting to %s:%d...", SERVER_IP, SERVER_PORT))
    local ok, err = net.connect(SERVER_IP, SERVER_PORT)
    if ok then
        connected = true
        print("[HogwartsMP] Connected!")
    else
        print("[HogwartsMP] Connection failed: " .. tostring(err))
    end
end

local function disconnect()
    if net then net.disconnect() end
    connected = false
    proxies = {}
    remote_targets = {}
    print("[HogwartsMP] Disconnected.")
end

-- ---------------------------------------------------------------------------
-- Send loop: runs at SEND_RATE via recursive ExecuteWithDelay.
-- Only started after ClientRestart so the game world exists.
-- ---------------------------------------------------------------------------

local loop_started = false

local function send_loop()
    -- Top-level pcall so any error logs and the loop keeps rescheduling.
    local ok, err = pcall(function()
        if connected and net then
            local events = net.poll() or {}
            for _, evt in ipairs(events) do
                if evt.type == "warp" then
                    local pawn = get_pawn()
                    if pawn then
                        local wok, werr = pcall(function()
                            pawn:K2_SetActorLocation({X=evt.x, Y=evt.y, Z=evt.z}, false, {}, false)
                        end)
                        if wok then
                            print(string.format("[HogwartsMP] Warped to %.0f, %.0f, %.0f", evt.x, evt.y, evt.z))
                        else
                            print("[HogwartsMP] Warp failed: " .. tostring(werr))
                        end
                    end
                elseif evt.player_id and evt.player_id ~= PLAYER_ID then
                    local rok, rerr = pcall(on_remote_move,
                        evt.player_id, evt.x, evt.y, evt.z, evt.yaw)
                    if not rok then
                        print("[HogwartsMP] remote move error: " .. tostring(rerr))
                    end
                end
            end

            local pawn = get_pawn()
            if pawn then
                local lok, loc = pcall(function() return pawn:K2_GetActorLocation() end)
                local rok, rot = pcall(function() return pawn:K2_GetActorRotation() end)
                if lok and loc and rok and rot then
                    net.send_position(PLAYER_ID, loc.X, loc.Y, loc.Z, rot.Yaw)
                end
            end
        end
    end)
    if not ok then
        print("[HogwartsMP] send_loop error: " .. tostring(err))
    end
    ExecuteWithDelay(math.floor(SEND_RATE * 1000), send_loop)
end

-- ClientRestart fires before pawn is assigned
RegisterHook("/Script/Engine.PlayerController:ClientRestart", function(self)
    print("[HogwartsMP] ClientRestart fired — waiting for pawn...")
    ExecuteWithDelay(1000, function()
        dump_player()
        if not loop_started then
            loop_started = true
            send_loop()
            print("[HogwartsMP] Send loop started.")
        end
    end)
end)

RegisterKeyBind(Key.F6, {}, function() dump_player() end)
RegisterKeyBind(Key.F7, {}, function() connect() end)
RegisterKeyBind(Key.F8, {}, function() disconnect() end)

print("[HogwartsMP] Loaded. F6=dump  F7=connect  F8=disconnect")
