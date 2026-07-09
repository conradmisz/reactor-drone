#include "uniform_grid_strategy.hpp"
#include "collision_math.hpp"
#include <cmath>
#include <set>
#include <unordered_map>
#include <algorithm>

UniformGridStrategy::UniformGridStrategy(int cell_size, float world_x, float world_y,
                                         float world_width, float world_height)
    : cell_size_(cell_size > 0 ? cell_size : 256),
      world_x_(world_x), world_y_(world_y),
      world_width_(world_width), world_height_(world_height) {
    num_cols_ = std::max(1, static_cast<int>(std::ceil(world_width_ / static_cast<float>(cell_size_))));
    num_rows_ = std::max(1, static_cast<int>(std::ceil(world_height_ / static_cast<float>(cell_size_))));
}

std::vector<std::pair<Entity, Entity>>
UniformGridStrategy::detect(const ComponentStorage& storage) const {
    pair_count_ = 0;
    narrow_cc_ = 0;
    narrow_ca_ = 0;
    narrow_aa_ = 0;
    narrow_oo_ = 0;
    narrow_oc_ = 0;
    narrow_oa_ = 0;

    // 1. Gather entities with both Position and Collider
    auto pos_entities = storage.entities_with_component<Position>();
    std::vector<Entity> candidates;
    for (Entity e : pos_entities) {
        if (storage.has_component<Collider>(e)) {
            candidates.push_back(e);
        }
    }

    if (candidates.size() < 2) {
        return {};
    }

    // 2. Build spatial hash — OBB > Circle > AABB-only for cell assignment
    std::unordered_map<int, std::vector<Entity>> grid;
    float cell_f = static_cast<float>(cell_size_);

    for (Entity e : candidates) {
        auto pos = storage.get_component<Position>(e);
        bool has_obb = storage.has_component<OBBCollider>(e);
        bool has_cc = storage.has_component<CircleCollider>(e);

        float left, bottom, right, top;
        if (has_obb) {
            auto& obb = storage.get_component<OBBCollider>(e)->get();
            float center_x = pos->get().x + obb.half_width;
            float center_y = pos->get().y + obb.half_height;
            float extent = std::sqrt(obb.half_width * obb.half_width + obb.half_height * obb.half_height);
            left   = center_x - extent;
            bottom = center_y - extent;
            right  = center_x + extent;
            top    = center_y + extent;
        } else if (has_cc) {
            auto& cc = storage.get_component<CircleCollider>(e)->get();
            left   = pos->get().x + cc.offset_x;
            bottom = pos->get().y + cc.offset_y;
            right  = left + 2.0f * cc.radius;
            top    = bottom + 2.0f * cc.radius;
        } else {
            auto col = storage.get_component<Collider>(e);
            left   = pos->get().x;
            bottom = pos->get().y;
            right  = pos->get().x + col->get().width;
            top    = pos->get().y + col->get().height;
        }

        int min_col = static_cast<int>(std::floor((left   - world_x_) / cell_f));
        int max_col = static_cast<int>(std::floor((right  - world_x_) / cell_f));
        int min_row = static_cast<int>(std::floor((bottom - world_y_) / cell_f));
        int max_row = static_cast<int>(std::floor((top    - world_y_) / cell_f));

        // Clamp to grid bounds
        min_col = std::max(0, std::min(min_col, num_cols_ - 1));
        max_col = std::max(0, std::min(max_col, num_cols_ - 1));
        min_row = std::max(0, std::min(min_row, num_rows_ - 1));
        max_row = std::max(0, std::min(max_row, num_rows_ - 1));

        for (int r = min_row; r <= max_row; ++r) {
            for (int c = min_col; c <= max_col; ++c) {
                int index = r * num_cols_ + c;
                grid[index].push_back(e);
            }
        }
    }

    // 3. Check pairs within each cell, deduplicate
    std::set<std::pair<Entity, Entity>> unique_pairs;
    std::set<std::pair<Entity, Entity>> counted_pairs;

    for (auto& [cell_index, entities] : grid) {
        if (entities.size() < 2) continue;

        for (size_t i = 0; i < entities.size(); ++i) {
            for (size_t j = i + 1; j < entities.size(); ++j) {
                Entity a = entities[i];
                Entity b = entities[j];

                Entity lo = std::min(a, b);
                Entity hi = std::max(a, b);
                auto pair_key = std::make_pair(lo, hi);

                if (counted_pairs.insert(pair_key).second) {
                    ++pair_count_;
                } else {
                    continue;
                }

                auto col_a = storage.get_component<Collider>(a);
                auto col_b = storage.get_component<Collider>(b);

                if (!layers_compatible(col_a->get().layer, col_a->get().mask,
                                       col_b->get().layer, col_b->get().mask)) {
                    continue;
                }

                auto pos_a = storage.get_component<Position>(a);
                auto pos_b = storage.get_component<Position>(b);

                // Determine shape type: OBB > Circle > AABB-only
                bool has_obb_a = storage.has_component<OBBCollider>(a);
                bool has_obb_b = storage.has_component<OBBCollider>(b);
                bool has_cc_a = storage.has_component<CircleCollider>(a);
                bool has_cc_b = storage.has_component<CircleCollider>(b);

                // Compute broad phase AABBs
                float bx_a, by_a, bw_a, bh_a;
                if (has_obb_a) {
                    auto& obb = storage.get_component<OBBCollider>(a)->get();
                    float center_x = pos_a->get().x + obb.half_width;
                    float center_y = pos_a->get().y + obb.half_height;
                    float extent = std::sqrt(obb.half_width * obb.half_width + obb.half_height * obb.half_height);
                    bx_a = center_x - extent;
                    by_a = center_y - extent;
                    bw_a = 2.0f * extent;
                    bh_a = 2.0f * extent;
                } else if (has_cc_a) {
                    auto& cc = storage.get_component<CircleCollider>(a)->get();
                    bx_a = pos_a->get().x + cc.offset_x;
                    by_a = pos_a->get().y + cc.offset_y;
                    bw_a = 2.0f * cc.radius;
                    bh_a = 2.0f * cc.radius;
                } else {
                    bx_a = pos_a->get().x;
                    by_a = pos_a->get().y;
                    bw_a = col_a->get().width;
                    bh_a = col_a->get().height;
                }

                float bx_b, by_b, bw_b, bh_b;
                if (has_obb_b) {
                    auto& obb = storage.get_component<OBBCollider>(b)->get();
                    float center_x = pos_b->get().x + obb.half_width;
                    float center_y = pos_b->get().y + obb.half_height;
                    float extent = std::sqrt(obb.half_width * obb.half_width + obb.half_height * obb.half_height);
                    bx_b = center_x - extent;
                    by_b = center_y - extent;
                    bw_b = 2.0f * extent;
                    bh_b = 2.0f * extent;
                } else if (has_cc_b) {
                    auto& cc = storage.get_component<CircleCollider>(b)->get();
                    bx_b = pos_b->get().x + cc.offset_x;
                    by_b = pos_b->get().y + cc.offset_y;
                    bw_b = 2.0f * cc.radius;
                    bh_b = 2.0f * cc.radius;
                } else {
                    bx_b = pos_b->get().x;
                    by_b = pos_b->get().y;
                    bw_b = col_b->get().width;
                    bh_b = col_b->get().height;
                }

                if (!aabb_overlap(bx_a, by_a, bw_a, bh_a,
                                  bx_b, by_b, bw_b, bh_b)) {
                    continue;
                }

                // Narrow phase dispatch: OBB > Circle > AABB-only
                if (has_obb_a && has_obb_b) {
                    ++narrow_oo_;
                    auto& obb_a = storage.get_component<OBBCollider>(a)->get();
                    auto& obb_b = storage.get_component<OBBCollider>(b)->get();
                    auto rot_a_opt = storage.get_component<Rotation>(a);
                    auto rot_b_opt = storage.get_component<Rotation>(b);
                    float angle_a = rot_a_opt ? rot_a_opt->get().angle : 0.0f;
                    float angle_b = rot_b_opt ? rot_b_opt->get().angle : 0.0f;
                    float cx_a = pos_a->get().x + obb_a.half_width;
                    float cy_a = pos_a->get().y + obb_a.half_height;
                    float cx_b = pos_b->get().x + obb_b.half_width;
                    float cy_b = pos_b->get().y + obb_b.half_height;
                    if (!obb_obb_overlap(cx_a, cy_a, obb_a.half_width, obb_a.half_height, angle_a,
                                         cx_b, cy_b, obb_b.half_width, obb_b.half_height, angle_b)) continue;
                } else if (has_obb_a || has_obb_b) {
                    Entity obb_e = has_obb_a ? a : b;
                    Entity other_e = has_obb_a ? b : a;
                    bool other_has_cc = has_obb_a ? has_cc_b : has_cc_a;

                    auto& obb = storage.get_component<OBBCollider>(obb_e)->get();
                    auto& pos_obb = storage.get_component<Position>(obb_e)->get();
                    auto rot_opt = storage.get_component<Rotation>(obb_e);
                    float obb_angle = rot_opt ? rot_opt->get().angle : 0.0f;
                    float obb_cx = pos_obb.x + obb.half_width;
                    float obb_cy = pos_obb.y + obb.half_height;

                    if (other_has_cc) {
                        ++narrow_oc_;
                        auto& cc = storage.get_component<CircleCollider>(other_e)->get();
                        auto& pos_c = storage.get_component<Position>(other_e)->get();
                        float circle_cx = pos_c.x + cc.offset_x + cc.radius;
                        float circle_cy = pos_c.y + cc.offset_y + cc.radius;
                        if (!obb_circle_overlap(obb_cx, obb_cy, obb.half_width, obb.half_height, obb_angle,
                                                circle_cx, circle_cy, cc.radius)) continue;
                    } else {
                        ++narrow_oa_;
                        auto& pos_r = storage.get_component<Position>(other_e)->get();
                        auto& col_r = storage.get_component<Collider>(other_e)->get();
                        float aabb_cx = pos_r.x + col_r.width / 2.0f;
                        float aabb_cy = pos_r.y + col_r.height / 2.0f;
                        if (!obb_obb_overlap(obb_cx, obb_cy, obb.half_width, obb.half_height, obb_angle,
                                             aabb_cx, aabb_cy, col_r.width / 2.0f, col_r.height / 2.0f, 0.0f)) continue;
                    }
                } else if (has_cc_a && has_cc_b) {
                    ++narrow_cc_;
                    auto& cc_a = storage.get_component<CircleCollider>(a)->get();
                    auto& cc_b = storage.get_component<CircleCollider>(b)->get();
                    float cx_a = pos_a->get().x + cc_a.offset_x + cc_a.radius;
                    float cy_a = pos_a->get().y + cc_a.offset_y + cc_a.radius;
                    float cx_b = pos_b->get().x + cc_b.offset_x + cc_b.radius;
                    float cy_b = pos_b->get().y + cc_b.offset_y + cc_b.radius;
                    if (!circle_circle_overlap(cx_a, cy_a, cc_a.radius, cx_b, cy_b, cc_b.radius)) continue;
                } else if (has_cc_a || has_cc_b) {
                    ++narrow_ca_;
                    Entity circle_e = has_cc_a ? a : b;
                    Entity aabb_e = has_cc_a ? b : a;
                    auto& cc = storage.get_component<CircleCollider>(circle_e)->get();
                    auto& pos_c = storage.get_component<Position>(circle_e)->get();
                    auto& pos_r = storage.get_component<Position>(aabb_e)->get();
                    auto& col_r = storage.get_component<Collider>(aabb_e)->get();
                    float cx = pos_c.x + cc.offset_x + cc.radius;
                    float cy = pos_c.y + cc.offset_y + cc.radius;
                    if (!circle_aabb_overlap(cx, cy, cc.radius, pos_r.x, pos_r.y, col_r.width, col_r.height)) continue;
                } else {
                    ++narrow_aa_;
                }

                unique_pairs.emplace(lo, hi);
            }
        }
    }

    return {unique_pairs.begin(), unique_pairs.end()};
}

size_t UniformGridStrategy::last_pair_count() const {
    return pair_count_;
}

size_t UniformGridStrategy::last_narrow_cc() const {
    return narrow_cc_;
}

size_t UniformGridStrategy::last_narrow_ca() const {
    return narrow_ca_;
}

size_t UniformGridStrategy::last_narrow_aa() const {
    return narrow_aa_;
}

size_t UniformGridStrategy::last_narrow_oo() const {
    return narrow_oo_;
}

size_t UniformGridStrategy::last_narrow_oc() const {
    return narrow_oc_;
}

size_t UniformGridStrategy::last_narrow_oa() const {
    return narrow_oa_;
}
