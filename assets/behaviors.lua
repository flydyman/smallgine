-- Data-driven node behavior. update(dt, t) runs every frame; engine exposes
-- node_set_pos / node_get_pos / node_set_rot to drive scene nodes from script.
function update(dt, t)
    -- Figure-eight orbit for "luaNode".
    local r = 1.6
    node_set_pos("luaNode", math.sin(t) * r, 2.0 + math.sin(t * 2.0) * 0.5, math.sin(t * 2.0) * r * 0.5)
    node_set_rot("luaNode", 0.0, t * 60.0, 0.0)
end
