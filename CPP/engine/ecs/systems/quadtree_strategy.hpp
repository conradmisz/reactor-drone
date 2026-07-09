#ifndef QUADTREE_STRATEGY_HPP
#define QUADTREE_STRATEGY_HPP

#include "collision_strategy.hpp"
#include <memory>
#include <vector>

/**
 * Precomputed entity data for quadtree insertion and collision checking.
 * Avoids repeated component lookups during tree traversal.
 */
struct QuadEntityData {
    Entity entity;
    float aabb_x, aabb_y, aabb_w, aabb_h;  // Broad phase AABB
    bool has_circle;                         // Has CircleCollider component
    bool has_obb;                            // Has OBBCollider component
};

/**
 * A node in the quadtree. Covers a rectangular region and holds entities
 * whose AABBs fit within (or span) its bounds. Subdivides into four
 * children when entity count exceeds capacity and depth < max_depth.
 */
struct QuadNode {
    float x, y, w, h;          // Node bounds (bottom-left corner + dimensions)
    int depth;                  // Depth in tree (root = 0)
    std::vector<QuadEntityData> entities;
    std::unique_ptr<QuadNode> ne, nw, sw, se;  // Children (null if leaf)

    bool is_subdivided() const { return ne != nullptr; }
};

/**
 * Quadtree spatial partitioning collision strategy.
 *
 * Adaptively subdivides the world based on entity density. The root node
 * covers the full world bounds. Nodes subdivide into four quadrants (NE,
 * NW, SW, SE) when their entity count exceeds node_capacity, up to
 * max_depth. Entities whose AABBs span a subdivision boundary remain in
 * the parent node. The tree is rebuilt from scratch every detect() call.
 *
 * Produces the identical collision pair set as BruteForceStrategy.
 */
class QuadtreeStrategy : public CollisionStrategy {
public:
    QuadtreeStrategy(int max_depth, int node_capacity,
                     float world_x, float world_y,
                     float world_width, float world_height);

    std::vector<std::pair<Entity, Entity>>
    detect(const ComponentStorage& storage) const override;

    size_t last_pair_count() const override;
    size_t last_narrow_cc() const override;
    size_t last_narrow_ca() const override;
    size_t last_narrow_aa() const override;

    size_t last_narrow_oo() const override;
    size_t last_narrow_oc() const override;
    size_t last_narrow_oa() const override;

private:
    int max_depth_;
    int node_capacity_;
    float world_x_, world_y_, world_w_, world_h_;
    mutable size_t pair_count_ = 0;
    mutable size_t narrow_cc_ = 0, narrow_ca_ = 0, narrow_aa_ = 0;
    mutable size_t narrow_oo_ = 0, narrow_oc_ = 0, narrow_oa_ = 0;
};

#endif // QUADTREE_STRATEGY_HPP
