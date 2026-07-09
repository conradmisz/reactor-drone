init_count = 0
update_count = 0

function on_init(entity_id)
    init_count = init_count + 1
end

function on_update(entity_id, dt)
    update_count = update_count + 1
end
