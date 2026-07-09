init_count = 0
update_count = 0
collision_count = 0
destroy_count = 0
last_entity_id = 0
last_dt = 0
last_other_id = 0

function on_init(entity_id)
    init_count = init_count + 1
    last_entity_id = entity_id
end

function on_update(entity_id, dt)
    update_count = update_count + 1
    last_entity_id = entity_id
    last_dt = dt
end

function on_collision(entity_id, other_id)
    collision_count = collision_count + 1
    last_entity_id = entity_id
    last_other_id = other_id
end

function on_destroy(entity_id)
    destroy_count = destroy_count + 1
    last_entity_id = entity_id
end
