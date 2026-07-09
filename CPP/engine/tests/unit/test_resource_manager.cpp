#include <catch2/catch_test_macros.hpp>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <memory>

#include "engine/resource_manager.hpp"

// Absolute path to test assets, resolved at compile time via CLASS_ROOT_DIR
static const std::string TEST_ASSETS_DIR =
    std::string(CLASS_ROOT_DIR) + "/CPP/engine/tests/test_assets";

// ============================================================================
// Test Fixture
// ============================================================================

struct ResourceManagerFixture {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::unique_ptr<ResourceManager> rm;

    ResourceManagerFixture() {
        REQUIRE(SDL_Init(SDL_INIT_VIDEO));
        REQUIRE(TTF_Init());

        window = SDL_CreateWindow("Test", 64, 64, SDL_WINDOW_HIDDEN);
        REQUIRE(window != nullptr);

        renderer = SDL_CreateRenderer(window, nullptr);
        REQUIRE(renderer != nullptr);

        rm = std::make_unique<ResourceManager>(renderer, TEST_ASSETS_DIR);
    }

    ~ResourceManagerFixture() {
        rm.reset();

        if (renderer) {
            SDL_DestroyRenderer(renderer);
        }
        if (window) {
            SDL_DestroyWindow(window);
        }

        TTF_Quit();
        SDL_Quit();
    }
};

// ============================================================================
// Unit Tests
// ============================================================================

TEST_CASE("Fresh manager has zero counts", "[resource_manager]") {
    ResourceManagerFixture f;
    CHECK(f.rm->texture_count() == 0);
    CHECK(f.rm->font_count() == 0);
    CHECK(f.rm->text_cache_count() == 0);
}

TEST_CASE("Missing texture is non-null", "[resource_manager]") {
    ResourceManagerFixture f;
    CHECK(f.rm->get_missing_texture() != nullptr);
}

TEST_CASE("Load valid texture returns non-null", "[resource_manager]") {
    ResourceManagerFixture f;
    auto* tex = f.rm->load_texture("test.png");
    CHECK(tex != nullptr);
    CHECK(tex != f.rm->get_missing_texture());
}

TEST_CASE("Load invalid texture returns missing texture", "[resource_manager]") {
    ResourceManagerFixture f;
    auto* tex = f.rm->load_texture("nonexistent.png");
    CHECK(tex == f.rm->get_missing_texture());
}

TEST_CASE("Load valid font returns non-null", "[resource_manager]") {
    ResourceManagerFixture f;
    auto* font = f.rm->load_font("test.ttf", 24.0f);
    CHECK(font != nullptr);
}

TEST_CASE("Load invalid font returns nullptr", "[resource_manager]") {
    ResourceManagerFixture f;
    auto* font = f.rm->load_font("nonexistent.ttf", 24.0f);
    CHECK(font == nullptr);
}

TEST_CASE("Render text with valid font returns non-null", "[resource_manager]") {
    ResourceManagerFixture f;
    auto* tex = f.rm->render_text("test.ttf", 24.0f, "hello", {255, 255, 255, 255});
    CHECK(tex != nullptr);
}

TEST_CASE("Render text with invalid font returns nullptr", "[resource_manager]") {
    ResourceManagerFixture f;
    auto* tex = f.rm->render_text("nonexistent.ttf", 24.0f, "hello", {255, 255, 255, 255});
    CHECK(tex == nullptr);
}

TEST_CASE("Same font different sizes are different entries", "[resource_manager]") {
    ResourceManagerFixture f;
    auto* f1 = f.rm->load_font("test.ttf", 24.0f);
    auto* f2 = f.rm->load_font("test.ttf", 48.0f);
    CHECK(f1 != f2);
    CHECK(f.rm->font_count() == 2);
}
