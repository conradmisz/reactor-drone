/**
 * Unit tests for Blackboard class
 * 
 * These tests verify specific examples and edge cases for the Blackboard's
 * type-safe global state storage functionality.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "engine/ecs/blackboard.hpp"
#include <string>

TEST_CASE("Blackboard stores and retrieves values", "[blackboard][unit]") {
    Blackboard bb;
    
    SECTION("Store and retrieve double") {
        bb.set("delta_time", 0.016);
        REQUIRE(bb.get<double>("delta_time") == 0.016);
    }
    
    SECTION("Store and retrieve int") {
        bb.set("window_width", 800);
        REQUIRE(bb.get<int>("window_width") == 800);
    }
    
    SECTION("Store and retrieve uint64_t") {
        bb.set("frame_count", static_cast<uint64_t>(12345));
        REQUIRE(bb.get<uint64_t>("frame_count") == 12345);
    }
    
    SECTION("Store and retrieve bool") {
        bb.set("is_paused", true);
        REQUIRE(bb.get<bool>("is_paused") == true);
    }
    
    SECTION("Store and retrieve string") {
        bb.set("player_name", std::string("Alice"));
        REQUIRE(bb.get<std::string>("player_name") == "Alice");
    }
}

TEST_CASE("Blackboard get() throws for non-existent keys", "[blackboard][unit]") {
    Blackboard bb;
    
    REQUIRE_THROWS_AS(bb.get<double>("missing_key"), std::runtime_error);
    
    try {
        bb.get<double>("missing_key");
        FAIL("Expected exception was not thrown");
    } catch (const std::runtime_error& e) {
        REQUIRE(std::string(e.what()) == "Blackboard key not found: missing_key");
    }
}

TEST_CASE("Blackboard get_or() returns default for non-existent keys", "[blackboard][unit]") {
    Blackboard bb;
    
    SECTION("Returns default when key doesn't exist") {
        REQUIRE(bb.get_or("missing_key", 42.0) == 42.0);
        REQUIRE(bb.get_or("missing_int", 100) == 100);
    }
    
    SECTION("Returns stored value when key exists") {
        bb.set("fps", 60.0);
        REQUIRE(bb.get_or("fps", 30.0) == 60.0);
    }
}

TEST_CASE("Blackboard has() method correctness", "[blackboard][unit]") {
    Blackboard bb;
    
    SECTION("Returns false for non-existent key") {
        REQUIRE_FALSE(bb.has("missing_key"));
    }
    
    SECTION("Returns true after setting a key") {
        bb.set("delta_time", 0.016);
        REQUIRE(bb.has("delta_time"));
    }
    
    SECTION("Returns false after removing a key") {
        bb.set("fps", 60.0);
        REQUIRE(bb.has("fps"));
        bb.remove("fps");
        REQUIRE_FALSE(bb.has("fps"));
    }
}

TEST_CASE("Blackboard remove() method correctness", "[blackboard][unit]") {
    Blackboard bb;
    bb.set("delta_time", 0.016);
    bb.set("fps", 60.0);
    
    SECTION("Remove deletes the key") {
        bb.remove("delta_time");
        REQUIRE_FALSE(bb.has("delta_time"));
        REQUIRE_THROWS_AS(bb.get<double>("delta_time"), std::runtime_error);
    }
    
    SECTION("Remove doesn't affect other keys") {
        bb.remove("delta_time");
        REQUIRE(bb.has("fps"));
        REQUIRE(bb.get<double>("fps") == 60.0);
    }
    
    SECTION("Remove on non-existent key is safe") {
        bb.remove("missing_key");  // Should not throw
        REQUIRE(bb.has("delta_time"));
        REQUIRE(bb.has("fps"));
    }
}

TEST_CASE("Blackboard clear() method correctness", "[blackboard][unit]") {
    Blackboard bb;
    bb.set("delta_time", 0.016);
    bb.set("fps", 60.0);
    bb.set("frame_count", static_cast<uint64_t>(100));
    
    bb.clear();
    
    REQUIRE_FALSE(bb.has("delta_time"));
    REQUIRE_FALSE(bb.has("fps"));
    REQUIRE_FALSE(bb.has("frame_count"));
}

TEST_CASE("Blackboard type mismatch error handling", "[blackboard][unit]") {
    Blackboard bb;
    bb.set("delta_time", 0.016);  // Store as double
    
    SECTION("get() throws on type mismatch") {
        REQUIRE_THROWS_AS(bb.get<int>("delta_time"), std::runtime_error);
        
        try {
            bb.get<int>("delta_time");
            FAIL("Expected exception was not thrown");
        } catch (const std::runtime_error& e) {
            REQUIRE(std::string(e.what()) == "Blackboard type mismatch for key: delta_time");
        }
    }
    
    SECTION("get_or() throws on type mismatch") {
        REQUIRE_THROWS_AS(bb.get_or<int>("delta_time", 42), std::runtime_error);
    }
}

TEST_CASE("Blackboard empty string key rejection", "[blackboard][unit]") {
    Blackboard bb;
    
    REQUIRE_THROWS_AS(bb.set("", 42.0), std::invalid_argument);
    
    try {
        bb.set("", 42.0);
        FAIL("Expected exception was not thrown");
    } catch (const std::invalid_argument& e) {
        REQUIRE(std::string(e.what()) == "Blackboard key cannot be empty");
    }
}

TEST_CASE("Blackboard overwrites existing keys", "[blackboard][unit]") {
    Blackboard bb;
    bb.set("value", 100);
    bb.set("value", 200);
    
    REQUIRE(bb.get<int>("value") == 200);
}

TEST_CASE("Blackboard stores standard timing data", "[blackboard][unit]") {
    Blackboard bb;
    
    // Simulate Timer writing timing data
    bb.set("delta_time", 0.0167);
    bb.set("fps", 59.88);
    bb.set("frame_count", static_cast<uint64_t>(120));
    
    // Simulate systems reading timing data
    REQUIRE(bb.get<double>("delta_time") == 0.0167);
    REQUIRE(bb.get<double>("fps") == 59.88);
    REQUIRE(bb.get<uint64_t>("frame_count") == 120);
}

TEST_CASE("Blackboard stores window dimensions", "[blackboard][unit]") {
    Blackboard bb;
    
    // Simulate game initialization
    bb.set("window_width", 800);
    bb.set("window_height", 600);
    
    // Simulate systems reading window dimensions
    REQUIRE(bb.get<int>("window_width") == 800);
    REQUIRE(bb.get<int>("window_height") == 600);
}
