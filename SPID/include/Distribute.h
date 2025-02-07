#pragma once

#include "LookupForms.h"
#include "LookupNPC.h"
#include "PCLevelMultManager.h"

namespace Distribute
{
	template <class Form>
	void for_each_form(
		const NPCData& a_npcData,
		Forms::Distributables<Form>& a_distributables,
		const PCLevelMult::Input& a_input,
		std::function<bool(Form*, IdxOrCount&)> a_callback)
	{
		auto& vec = a_input.onlyPlayerLevelEntries ? a_distributables.formsWithLevels : a_distributables.forms;

		const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();

		for (std::uint32_t idx = 0; auto& formData : vec) {
			++idx;
			auto& [form, idxOrCount, filters, npcCount] = formData;
			auto distributedFormID = form->GetFormID();

			if (pcLevelMultManager->FindRejectedEntry(a_input, distributedFormID, idx)) {
				continue;
			}

			auto result = filters.PassedFilters(a_npcData, a_input.noPlayerLevelDistribution);
			if (result != Filter::Result::kPass) {
				if (result == Filter::Result::kFailRNG) {
					pcLevelMultManager->InsertRejectedEntry(a_input, distributedFormID, idx);
				}
				continue;
			}

			if (a_callback(form, idxOrCount)) {
				pcLevelMultManager->InsertDistributedEntry(a_input, distributedFormID, idxOrCount);
				++npcCount;
			}
		}
	}

	template <class Form>
	void list_npc_count(std::string_view a_recordType, Forms::Distributables<Form>& a_distributables, const size_t a_totalNPCCount)
	{
		if (a_distributables) {
			logger::info("\t{}", a_recordType);

			// Group the same entries together to show total number of distributed records in the log.
			std::map<RE::FormID, Forms::Data<Form>> sums{};
			for (auto& formData : a_distributables.forms) {
				if (const auto& form = formData.form) {
					auto it = sums.find(form->GetFormID());
					if (it != sums.end()) {
						it->second.npcCount += formData.npcCount;
					} else {
						sums.insert({ form->GetFormID(), formData });
					}
				}
			}

			for (auto& entry : sums) {
				auto& formData = entry.second;
				if (const auto& form = formData.form) {
					std::string name{};
					if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
						name = form->GetFormEditorID();
					} else {
						name = Cache::EditorID::GetEditorID(form);
					}
					if (auto file = form->GetFile(0)) {
						logger::info("\t\t{} [0x{:X}~{}] added to {}/{} NPCs", name, form->GetLocalFormID(), file->GetFilename(), formData.npcCount, a_totalNPCCount);
					} else {
						logger::info("\t\t{} [0x{:X}] added to {}/{} NPCs", name, form->GetFormID(), formData.npcCount, a_totalNPCCount);
					}
				}
			}
		}
	}

	namespace detail
	{
		bool uses_template(const RE::TESNPC* a_npc);
	}

	namespace Event
	{
		class Manager :
			public RE::BSTEventSink<RE::TESDeathEvent>,
			public RE::BSTEventSink<RE::TESFormDeleteEvent>
		{
		public:
			static Manager* GetSingleton()
			{
				static Manager singleton;
				return &singleton;
			}

			static void Register();

		protected:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override;
			RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

		private:
			Manager() = default;
			Manager(const Manager&) = delete;
			Manager(Manager&&) = delete;

			~Manager() override = default;

			Manager& operator=(const Manager&) = delete;
			Manager& operator=(Manager&&) = delete;
		};
	}

	namespace LeveledActor
	{
		void Install();
	}

	void Distribute(const NPCData& a_npcData, const PCLevelMult::Input& a_input);

	// Distribute to all unique and static NPCs, after data load
    void OnInit();

	// Distribute to all actors in each processList, when in game
	void InGame(std::function<void(const RE::NiPointer<RE::Actor>&)> a_callback);
}
