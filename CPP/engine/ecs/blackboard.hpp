#ifndef BLACKBOARD_HPP
#define BLACKBOARD_HPP

#include <any>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

/**
 * Blackboard class
 * 
 * Provides type-safe global state storage for the game engine.
 * The Blackboard stores key-value pairs where keys are strings and values
 * can be of any type (using std::any for type-safe heterogeneous storage).
 * 
 * Common use cases:
 * - Storing timing data (delta_time, fps, frame_count)
 * - Storing window dimensions (window_width, window_height)
 * - Sharing global game state between systems
 * 
 * Design rationale:
 * - std::any provides type safety while allowing heterogeneous storage
 * - String keys are readable and debuggable
 * - Explicit error handling for missing keys and type mismatches
 */
class Blackboard {
public:
    /**
     * Store a value in the blackboard
     * 
     * @param key The key to store the value under (must not be empty)
     * @param value The value to store (can be any type)
     * @throws std::invalid_argument if key is empty
     */
    template<typename T>
    void set(const std::string& key, const T& value) {
        if (key.empty()) {
            throw std::invalid_argument("Blackboard key cannot be empty");
        }
        data_[key] = value;
    }
    
    /**
     * Retrieve a value from the blackboard
     * 
     * @param key The key to retrieve
     * @return The value associated with the key
     * @throws std::runtime_error if key does not exist
     * @throws std::runtime_error if type mismatch occurs
     */
    template<typename T>
    T get(const std::string& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) {
            throw std::runtime_error("Blackboard key not found: " + key);
        }
        
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            throw std::runtime_error("Blackboard type mismatch for key: " + key);
        }
    }
    
    /**
     * Retrieve a value from the blackboard, or return a default if key doesn't exist
     * 
     * @param key The key to retrieve
     * @param default_value The value to return if key doesn't exist
     * @return The value associated with the key, or default_value if key not found
     * @throws std::runtime_error if type mismatch occurs
     */
    template<typename T>
    T get_or(const std::string& key, const T& default_value) const {
        auto it = data_.find(key);
        if (it == data_.end()) {
            return default_value;
        }
        
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            throw std::runtime_error("Blackboard type mismatch for key: " + key);
        }
    }
    
    /**
     * Check if a key exists in the blackboard
     * 
     * @param key The key to check
     * @return true if the key exists, false otherwise
     */
    bool has(const std::string& key) const {
        return data_.find(key) != data_.end();
    }
    
    /**
     * Remove a key-value pair from the blackboard
     * 
     * @param key The key to remove
     */
    void remove(const std::string& key) {
        data_.erase(key);
    }
    
    /**
     * Remove all key-value pairs from the blackboard
     */
    void clear() {
        data_.clear();
    }
    
    /**
     * Get all keys currently stored in the blackboard.
     * Returns keys in no particular order.
     */
    std::vector<std::string> get_all_keys() const {
        std::vector<std::string> keys;
        keys.reserve(data_.size());
        for (const auto& [key, _] : data_) {
            keys.push_back(key);
        }
        return keys;
    }

private:
    std::unordered_map<std::string, std::any> data_;
};

#endif // BLACKBOARD_HPP
