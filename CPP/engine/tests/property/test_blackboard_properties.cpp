/**
 * Property-based tests for Blackboard class
 * 
 * These tests verify universal properties that should hold across all inputs
 * using Catch2's GENERATE() for property-based testing.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/blackboard.hpp"
#include <string>
#include <sstream>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different keys to test
constexpr int NUM_INNER_TESTS = 5;   // Number of different values per key

// Feature: red-moving-square-class-020, Property 8: Blackboard Set/Get Round Trip
// **Validates: Requirements 3.2**
TEST_CASE("Blackboard set/get round trip for all supported types", "[blackboard][property]") {
    Blackboard bb;
    
    SECTION("Round trip for double values") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key_str(key.begin(), key.end());
        
        bb.set(key_str, value);
        double retrieved = bb.get<double>(key_str);
        
        REQUIRE(retrieved == value);
    }
    
    SECTION("Round trip for int values") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-10000, 10000)));
        
        std::string key_str(key.begin(), key.end());
        
        bb.set(key_str, value);
        int retrieved = bb.get<int>(key_str);
        
        REQUIRE(retrieved == value);
    }
    
    SECTION("Round trip for uint64_t values") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto raw = GENERATE(take(NUM_INNER_TESTS, random(0u, 1000000u)));
        uint64_t value = static_cast<uint64_t>(raw);
        
        std::string key_str(key.begin(), key.end());
        
        bb.set(key_str, value);
        uint64_t retrieved = bb.get<uint64_t>(key_str);
        
        REQUIRE(retrieved == value);
    }
    
    SECTION("Round trip for bool values") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(0, 1)));
        
        std::string key_str(key.begin(), key.end());
        bool bool_value = (value == 1);
        
        bb.set(key_str, bool_value);
        bool retrieved = bb.get<bool>(key_str);
        
        REQUIRE(retrieved == bool_value);
    }
    
    SECTION("Round trip for string values") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value_chars = GENERATE(take(NUM_INNER_TESTS, chunk(20, random('a', 'z'))));
        
        std::string key_str(key.begin(), key.end());
        std::string value_str(value_chars.begin(), value_chars.end());
        
        bb.set(key_str, value_str);
        std::string retrieved = bb.get<std::string>(key_str);
        
        REQUIRE(retrieved == value_str);
    }
}

// Feature: red-moving-square-class-020, Property 9: Blackboard Has Method Correctness
// **Validates: Requirements 3.5**
TEST_CASE("Blackboard has() returns true iff key was set and not removed", "[blackboard][property]") {
    Blackboard bb;
    
    SECTION("has() returns false for keys that were never set") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        std::string key_str(key.begin(), key.end());
        
        REQUIRE_FALSE(bb.has(key_str));
    }
    
    SECTION("has() returns true after set() is called") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key_str(key.begin(), key.end());
        
        bb.set(key_str, value);
        
        REQUIRE(bb.has(key_str));
    }
    
    SECTION("has() returns false after remove() is called") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key_str(key.begin(), key.end());
        
        // Set the key
        bb.set(key_str, value);
        REQUIRE(bb.has(key_str));
        
        // Remove the key
        bb.remove(key_str);
        
        // has() should now return false
        REQUIRE_FALSE(bb.has(key_str));
    }
    
    SECTION("has() returns true after set() is called again following remove()") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value1 = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key_str(key.begin(), key.end());
        double value2 = value1 + 42.0;  // Derived instead of nested GENERATE to avoid combinatorial explosion
        
        // Set, remove, then set again
        bb.set(key_str, value1);
        bb.remove(key_str);
        bb.set(key_str, value2);
        
        // has() should return true
        REQUIRE(bb.has(key_str));
    }
}

// Feature: red-moving-square-class-020, Property 10: Blackboard Remove Method Correctness
// **Validates: Requirements 3.6**
TEST_CASE("Blackboard remove() correctly removes keys", "[blackboard][property]") {
    Blackboard bb;
    
    SECTION("has() returns false after remove()") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key_str(key.begin(), key.end());
        
        // Set the key
        bb.set(key_str, value);
        REQUIRE(bb.has(key_str));
        
        // Remove the key
        bb.remove(key_str);
        
        // has() should return false
        REQUIRE_FALSE(bb.has(key_str));
    }
    
    SECTION("get() throws exception after remove()") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key_str(key.begin(), key.end());
        
        // Set the key
        bb.set(key_str, value);
        REQUIRE(bb.has(key_str));
        
        // Remove the key
        bb.remove(key_str);
        
        // get() should throw std::runtime_error
        REQUIRE_THROWS_AS(bb.get<double>(key_str), std::runtime_error);
    }
    
    SECTION("remove() is idempotent (removing non-existent key is safe)") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        
        std::string key_str(key.begin(), key.end());
        
        // Remove a key that was never set (should not throw)
        REQUIRE_NOTHROW(bb.remove(key_str));
        
        // has() should still return false
        REQUIRE_FALSE(bb.has(key_str));
    }
    
    SECTION("remove() only removes the specified key") {
        auto key1 = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value1 = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key1_str(key1.begin(), key1.end());
        // Derive key2 and value2 to avoid combinatorial explosion (was 4 nested GENERATEs = 2,500 cases)
        std::string key2_str = key1_str + "_other";
        double value2 = value1 + 99.0;
        
        // Set both keys
        bb.set(key1_str, value1);
        bb.set(key2_str, value2);
        
        // Remove only key1
        bb.remove(key1_str);
        
        // key1 should be gone, key2 should remain
        REQUIRE_FALSE(bb.has(key1_str));
        REQUIRE(bb.has(key2_str));
        REQUIRE(bb.get<double>(key2_str) == value2);
    }
}

// Feature: red-moving-square-class-020, Property 11: Blackboard Clear Method Correctness
// **Validates: Requirements 3.7**
TEST_CASE("Blackboard clear() removes all keys", "[blackboard][property]") {
    Blackboard bb;
    
    SECTION("clear() removes all previously set keys") {
        // Use single GENERATE for base key, derive others to avoid combinatorial explosion
        // (was 4 nested GENERATEs = 5,000 cases)
        auto key1 = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key1_str(key1.begin(), key1.end());
        std::string key2_str = key1_str + "_second";
        std::string key3_str = key1_str + "_third";
        
        // Set multiple keys
        bb.set(key1_str, value);
        bb.set(key2_str, value + 1.0);
        bb.set(key3_str, value + 2.0);
        
        // Verify all keys exist
        REQUIRE(bb.has(key1_str));
        REQUIRE(bb.has(key2_str));
        REQUIRE(bb.has(key3_str));
        
        // Clear the blackboard
        bb.clear();
        
        // Verify all keys are gone
        REQUIRE_FALSE(bb.has(key1_str));
        REQUIRE_FALSE(bb.has(key2_str));
        REQUIRE_FALSE(bb.has(key3_str));
    }
    
    SECTION("clear() on empty blackboard is safe") {
        // Clear an empty blackboard (should not throw)
        REQUIRE_NOTHROW(bb.clear());
    }
    
    SECTION("clear() is idempotent") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key_str(key.begin(), key.end());
        
        // Set a key
        bb.set(key_str, value);
        REQUIRE(bb.has(key_str));
        
        // Clear twice
        bb.clear();
        bb.clear();
        
        // Key should still be gone
        REQUIRE_FALSE(bb.has(key_str));
    }
    
    SECTION("set() works after clear()") {
        auto key = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        auto value1 = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        
        std::string key_str(key.begin(), key.end());
        double value2 = value1 + 42.0;  // Derived instead of nested GENERATE to avoid combinatorial explosion
        
        // Set, clear, then set again
        bb.set(key_str, value1);
        bb.clear();
        bb.set(key_str, value2);
        
        // Key should exist with new value
        REQUIRE(bb.has(key_str));
        REQUIRE(bb.get<double>(key_str) == value2);
    }
}
