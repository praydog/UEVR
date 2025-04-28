local test_table = {
    name = "test",
    version = 1,
    description = "test",
    author = "test",
    url = "",
    test = 0.5,
}

json.dump_file("test.json", test_table, 4)

local re_read_table = json.load_file("test.json")

assert(re_read_table.name == test_table.name, "Name is not the same")
assert(re_read_table.version == test_table.version, "Version is not the same")
assert(re_read_table.description == test_table.description, "Description is not the same")
assert(re_read_table.author == test_table.author, "Author is not the same")
assert(re_read_table.url == test_table.url, "URL is not the same")
assert(re_read_table.test == test_table.test, "Test is not the same")

-- Write a random file using FS api
local file = io.open("test.txt", "w")
file:write("Hello, world!")
file:close()

local json_files = fs.glob([[.*json]])

uevr.sdk.callbacks.on_draw_ui(function()
    imgui.text("Hello, world!")

    for _, file in ipairs(json_files) do
        imgui.text(file)
    end

    -- Some levers the user can mess around with and save to disk
    local changed, new_value = imgui.slider_float("Test", test_table.test, 0, 1)

    if changed then
        test_table.test = new_value
        json.dump_file("test.json", test_table, 4)
    end
end)