print("Initializing hello_world.lua")

UEVR_UObjectHook.activate()

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

uevr.sdk.callbacks.on_post_engine_tick(function(engine, delta)

end)

local spawn_once = true

uevr.sdk.callbacks.on_pre_engine_tick(function(engine, delta)
    --[[if spawn_once then
        local cheat_manager_c = api:find_uobject("Class /Script/Engine.CheatManager")
        local cheat_manager = UEVR_UObjectHook.get_first_object_by_class(cheat_manager_c)

        print(tostring(cheat_manager_c))

        cheat_manager:Summon("Something_C")

        spawn_once = false
    end]]

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
                --pawn.BaseEyeHeight = 0.0
                --pawn.bActorEnableCollision = not pawn.bActorEnableCollision

                local actor_component_c = api:find_uobject("Class /Script/Engine.ActorComponent");
                print("actor_component_c class: " .. tostring(actor_component_c))
                local test_component = pawn:GetComponentByClass(actor_component_c)

                print("TestComponent: " .. tostring(test_component))

                local controller = pawn.Controller

                if controller ~= nil then
                    print("Controller: " .. controller:get_full_name())

                    local velocity = controller:GetVelocity()
                    print("Velocity: " .. tostring(velocity.x) .. ", " .. tostring(velocity.y) .. ", " .. tostring(velocity.z))

                    local test = Vector3d.new(1.337, 1.0, 1.0)
                    print("Test: " .. tostring(test.x) .. ", " .. tostring(test.y) .. ", " .. tostring(test.z))

                    controller:SetActorScale3D(Vector3d.new(1.337, 1.0, 1.0))

                    local actor_scale_3d = controller:GetActorScale3D()
                    print("ActorScale3D: " .. tostring(actor_scale_3d.x) .. ", " .. tostring(actor_scale_3d.y) .. ", " .. tostring(actor_scale_3d.z))

                    
                    local control_rotation = controller:GetControlRotation()

                    print("ControlRotation: " .. tostring(control_rotation.Pitch) .. ", " .. tostring(control_rotation.Yaw) .. ", " .. tostring(control_rotation.Roll))
                    control_rotation.Pitch = 1.337

                    controller:SetControlRotation(control_rotation)
                    control_rotation = controller:GetControlRotation()

                    print("New ControlRotation: " .. tostring(control_rotation.Pitch) .. ", " .. tostring(control_rotation.Yaw) .. ", " .. tostring(control_rotation.Roll))
                end

                local primary_actor_tick = pawn.PrimaryActorTick

                if primary_actor_tick ~= nil then
                    print("PrimaryActorTick: " .. tostring(primary_actor_tick))

                    -- Print various properties, this is testing of StructProperty as PrimaryActorTick is a struct
                    local tick_interval = primary_actor_tick.TickInterval
                    print("TickInterval: " .. tostring(tick_interval))

                    print("bAllowTickOnDedicatedServer: " .. tostring(primary_actor_tick.bAllowTickOnDedicatedServer))
                    print("bCanEverTick: " .. tostring(primary_actor_tick.bCanEverTick))
                    print("bStartWithTickEnabled: " .. tostring(primary_actor_tick.bStartWithTickEnabled))
                    print("bTickEvenWhenPaused: " .. tostring(primary_actor_tick.bTickEvenWhenPaused))
                else
                    print("PrimaryActorTick is nil")
                end

                local control_input_vector = pawn.ControlInputVector
                pawn.ControlInputVector.x = 1.337

                print("ControlInputVector: " .. tostring(control_input_vector.x) .. ", " .. tostring(control_input_vector.y) .. ", " .. tostring(control_input_vector.z))

                local is_actor_tick_enabled = pawn:IsActorTickEnabled()
                print("IsActorTickEnabled: " .. tostring(is_actor_tick_enabled))

                pawn:SetActorTickEnabled(not is_actor_tick_enabled)
                is_actor_tick_enabled = pawn:IsActorTickEnabled()
                print("New IsActorTickEnabled: " .. tostring(is_actor_tick_enabled))

                pawn:SetActorTickEnabled(not is_actor_tick_enabled) -- resets it back to default

                local life_span = pawn:GetLifeSpan()
                local og_life_span = life_span
                print("LifeSpan: " .. tostring(life_span))

                pawn:SetLifeSpan(10.0)
                life_span = pawn:GetLifeSpan()

                print("New LifeSpan: " .. tostring(life_span))
                pawn:SetLifeSpan(og_life_span) -- resets it back to default

                local net_driver_name = pawn.NetDriverName:to_string()

                print("NetDriverName: " .. net_driver_name)
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