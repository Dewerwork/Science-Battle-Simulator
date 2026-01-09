#include "core/faction_rules.hpp"
#include "rules/factions/faction_rule_ids.hpp"

namespace battle {

// ==============================================================================
// Initialize all faction rules - delegates to new unified system
// ==============================================================================
// The old FactionRulesRegistry is now deprecated.
// All faction rule handling is done through the new system in faction_rule_ids.hpp.

void initialize_faction_rules() {
    // Initialize the new unified faction rules system
    register_all_faction_rules();

    // Mark old registry as initialized for backward compatibility
    get_faction_registry().set_initialized(true);
}

} // namespace battle
