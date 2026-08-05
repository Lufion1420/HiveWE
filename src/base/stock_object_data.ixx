export module StockObjectData;

import std;
import SLK;
import INI;
import Globals;

/// Loads the stock (base-game) object-data SLK/meta-SLK pairs for all 7 categories, independent of
/// any currently-loaded Map. Extracted verbatim from Map::load() (see map.ixx's units_future/
/// items_future/doodads_future/destructibles_future/abilities_future/upgrade_future/buff_future
/// blocks) so both Map::load() (which assigns the results into the global units_slk/units_meta_slk
/// etc. from the Globals module) and anything that needs a scratch copy of the stock tables without
/// disturbing whichever map is actually loaded in the editor - namely Map Protector's asset-path
/// obfuscation, which must classify/rewrite object-data path fields for an external source map that
/// was deliberately never loaded into HiveWE's Map object - can get identical, correct results from
/// one place instead of two independently-maintained copies of this loading logic.
namespace stock_object_data {
	export struct TableSet {
		slk::SLK data;
		slk::SLK meta;
	};

	export struct Tables {
		TableSet units;
		TableSet items;
		TableSet doodads;
		TableSet destructibles;
		TableSet abilities;
		TableSet upgrades;
		TableSet buffs;
		ini::INI unit_editor_data; // Object Editor UI-only data, not needed for path-field rewriting
	};

	export Tables load() {
		Tables tables;

		auto units_future = std::async(std::launch::async, [&] {
			tables.units.data = slk::SLK("Units/UnitData.slk");
			// By making some changes to unitmetadata.slk and unitdata.slk we can avoid the 1->2->2 mapping for SLK->OE->W3U files. We have to add some columns for this though
			tables.units.data.add_column("missilearc2");
			tables.units.data.add_column("missileart2");
			tables.units.data.add_column("missilespeed2");
			tables.units.data.add_column("buttonpos2");

			tables.units.meta = slk::SLK("Units/UnitMetaData.slk");
			tables.units.meta.substitute(world_edit_strings, "WorldEditStrings");
			tables.units.meta.build_meta_map();

			tables.unit_editor_data = ini::INI("UI/UnitEditorData.txt");
			tables.unit_editor_data.substitute(world_edit_strings, "WorldEditStrings");
			// Have to substitute twice since some keys refer to other keys in the same file
			tables.unit_editor_data.substitute(world_edit_strings, "WorldEditStrings");

			tables.units.data.merge(ini::INI("Units/UnitSkin.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/UnitWeaponsFunc.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/UnitWeaponsSkin.txt"), tables.units.meta);

			tables.units.data.merge(slk::SLK("Units/UnitBalance.slk"));
			tables.units.data.merge(slk::SLK("Units/unitUI.slk"));
			tables.units.data.merge(slk::SLK("Units/UnitWeapons.slk"));
			tables.units.data.merge(slk::SLK("Units/UnitAbilities.slk"));

			tables.units.data.merge(ini::INI("Units/HumanUnitFunc.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/OrcUnitFunc.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/UndeadUnitFunc.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/NightElfUnitFunc.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/NeutralUnitFunc.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/CampaignUnitFunc.txt"), tables.units.meta);

			tables.units.data.merge(ini::INI("Units/HumanUnitStrings.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/OrcUnitStrings.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/UndeadUnitStrings.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/NightElfUnitStrings.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/NeutralUnitStrings.txt"), tables.units.meta);
			tables.units.data.merge(ini::INI("Units/CampaignUnitStrings.txt"), tables.units.meta);
		});

		auto items_future = std::async(std::launch::async, [&] {
			tables.items.data = slk::SLK("Units/ItemData.slk");
			tables.items.meta = slk::SLK("Units/ItemMetaData.slk");
			tables.items.meta.substitute(world_edit_strings, "WorldEditStrings");
			tables.items.meta.build_meta_map();

			tables.items.data.merge(ini::INI("Units/ItemSkin.txt"), tables.items.meta);
			tables.items.data.merge(ini::INI("Units/ItemFunc.txt"), tables.items.meta);
			tables.items.data.merge(ini::INI("Units/ItemStrings.txt"), tables.items.meta);
		});

		auto doodads_future = std::async(std::launch::async, [&] {
			tables.doodads.data = slk::SLK("Doodads/Doodads.slk");
			tables.doodads.meta = slk::SLK("Doodads/DoodadMetaData.slk");
			tables.doodads.meta.substitute(world_edit_strings, "WorldEditStrings");
			tables.doodads.meta.build_meta_map();

			tables.doodads.data.merge(ini::INI("Doodads/DoodadSkins.txt"), tables.doodads.meta);
			tables.doodads.data.substitute(world_edit_strings, "WorldEditStrings");
			tables.doodads.data.substitute(world_edit_game_strings, "WorldEditStrings");

			// Sometimes fields are empty or "-" which denotes empty aka the value 0.0
			for (auto& [key, fields] : tables.doodads.data.base_data) {
				if (auto found = fields.find("maxpitch"); found != fields.end()) {
					if (found->second.empty() || found->second == "-") {
						found->second = "0";
					}
				} else {
					fields["maxpitch"] = "0";
				}
				if (auto found = fields.find("maxroll"); found != fields.end()) {
					if (found->second.empty() || found->second == "-") {
						found->second = "0";
					}
				} else {
					fields["maxroll"] = "0";
				}
			}
		});

		auto destructibles_future = std::async(std::launch::async, [&] {
			tables.destructibles.data = slk::SLK("Units/DestructableData.slk");
			tables.destructibles.data.substitute(world_edit_strings, "WorldEditStrings");

			tables.destructibles.meta = slk::SLK("Units/DestructableMetaData.slk");
			tables.destructibles.meta.substitute(world_edit_strings, "WorldEditStrings");
			tables.destructibles.meta.build_meta_map();

			tables.destructibles.data.merge(ini::INI("Units/DestructableSkin.txt"), tables.destructibles.meta);
			tables.destructibles.data.substitute(world_edit_strings, "WorldEditStrings");
			tables.destructibles.data.substitute(world_edit_game_strings, "WorldEditStrings");

			// Sometimes fields are empty or "-" which denotes empty aka the value 0.0
			for (auto& [key, fields] : tables.destructibles.data.base_data) {
				if (auto found = fields.find("maxpitch"); found != fields.end()) {
					if (found->second.empty() || found->second == "-") {
						found->second = "0";
					}
				} else {
					fields["maxpitch"] = "0";
				}
				if (auto found = fields.find("maxroll"); found != fields.end()) {
					if (found->second.empty() || found->second == "-") {
						found->second = "0";
					}
				} else {
					fields["maxroll"] = "0";
				}
			}
		});

		// Load shared files
		const ini::INI ability_skin_ini("Units/AbilitySkin.txt");
		const ini::INI ability_skin_strings_ini("Units/AbilitySkinStrings.txt");
		const ini::INI human_ability_func_ini("Units/HumanAbilityFunc.txt");
		const ini::INI orc_ability_func_ini("Units/OrcAbilityFunc.txt");
		const ini::INI undead_ability_func_ini("Units/UndeadAbilityFunc.txt");
		const ini::INI night_elf_ability_func_ini("Units/NightElfAbilityFunc.txt");
		const ini::INI neutral_ability_func_ini("Units/NeutralAbilityFunc.txt");
		const ini::INI item_ability_func_ini("Units/ItemAbilityFunc.txt");
		const ini::INI common_ability_func_ini("Units/CommonAbilityFunc.txt");
		const ini::INI campaign_ability_func_ini("Units/CampaignAbilityFunc.txt");
		const ini::INI human_ability_strings_ini("Units/HumanAbilityStrings.txt");
		const ini::INI orc_ability_strings_ini("Units/OrcAbilityStrings.txt");
		const ini::INI undead_ability_strings_ini("Units/UndeadAbilityStrings.txt");
		const ini::INI night_elf_ability_strings_ini("Units/NightElfAbilityStrings.txt");
		const ini::INI neutral_ability_strings_ini("Units/NeutralAbilityStrings.txt");
		const ini::INI item_ability_strings_ini("Units/ItemAbilityStrings.txt");
		const ini::INI common_ability_strings_ini("Units/CommonAbilityStrings.txt");
		const ini::INI campaign_ability_strings_ini("Units/CampaignAbilityStrings.txt");

		auto abilities_future = std::async(std::launch::async, [&] {
			tables.abilities.data = slk::SLK("Units/AbilityData.slk");
			tables.abilities.meta = slk::SLK("Units/AbilityMetaData.slk");
			tables.abilities.meta.substitute(world_edit_strings, "WorldEditStrings");

			// Patch the SLKs
			tables.abilities.data.add_column("buttonpos2");
			tables.abilities.data.add_column("unbuttonpos2");
			tables.abilities.data.add_column("researchbuttonpos2");
			tables.abilities.meta.set_shadow_data("field", "abpy", "buttonpos2");
			tables.abilities.meta.set_shadow_data("field", "auby", "unbuttonpos2");
			tables.abilities.meta.set_shadow_data("field", "arpy", "researchbuttonpos2");
			tables.abilities.meta.set_shadow_data("type", "abuf", "buffList");
			tables.abilities.meta.set_shadow_data("type", "ahky", "hotkey");
			tables.abilities.meta.set_shadow_data("type", "auhk", "hotkey");
			tables.abilities.meta.build_meta_map();

			tables.abilities.data.merge(ability_skin_ini, tables.abilities.meta);
			tables.abilities.data.merge(ability_skin_strings_ini, tables.abilities.meta);
			tables.abilities.data.merge(human_ability_func_ini, tables.abilities.meta);
			tables.abilities.data.merge(orc_ability_func_ini, tables.abilities.meta);
			tables.abilities.data.merge(undead_ability_func_ini, tables.abilities.meta);
			tables.abilities.data.merge(night_elf_ability_func_ini, tables.abilities.meta);
			tables.abilities.data.merge(neutral_ability_func_ini, tables.abilities.meta);
			tables.abilities.data.merge(item_ability_func_ini, tables.abilities.meta);
			tables.abilities.data.merge(common_ability_func_ini, tables.abilities.meta);
			tables.abilities.data.merge(campaign_ability_func_ini, tables.abilities.meta);

			tables.abilities.data.merge(human_ability_strings_ini, tables.abilities.meta);
			tables.abilities.data.merge(orc_ability_strings_ini, tables.abilities.meta);
			tables.abilities.data.merge(undead_ability_strings_ini, tables.abilities.meta);
			tables.abilities.data.merge(night_elf_ability_strings_ini, tables.abilities.meta);
			tables.abilities.data.merge(neutral_ability_strings_ini, tables.abilities.meta);
			tables.abilities.data.merge(item_ability_strings_ini, tables.abilities.meta);
			tables.abilities.data.merge(common_ability_strings_ini, tables.abilities.meta);
			tables.abilities.data.merge(campaign_ability_strings_ini, tables.abilities.meta);
		});

		// Upgrades
		auto upgrade_future = std::async(std::launch::async, [&] {
			tables.upgrades.data = slk::SLK("Units/UpgradeData.slk");
			tables.upgrades.meta = slk::SLK("Units/UpgradeMetaData.slk");
			tables.upgrades.meta.substitute(world_edit_strings, "WorldEditStrings");

			// Patch the SLKs
			tables.upgrades.data.add_column("buttonpos2");
			tables.upgrades.meta.set_shadow_data("field", "gbpy", "buttonpos2");
			tables.upgrades.meta.build_meta_map();

			tables.upgrades.data.merge(ability_skin_ini, tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/UpgradeSkin.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/HumanUpgradeFunc.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/OrcUpgradeFunc.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/UndeadUpgradeFunc.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/NightElfUpgradeFunc.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/NeutralUpgradeFunc.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/CampaignUpgradeFunc.txt"), tables.upgrades.meta);

			tables.upgrades.data.merge(ini::INI("Units/CampaignUpgradeStrings.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/HumanUpgradeStrings.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/NeutralUpgradeStrings.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/NightElfUpgradeStrings.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/OrcUpgradeStrings.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/UndeadUpgradeStrings.txt"), tables.upgrades.meta);
			tables.upgrades.data.merge(ini::INI("Units/UpgradeSkinStrings.txt"), tables.upgrades.meta);
		});

		auto buff_future = std::async(std::launch::async, [&] {
			tables.buffs.data = slk::SLK("Units/AbilityBuffData.slk");
			tables.buffs.meta = slk::SLK("Units/AbilityBuffMetaData.slk");
			tables.buffs.meta.substitute(world_edit_strings, "WorldEditStrings");
			tables.buffs.meta.build_meta_map();

			tables.buffs.data.merge(ability_skin_ini, tables.buffs.meta);
			tables.buffs.data.merge(ability_skin_strings_ini, tables.buffs.meta);
			tables.buffs.data.merge(human_ability_func_ini, tables.buffs.meta);
			tables.buffs.data.merge(orc_ability_func_ini, tables.buffs.meta);
			tables.buffs.data.merge(undead_ability_func_ini, tables.buffs.meta);
			tables.buffs.data.merge(night_elf_ability_func_ini, tables.buffs.meta);
			tables.buffs.data.merge(neutral_ability_func_ini, tables.buffs.meta);
			tables.buffs.data.merge(item_ability_func_ini, tables.buffs.meta);
			tables.buffs.data.merge(common_ability_func_ini, tables.buffs.meta);
			tables.buffs.data.merge(campaign_ability_func_ini, tables.buffs.meta);

			tables.buffs.data.merge(human_ability_strings_ini, tables.buffs.meta);
			tables.buffs.data.merge(orc_ability_strings_ini, tables.buffs.meta);
			tables.buffs.data.merge(undead_ability_strings_ini, tables.buffs.meta);
			tables.buffs.data.merge(night_elf_ability_strings_ini, tables.buffs.meta);
			tables.buffs.data.merge(neutral_ability_strings_ini, tables.buffs.meta);
			tables.buffs.data.merge(item_ability_strings_ini, tables.buffs.meta);
			tables.buffs.data.merge(common_ability_strings_ini, tables.buffs.meta);
			tables.buffs.data.merge(campaign_ability_strings_ini, tables.buffs.meta);
		});

		units_future.get();
		abilities_future.get();
		items_future.get();
		doodads_future.get();
		destructibles_future.get();
		upgrade_future.get();
		buff_future.get();

		return tables;
	}
} // namespace stock_object_data
