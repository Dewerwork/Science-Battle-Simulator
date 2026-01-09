#pragma once

#include "core/types.hpp"
#include <bitset>
#include <memory>

// Cross-platform popcount for 64-bit integers
#if defined(_MSC_VER)
#include <intrin.h>
inline int portable_popcountll(unsigned long long x) {
    return static_cast<int>(__popcnt64(x));
}
#else
inline int portable_popcountll(unsigned long long x) {
    return __builtin_popcountll(x);
}
#endif

namespace battle {

// ==============================================================================
// Rule Presence - Tiered Bitset Architecture
// ==============================================================================
// Solves the RuleMask overflow problem where u64 can only hold 64 rules.
// Uses a tiered approach:
//   - PrimaryRuleMask (u64): Hot path rules 0-63, fits in single cache line
//   - ExtendedRuleMask (bitset<256>): Cold path rules 64+, lazily allocated
//
// Performance characteristics:
//   - Primary rule lookup: O(1), single bitwise AND, no indirection
//   - Extended rule lookup: O(1), but requires pointer dereference
//   - Memory: 8 bytes base, +32 bytes if extended rules used

// Extended rule mask capacity (supports rules 64-319)
constexpr size_t EXTENDED_RULE_CAPACITY = 256;

// Maximum total rules supported
constexpr size_t MAX_RULES = PRIMARY_RULE_LIMIT + EXTENDED_RULE_CAPACITY;

// Extended rule mask - only allocated if extended rules are used
using ExtendedRuleMask = std::bitset<EXTENDED_RULE_CAPACITY>;

// ==============================================================================
// RulePresence - Hybrid structure for efficient rule presence tracking
// ==============================================================================

struct RulePresence {
    // Primary rules (0-63) - hot path, always present
    // Single 64-bit value for cache-friendly O(1) lookup
    PrimaryRuleMask primary = 0;

    // Extended rules (64+) - cold path, lazily allocated
    // Only allocated when unit has rules in extended range
    ExtendedRuleMask* extended = nullptr;

    // Default constructor
    RulePresence() = default;

    // Copy constructor - deep copies extended mask if present
    RulePresence(const RulePresence& other)
        : primary(other.primary)
        , extended(other.extended ? new ExtendedRuleMask(*other.extended) : nullptr) {}

    // Move constructor
    RulePresence(RulePresence&& other) noexcept
        : primary(other.primary)
        , extended(other.extended) {
        other.extended = nullptr;
    }

    // Copy assignment
    RulePresence& operator=(const RulePresence& other) {
        if (this != &other) {
            primary = other.primary;
            delete extended;
            extended = other.extended ? new ExtendedRuleMask(*other.extended) : nullptr;
        }
        return *this;
    }

    // Move assignment
    RulePresence& operator=(RulePresence&& other) noexcept {
        if (this != &other) {
            primary = other.primary;
            delete extended;
            extended = other.extended;
            other.extended = nullptr;
        }
        return *this;
    }

    // Destructor
    ~RulePresence() {
        delete extended;
    }

    // ==============================================================================
    // Core Operations
    // ==============================================================================

    // Check if a rule is present - O(1)
    bool has_rule(RuleId id) const {
        auto idx = static_cast<size_t>(id);
        if (idx == 0) return false;  // RuleId::None

        if (idx < PRIMARY_RULE_LIMIT) {
            // Primary rule - direct bit check
            return (primary & (PrimaryRuleMask(1) << idx)) != 0;
        }

        // Extended rule - check pointer first
        if (!extended) return false;
        size_t ext_idx = idx - PRIMARY_RULE_LIMIT;
        return ext_idx < EXTENDED_RULE_CAPACITY && extended->test(ext_idx);
    }

    // Set a rule as present
    void set_rule(RuleId id) {
        auto idx = static_cast<size_t>(id);
        if (idx == 0) return;  // RuleId::None

        if (idx < PRIMARY_RULE_LIMIT) {
            // Primary rule - set bit directly
            primary |= (PrimaryRuleMask(1) << idx);
        } else {
            // Extended rule - allocate if needed
            size_t ext_idx = idx - PRIMARY_RULE_LIMIT;
            if (ext_idx < EXTENDED_RULE_CAPACITY) {
                if (!extended) {
                    extended = new ExtendedRuleMask();
                }
                extended->set(ext_idx);
            }
        }
    }

    // Clear a rule
    void clear_rule(RuleId id) {
        auto idx = static_cast<size_t>(id);
        if (idx == 0) return;

        if (idx < PRIMARY_RULE_LIMIT) {
            primary &= ~(PrimaryRuleMask(1) << idx);
        } else if (extended) {
            size_t ext_idx = idx - PRIMARY_RULE_LIMIT;
            if (ext_idx < EXTENDED_RULE_CAPACITY) {
                extended->reset(ext_idx);
            }
        }
    }

    // Clear all rules
    void clear() {
        primary = 0;
        if (extended) {
            extended->reset();
        }
    }

    // Check if any rules are set
    bool any() const {
        if (primary != 0) return true;
        return extended && extended->any();
    }

    // Check if no rules are set
    bool none() const {
        return !any();
    }

    // Count total rules set
    size_t count() const {
        size_t n = static_cast<size_t>(portable_popcountll(primary));
        if (extended) {
            n += extended->count();
        }
        return n;
    }

    // Check if extended rules are in use
    bool has_extended() const {
        return extended && extended->any();
    }

    // ==============================================================================
    // Bitwise Operations
    // ==============================================================================

    // OR operation - combine rule sets
    RulePresence operator|(const RulePresence& other) const {
        RulePresence result;
        result.primary = primary | other.primary;
        if (extended || other.extended) {
            result.extended = new ExtendedRuleMask();
            if (extended) *result.extended |= *extended;
            if (other.extended) *result.extended |= *other.extended;
        }
        return result;
    }

    // AND operation - intersect rule sets
    RulePresence operator&(const RulePresence& other) const {
        RulePresence result;
        result.primary = primary & other.primary;
        if (extended && other.extended) {
            result.extended = new ExtendedRuleMask(*extended & *other.extended);
        }
        return result;
    }

    // Compound OR assignment
    RulePresence& operator|=(const RulePresence& other) {
        primary |= other.primary;
        if (other.extended) {
            if (!extended) {
                extended = new ExtendedRuleMask(*other.extended);
            } else {
                *extended |= *other.extended;
            }
        }
        return *this;
    }

    // ==============================================================================
    // Legacy Compatibility
    // ==============================================================================

    // Get primary mask for legacy code (backwards compatibility)
    PrimaryRuleMask get_primary_mask() const { return primary; }

    // Set from legacy RuleMask (for migration)
    void set_from_legacy_mask(RuleMask mask) {
        primary = mask;
    }
};

// ==============================================================================
// Compact Rule Set - For entities with few rules
// ==============================================================================
// Alternative representation for units with small number of rules.
// Uses parallel arrays which are more cache-friendly for small sets.

struct CompactRuleSet {
    static constexpr u8 MAX_RULES_INLINE = 16;

    std::array<RuleId, MAX_RULES_INLINE> ids{};
    std::array<u8, MAX_RULES_INLINE> values{};
    RulePresence presence;  // For fast has_rule() check
    u8 count = 0;

    // Add a rule with optional value
    bool add_rule(RuleId id, u8 value = 0) {
        if (id == RuleId::None || count >= MAX_RULES_INLINE) return false;
        if (presence.has_rule(id)) return false;  // Already present

        ids[count] = id;
        values[count] = value;
        count++;
        presence.set_rule(id);
        return true;
    }

    // Check if rule is present - O(1) using presence bitset
    bool has_rule(RuleId id) const {
        return presence.has_rule(id);
    }

    // Get rule value - O(n) but n is small (max 16)
    u8 get_value(RuleId id) const {
        if (!presence.has_rule(id)) return 0;
        for (u8 i = 0; i < count; ++i) {
            if (ids[i] == id) return values[i];
        }
        return 0;
    }

    // Clear all rules
    void clear() {
        count = 0;
        presence.clear();
        for (u8 i = 0; i < MAX_RULES_INLINE; ++i) {
            ids[i] = RuleId::None;
            values[i] = 0;
        }
    }

    // Iterator support for range-based for loops
    struct RuleIterator {
        const CompactRuleSet* set;
        u8 index;

        struct RuleRef {
            RuleId id;
            u8 value;
        };

        RuleRef operator*() const {
            return { set->ids[index], set->values[index] };
        }

        RuleIterator& operator++() { ++index; return *this; }
        bool operator!=(const RuleIterator& other) const { return index != other.index; }
    };

    RuleIterator begin() const { return { this, 0 }; }
    RuleIterator end() const { return { this, count }; }
};

} // namespace battle
