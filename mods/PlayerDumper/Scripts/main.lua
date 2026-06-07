-- PlayerDumper: reads player position and forwards it to the server via HogwartsMPNet.dll
-- F6 = manual state dump | F7 = connect to server | F8 = disconnect

local SERVER_IP   = "127.0.0.1"
local SERVER_PORT = 7777
local PLAYER_ID   = 1
local SEND_RATE   = 0.1  -- seconds between position updates

local net = nil
local connected = false
local last_send = 0

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
    print("[HogwartsMP] Disconnected.")
end

-- Send loop: fires every SEND_RATE ms via recursive ExecuteWithDelay.
-- Only started after ClientRestart so the game world exists.
local loop_started = false
local function send_loop()
    if connected and net then
        net.poll()
        local pawn = get_pawn()
        if pawn then
            local lok, loc = pcall(function() return pawn:K2_GetActorLocation() end)
            local rok, rot = pcall(function() return pawn:K2_GetActorRotation() end)
            if lok and loc and rok and rot then
                net.send_position(PLAYER_ID, loc.X, loc.Y, loc.Z, rot.Yaw)
            end
        end
    end
    ExecuteWithDelay(math.floor(SEND_RATE * 1000), send_loop)
end

-- ClientRestart fires before pawn is assigned
RegisterHook("/Script/Engine.PlayerController:ClientRestart", function(self)
    print("[HogwartsMP] ClientRestart fired — waiting for pawn...")
    ExecuteWithDelay(1000, function()
        dump_player()
        -- Start the send loop once the world is ready
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
