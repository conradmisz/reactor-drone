#include "quadtree_strategy.hpp"
#include "collision_math.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>

QuadtreeStrategy::QuadtreeStrategy(int max_depth, int node_capacity,
                                   float world_x, float world_y,
                                   float world_width, float world_height)
    : max_depth_(std::max(0, max_depth)),
      node_capacity_(std::max(1, node_capacity)),
      world_x_(world_x), world_y_(world_y),
      world_w_(world_width), world_h_(world_height) {}

// Returns true if the entity AABB fits entirely within the node bounds
static bool fits_in_node(const QuadEntityData& ed, const QuadNode& node) {
    return ed.aabb_x >= node.x &&
           ed.aabb_y >= node.y &&
           ed.aabb_x + ed.aabb_w <= node.x + node.w &&
           ed.aabb_y + ed.aabb_h <= node.y + node.h;
}

// Subdivide a leaf node into four children
static void subdivide(QuadNode& node) {
    float hw = node.w / 2.0f;
    float hh = node.h / 2.0f;
    int child_depth = node.depth + 1;

    node.sw = std::make_unique<QuadNode>();
    node.sw->x = node.x;       node.sw->y = node.y;
    node.sw->w = hw;            node.sw->h = hh;
    node.sw->depth = child_depth;

    node.se = std::make_unique<QuadNode>();
    node.se->x = node.x + hw;  node.se->y = node.y;
    node.se->w = hw;            node.se->h = hh;
    node.se->depth = child_depth;

    node.nw = std::make_unique<QuadNode>();
    node.nw->x = node.x;       node.nw->y = node.y + hh;
    node.nw->w = hw;            node.nw->h = hh;
    node.nw->depth = child_depth;

    node.ne = std::make_unique<QuadNode>();
    node.ne->x = node.x + hw;  node.ne->y = node.y + hh;
    node.ne->w = hw;            node.ne->h = hh;
    node.ne->depth = child_depth;

    // Redistribute existing entities: those fitting in a child move down
    std::vector<QuadEntityData> remaining;
    for (auto& ed : node.entities) {
        if (fits_in_node(ed, *node.ne)) {
            node.ne->entities.push_back(ed);
        } else if (fits_in_node(ed, *node.nw)) {
            node.nw->entities.push_back(ed);
        } else if (fits_in_node(ed, *node.sw)) {
            node.sw->entities.push_back(ed);
        } else if (fits_in_node(ed, *node.se)) {
            node.se->entities.push_back(ed);
        } else {
            remaining.push_back(ed);  // Boundary entity stays in parent
        }
    }
    node.entities = std::move(remaining);
}

// Insert entity into the tree
static void insert(QuadNode& node, const QuadEntityData& ed,
                   int max_depth, int node_capacity) {
    if (node.is_subdivided()) {
        // Try to place in a child
        if (fits_in_node(ed, *node.ne)) {
            insert(*node.ne, ed, max_depth, node_capacity);
        } else if (fits_in_node(ed, *node.nw)) {
            insert(*node.nw, ed, max_depth, node_capacity);
        } else if (fits_in_node(ed, *node.sw)) {
            insert(*node.sw, ed, max_depth, node_capacity);
        } else if (fits_in_node(ed, *node.se)) {
            insert(*node.se, ed, max_depth, node_capacity);
        } else {
            node.entities.push_back(ed);  // Boundary entity stays in parent
        }
        return;
    }

    node.entities.push_back(ed);

    // Subdivide if over capacity and not at max depth
    if (static_cast<int>(node.entities.size()) > node_capacity &&
        node.depth < max_depth) {
        subdivide(node);
    }
}

// Check a pair through the layer/broad/narrow pipeline
static bool check_pair(const QuadEntityData& a, const QuadEntityData& b,
                       const ComponentStorage& storage,
                       size_t& narrow_cc, size_t& narrow_ca, size_t& narrow_aa,
                       size_t& narrow_oo, size_t& narrow_oc, size_t& narrow_oa) {
    auto col_a = storage.get_component<Collider>(a.entity);
    auto col_b = storage.get_component<Collider>(b.entity);

    if (!layers_compatible(col_a->get().layer, col_a->get().mask,
                           col_b->get().layer, col_b->get().mask)) {
        return false;
    }

    if (!aabb_overlap(a.aabb_x, a.aabb_y, a.aabb_w, a.aabb_h,
                      b.aabb_x, b.aabb_y, b.aabb_w, b.aabb_h)) {
        return false;
    }

    // Narrow phase dispatch: OBB > Circle > AABB-only
    if (a.has_obb && b.has_obb) {
        ++narrow_oo;
        auto& obb_a = storage.get_component<OBBCollider>(a.entity)->get();
        auto& obb_b = storage.get_component<OBBCollider>(b.entity)->get();
        auto& pos_a = storage.get_component<Position>(a.entity)->get();
        auto& pos_b = storage.get_component<Position>(b.entity)->get();
        auto rot_a = storage.get_component<Rotation>(a.entity);
        auto rot_b = storage.get_component<Rotation>(b.entity);
        float angle_a = rot_a ? rot_a->get().angle : 0.0f;
        float angle_b = rot_b ? rot_b->get().angle : 0.0f;
        float cx_a = pos_a.x + obb_a.half_width;
        float cy_a = pos_a.y + obb_a.half_height;
        float cx_b = pos_b.x + obb_b.half_width;
        float cy_b = pos_b.y + obb_b.half_height;
        if (!obb_obb_overlap(cx_a, cy_a, obb_a.half_width, obb_a.half_height, angle_a,
                             cx_b, cy_b, obb_b.half_width, obb_b.half_height, angle_b)) return false;
    } else if (a.has_obb || b.has_obb) {
        const QuadEntityData& obb_ed = a.has_obb ? a : b;
        const QuadEntityData& other_ed = a.has_obb ? b : a;
        bool other_has_cc = a.has_obb ? b.has_circle : a.has_circle;

        auto& obb = storage.get_component<OBBCollider>(obb_ed.entity)->get();
        auto& pos_o = storage.get_component<Position>(obb_ed.entity)->get();
        auto rot_o = storage.get_component<Rotation>(obb_ed.entity);
        float angle_o = rot_o ? rot_o->get().angle : 0.0f;
        float cx_o = pos_o.x + obb.half_width;
        float cy_o = pos_o.y + obb.half_height;

        if (other_has_cc) {
            ++narrow_oc;
            auto& cc = storage.get_component<CircleCollider>(other_ed.entity)->get();
            auto& pos_c = storage.get_component<Position>(other_ed.entity)->get();
            float ccx = pos_c.x + cc.offset_x + cc.radius;
            float ccy = pos_c.y + cc.offset_y + cc.radius;
            if (!obb_circle_overlap(cx_o, cy_o, obb.half_width, obb.half_height, angle_o,
                                    ccx, ccy, cc.radius)) return false;
        } else {
            ++narrow_oa;
            auto& pos_r = storage.get_component<Position>(other_ed.entity)->get();
            auto& col_r = storage.get_component<Collider>(other_ed.entity)->get();
            float cx_r = pos_r.x + col_r.width / 2.0f;
            float cy_r = pos_r.y + col_r.height / 2.0f;
            if (!obb_obb_overlap(cx_o, cy_o, obb.half_width, obb.half_height, angle_o,
                                 cx_r, cy_r, col_r.width / 2.0f, col_r.height / 2.0f, 0.0f)) return false;
        }
    } else if (a.has_circle && b.has_circle) {
        ++narrow_cc;
        auto& cc_a = storage.get_component<CircleCollider>(a.entity)->get();
        auto& cc_b = storage.get_component<CircleCollider>(b.entity)->get();
        auto& pos_a = storage.get_component<Position>(a.entity)->get();
        auto& pos_b = storage.get_component<Position>(b.entity)->get();
        float cx_a = pos_a.x + cc_a.offset_x + cc_a.radius;
        float cy_a = pos_a.y + cc_a.offset_y + cc_a.radius;
        float cx_b = pos_b.x + cc_b.offset_x + cc_b.radius;
        float cy_b = pos_b.y + cc_b.offset_y + cc_b.radius;
        if (!circle_circle_overlap(cx_a, cy_a, cc_a.radius,
                                   cx_b, cy_b, cc_b.radius)) return false;
    } else if (a.has_circle || b.has_circle) {
        ++narrow_ca;
        const QuadEntityData& circle_ed = a.has_circle ? a : b;
        const QuadEntityData& aabb_ed = a.has_circle ? b : a;
        auto& cc = storage.get_component<CircleCollider>(circle_ed.entity)->get();
        auto& pos_c = storage.get_component<Position>(circle_ed.entity)->get();
        auto& pos_r = storage.get_component<Position>(aabb_ed.entity)->get();
        auto& col_r = storage.get_component<Collider>(aabb_ed.entity)->get();
        float cx = pos_c.x + cc.offset_x + cc.radius;
        float cy = pos_c.y + cc.offset_y + cc.radius;
        if (!circle_aabb_overlap(cx, cy, cc.radius,
                                 pos_r.x, pos_r.y,
                                 col_r.width, col_r.height)) return false;
    } else {
        ++narrow_aa;
    }

    return true;
}

// Collect all unique candidate pairs from the quadtree
static void collect_candidate_pairs(const QuadNode& node,
                                    const std::vector<QuadEntityData>& ancestors,
                                    std::set<std::pair<Entity, Entity>>& pairs) {
    const auto& ents = node.entities;

    // Pairs within this node
    for (size_t i = 0; i < ents.size(); ++i) {
        for (size_t j = i + 1; j < ents.size(); ++j) {
            Entity lo = std::min(ents[i].entity, ents[j].entity);
            Entity hi = std::max(ents[i].entity, ents[j].entity);
            pairs.emplace(lo, hi);
        }
    }

    // Pairs between this node's entities and ancestor entities
    for (const auto& e : ents) {
        for (const auto& a : ancestors) {
            Entity lo = std::min(e.entity, a.entity);
            Entity hi = std::max(e.entity, a.entity);
            pairs.emplace(lo, hi);
        }
    }

    if (node.is_subdivided()) {
        std::vector<QuadEntityData> new_ancestors;
        new_ancestors.reserve(ancestors.size() + ents.size());
        new_ancestors.insert(new_ancestors.end(), ancestors.begin(), ancestors.end());
        new_ancestors.insert(new_ancestors.end(), ents.begin(), ents.end());

        collect_candidate_pairs(*node.ne, new_ancestors, pairs);
        collect_candidate_pairs(*node.nw, new_ancestors, pairs);
        collect_candidate_pairs(*node.sw, new_ancestors, pairs);
        collect_candidate_pairs(*node.se, new_ancestors, pairs);
    }
}

std::vector<std::pair<Entity, Entity>>
QuadtreeStrategy::detect(const ComponentStorage& storage) const {
    pair_count_ = 0;
    narrow_cc_ = 0;
    narrow_ca_ = 0;
    narrow_aa_ = 0;
    narrow_oo_ = 0;
    narrow_oc_ = 0;
    narrow_oa_ = 0;

    // 1. Gather entities with both Position and Collider, precompute AABBs
    auto pos_entities = storage.entities_with_component<Position>();
    std::vector<QuadEntityData> candidates;
    for (Entity e : pos_entities) {
        if (!storage.has_component<Collider>(e)) continue;

        QuadEntityData ed;
        ed.entity = e;
        ed.has_obb = storage.has_component<OBBCollider>(e);
        ed.has_circle = storage.has_component<CircleCollider>(e);

        auto& pos = storage.get_component<Position>(e)->get();
        if (ed.has_obb) {
            auto& obb = storage.get_component<OBBCollider>(e)->get();
            float center_x = pos.x + obb.half_width;
            float center_y = pos.y + obb.half_height;
            float extent = std::sqrt(obb.half_width * obb.half_width + obb.half_height * obb.half_height);
            ed.aabb_x = center_x - extent;
            ed.aabb_y = center_y - extent;
            ed.aabb_w = 2.0f * extent;
            ed.aabb_h = 2.0f * extent;
        } else if (ed.has_circle) {
            auto& cc = storage.get_component<CircleCollider>(e)->get();
            ed.aabb_x = pos.x + cc.offset_x;
            ed.aabb_y = pos.y + cc.offset_y;
            ed.aabb_w = 2.0f * cc.radius;
            ed.aabb_h = 2.0f * cc.radius;
        } else {
            auto& col = storage.get_component<Collider>(e)->get();
            ed.aabb_x = pos.x;
            ed.aabb_y = pos.y;
            ed.aabb_w = col.width;
            ed.aabb_h = col.height;
        }
        candidates.push_back(ed);
    }

    if (candidates.size() < 2) {
        return {};
    }

    // 2. Build quadtree
    QuadNode root;
    root.x = world_x_;
    root.y = world_y_;
    root.w = world_w_;
    root.h = world_h_;
    root.depth = 0;

    for (const auto& ed : candidates) {
        insert(root, ed, max_depth_, node_capacity_);
    }

    // 3. Collect unique candidate pairs from tree traversal
    std::set<std::pair<Entity, Entity>> candidate_pairs;
    std::vector<QuadEntityData> empty_ancestors;
    collect_candidate_pairs(root, empty_ancestors, candidate_pairs);

    pair_count_ = candidate_pairs.size();

    // 4. Build entity lookup map for fast access during pair checking
    std::unordered_map<Entity, const QuadEntityData*> entity_map;
    for (const auto& ed : candidates) {
        entity_map[ed.entity] = &ed;
    }

    // 5. Check each candidate pair through the collision pipeline
    std::vector<std::pair<Entity, Entity>> results;
    for (const auto& [lo, hi] : candidate_pairs) {
        const QuadEntityData& a = *entity_map[lo];
        const QuadEntityData& b = *entity_map[hi];

        if (check_pair(a, b, storage, narrow_cc_, narrow_ca_, narrow_aa_,
                      narrow_oo_, narrow_oc_, narrow_oa_)) {
            results.emplace_back(lo, hi);
        }
    }

    return results;
}

size_t QuadtreeStrategy::last_pair_count() const { return pair_count_; }
size_t QuadtreeStrategy::last_narrow_cc() const { return narrow_cc_; }
size_t QuadtreeStrategy::last_narrow_ca() const { return narrow_ca_; }
size_t QuadtreeStrategy::last_narrow_aa() const { return narrow_aa_; }
size_t QuadtreeStrategy::last_narrow_oo() const { return narrow_oo_; }
size_t QuadtreeStrategy::last_narrow_oc() const { return narrow_oc_; }
size_t QuadtreeStrategy::last_narrow_oa() const { return narrow_oa_; }
