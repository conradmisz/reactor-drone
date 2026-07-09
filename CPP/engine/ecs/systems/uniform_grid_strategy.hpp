#ifndef UNIFORM_GRID_STRATEGY_HPP
#define UNIFORM_GRID_STRATEGY_HPP

#include "collision_strategy.hpp"

/**
 * Uniform grid spatial partitioning collision strategy.
 *
 * Divides the world into equal-sized cells and only checks collisions
 * between entities sharing a cell. Entities whose AABBs span multiple
 * cells are inserted into all overlapping cells. Pair deduplication
 * ensures no double-counting across shared cells.
 *
 * Produces the identical collision pair set as BruteForceStrategy,
 * but with fewer pairwise comparisons when entities are spread out.
 */
class UniformGridStrategy : public CollisionStrategy {
public:
    /**
     * Construct with cell size and world bounds.
     *
     * @param cell_size    Width and height of each grid cell (defaults to 256 if <= 0)
     * @param world_x      Left edge of the world (e.g. -400)
     * @param world_y      Bottom edge of the world (e.g. -300)
     * @param world_width  Total world width (e.g. 800)
     * @param world_height Total world height (e.g. 600)
     */
    UniformGridStrategy(int cell_size, float world_x, float world_y,
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
    int cell_size_;
    float world_x_, world_y_;
    float world_width_, world_height_;
    int num_cols_, num_rows_;
    mutable size_t pair_count_ = 0;
    mutable size_t narrow_cc_ = 0, narrow_ca_ = 0, narrow_aa_ = 0;
    mutable size_t narrow_oo_ = 0, narrow_oc_ = 0, narrow_oa_ = 0;
};

#endif // UNIFORM_GRID_STRATEGY_HPP
