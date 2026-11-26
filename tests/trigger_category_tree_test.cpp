// doctest's own forward declaration of std::char_traits breaks std::string here otherwise.
#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

import std;
import Triggers;

namespace {
	using Parents = std::vector<std::optional<std::size_t>>;

	/// A (top level) holds B, B holds C, D is top level.
	Triggers nested_categories() {
		Triggers triggers;
		triggers.categories.push_back(make_new_trigger_category("A", 1, 0));
		triggers.categories.push_back(make_new_trigger_category("B", 2, 1));
		triggers.categories.push_back(make_new_trigger_category("C", 3, 2));
		triggers.categories.push_back(make_new_trigger_category("D", 4, 0));
		return triggers;
	}

	std::vector<int> parent_ids(const Triggers& triggers) {
		std::vector<int> result;
		for (const TriggerCategory& category : triggers.categories) {
			result.push_back(category.parent_id);
		}
		return result;
	}
}

TEST_CASE("the trigger tree finds a category's parent wherever it is stored") {
	Triggers triggers;
	triggers.categories.push_back({ Classifier::map, 0, "Map Header", true, false, -1 });
	triggers.categories.push_back(make_new_trigger_category("Moved", 1, 2)); // stored before its parent
	triggers.categories.push_back(make_new_trigger_category("Later", 2, 3)); // and so is this one
	triggers.categories.push_back(make_new_trigger_category("Top", 3, 0));
	triggers.categories.push_back(make_new_trigger_category("Child", 4, 3));

	CHECK(category_tree_parents(triggers.categories) == Parents { std::nullopt, 2, 3, std::nullopt, 3 });
}

TEST_CASE("the trigger tree breaks a stored parent cycle so every category is reached from the map header") {
	std::vector<TriggerCategory> categories;
	categories.push_back(make_new_trigger_category("Under the cycle", 1, 2));
	categories.push_back(make_new_trigger_category("A", 2, 3));
	categories.push_back(make_new_trigger_category("B", 3, 4));
	categories.push_back(make_new_trigger_category("C", 4, 2));
	categories.push_back(make_new_trigger_category("Self", 5, 5));
	categories.push_back(make_new_trigger_category("D", 6, 7));
	categories.push_back(make_new_trigger_category("E", 7, 6));
	const std::vector<TriggerCategory> before = categories;

	// The first category of each cycle goes to the map header; the others keep their parents.
	CHECK(category_tree_parents(categories) == Parents { 1, std::nullopt, 3, 1, std::nullopt, std::nullopt, 5 });
	for (std::size_t i = 0; i < categories.size(); i++) {
		CHECK(categories[i].parent_id == before[i].parent_id);
	}
}

TEST_CASE("the trigger tree lists a category with a missing parent under the map header") {
	std::vector<TriggerCategory> categories;
	categories.push_back(make_new_trigger_category("Orphan", 1, 999));
	categories.push_back(make_new_trigger_category("Child of the orphan", 2, 1));
	categories.push_back(make_new_trigger_category("Parent -1 but not the header", 3, -1));
	categories.push_back(make_new_trigger_category("Child of that", 4, 3));

	CHECK_FALSE(is_map_header_entry(categories[2]));
	CHECK(category_tree_parents(categories) == Parents { std::nullopt, 0, std::nullopt, 2 });
}

TEST_CASE("the trigger tree gives a duplicated parent id to the nearest category stored before the child") {
	std::vector<TriggerCategory> categories;
	categories.push_back(make_new_trigger_category("First", 1, 0));
	categories.push_back(make_new_trigger_category("Second", 1, 0));
	categories.push_back(make_new_trigger_category("Child", 2, 1));
	categories.push_back(make_new_trigger_category("Third", 1, 0));

	CHECK(category_tree_parents(categories) == Parents { std::nullopt, std::nullopt, 1, std::nullopt });
}

TEST_CASE("moving a category in a map with a stored parent cycle ends and never keeps the moved one inside itself") {
	Triggers triggers;
	triggers.categories.push_back(make_new_trigger_category("A", 1, 2));
	triggers.categories.push_back(make_new_trigger_category("B", 2, 1));
	triggers.categories.push_back(make_new_trigger_category("Other", 3, 0));

	CHECK(triggers.is_category_inside(1, 2));
	CHECK_FALSE(triggers.is_category_inside(1, 3));
	triggers.set_element_parent(Classifier::category, 3, 1); // into the cycle: it is not inside the cycle, so allowed
	CHECK(triggers.categories[2].parent_id == 1);
	triggers.set_element_parent(Classifier::category, 2, 0); // to the top level: repairs the cycle
	CHECK(parent_ids(triggers) == std::vector<int> { 2, 0, 1 });
}

TEST_CASE("a trigger category is never moved under itself or under its own subcategory") {
	Triggers triggers = nested_categories();
	const std::vector<int> before = parent_ids(triggers);

	triggers.set_element_parent(Classifier::category, 1, 1); // itself
	CHECK(parent_ids(triggers) == before);
	triggers.set_element_parent(Classifier::category, 1, 2); // its child
	CHECK(parent_ids(triggers) == before);
	triggers.set_element_parent(Classifier::category, 1, 3); // its grandchild
	CHECK(parent_ids(triggers) == before);

	triggers.set_element_parent(Classifier::category, 3, 4); // a normal move
	CHECK(parent_ids(triggers) == std::vector<int> { 0, 1, 4, 0 });
	triggers.set_element_parent(Classifier::category, 3, 0); // back to the top level
	CHECK(parent_ids(triggers) == std::vector<int> { 0, 1, 0, 0 });
}
