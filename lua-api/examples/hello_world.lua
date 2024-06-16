print("Initializing hello_world.lua")

local api = uevr.api;
local uobjects = uevr.types.FUObjectArray.get()

print("Printing first 5 UObjects")
for i=0, 5 do
    local uobject = uobjects:get_object(i)

    if uobject ~= nil then
        print(uobject:get_full_name())
    end
end

local once = true
local last_world = nil
local last_level = nil

uevr.sdk.callbacks.on_pre_engine_tick(function(engine, delta)
    local game_engine_class = api:find_uobject("Class /Script/Engine.GameEngine")
    local game_engine = UEVR_UObjectHook.get_first_object_by_class(game_engine_class)

    local viewport = game_engine.GameViewport
    if viewport == nil then
        print("Viewport is nil")
        return
    end
    local world = viewport.World

    if world == nil then
        print("World is nil")
        return
    end

    if world ~= last_world then
        print("World changed")
    end

    last_world = world

    local level = world.PersistentLevel

    if level == nil then
        print("Level is nil")
        return
    end

    if level ~= last_level then
        print("Level changed")
        print("Level name: " .. level:get_full_name())

        local game_instance = game_engine.GameInstance

        if game_instance == nil then
            print("GameInstance is nil")
            return
        end

        local local_players = game_instance.LocalPlayers

        for i in ipairs(local_players) do
            local player = local_players[i]
            local player_controller = player.PlayerController
            local pawn = player_controller.Pawn
    
            if pawn ~= nil then
                print("Pawn: " .. pawn:get_full_name())
            end
    
            if player_controller ~= nil then
                print("PlayerController: " .. player_controller:get_full_name())
            end
        end

        print("Local players: " .. tostring(local_players))
    end

    last_level = level

    if once then
        print("executing stat fps")
        uevr.api:execute_command("stat fps")
        once = false

        print("executing stat unit")
        uevr.api:execute_command("stat unit")

        print("GameEngine class: " .. game_engine_class:get_full_name())
        print("GameEngine object: " .. game_engine:get_full_name())
    end
end)

uevr.sdk.callbacks.on_script_reset(function()
    print("Resetting hello_world.lua")
end)