my_value = 0

function on_init(entity_id)
    my_value = entity_id
end

function on_update(entity_id, dt)
    -- my_value should still be what on_init set it to
    -- (not contaminated by another entity's script)
end
