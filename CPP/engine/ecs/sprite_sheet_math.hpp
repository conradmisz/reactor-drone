#ifndef SPRITE_SHEET_MATH_HPP
#define SPRITE_SHEET_MATH_HPP

#include <SDL3/SDL.h>

/**
 * Compute the source rectangle for a given frame index in a sprite sheet atlas.
 *
 * Frames are indexed left-to-right, top-to-bottom starting from 0.
 * Column = frame_index % columns, Row = frame_index / columns.
 *
 * @param frame_index  Zero-based frame index (0 to total_frames - 1)
 * @param columns      Number of columns in the atlas grid (must be > 0)
 * @param frame_width  Width of each frame in pixels
 * @param frame_height Height of each frame in pixels
 * @return SDL_FRect with x, y, w, h for the source region
 */
inline SDL_FRect compute_source_rect(int frame_index, int columns,
                                      int frame_width, int frame_height) {
    int col = frame_index % columns;
    int row = frame_index / columns;
    return SDL_FRect{
        static_cast<float>(col * frame_width),
        static_cast<float>(row * frame_height),
        static_cast<float>(frame_width),
        static_cast<float>(frame_height)
    };
}

#endif // SPRITE_SHEET_MATH_HPP
