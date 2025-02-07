#include "Distribute.h"
#include "DistributePCLevelMult.h"

HMODULE kid{ nullptr };
HMODULE tweaks{ nullptr };

bool shouldLookupForms{ false };
bool shouldLogErrors{ false };
bool shouldDistribute{ false };

bool DoDistribute()
{
	if (shouldDistribute = Lookup::GetForms(); shouldDistribute) {
		Distribute::OnInit();
		Distribute::Event::Manager::Register();

	    // Clear logger's buffer to free some memory :)
		logger::clear();

	    return true;
	}

	return false;
}

class DistributionManager : public RE::BSTEventSink<SKSE::ModCallbackEvent>
{
public:
	static DistributionManager* GetSingleton()
	{
		static DistributionManager singleton;
		return &singleton;
	}

protected:
	using EventResult = RE::BSEventNotifyControl;

	EventResult ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
	{
		if (a_event && a_event->eventName == "KID_KeywordDistributionDone") {
			logger::info("{:*^50}", "LOOKUP");
			logger::info("Starting distribution since KID is done...");

			DoDistribute();
			SKSE::GetModCallbackEventSource()->RemoveEventSink(GetSingleton());
		}

		return EventResult::kContinue;
	}

private:
	DistributionManager() = default;
	DistributionManager(const DistributionManager&) = delete;
	DistributionManager(DistributionManager&&) = delete;

	~DistributionManager() override = default;

	DistributionManager& operator=(const DistributionManager&) = delete;
	DistributionManager& operator=(DistributionManager&&) = delete;
};

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case SKSE::MessagingInterface::kPostLoad:
		{
			logger::info("{:*^50}", "DEPENDENCIES");

			kid = GetModuleHandle(L"po3_KeywordItemDistributor");
			logger::info("Keyword Item Distributor (KID) detected : {}", kid != nullptr);

			tweaks = GetModuleHandle(L"po3_Tweaks");
			logger::info("powerofthree's Tweaks (po3_tweaks) detected : {}", tweaks != nullptr);

			if (std::tie(shouldLookupForms, shouldLogErrors) = INI::GetConfigs(); shouldLookupForms) {
				logger::info("{:*^50}", "HOOKS");
				Distribute::LeveledActor::Install();
				Distribute::PlayerLeveledActor::Install();
				if (kid != nullptr) {
					SKSE::GetModCallbackEventSource()->AddEventSink(DistributionManager::GetSingleton());
				}
			}
		}
		break;
	case SKSE::MessagingInterface::kPostPostLoad:
		{
			logger::info("{:*^50}", "MERGES");
			MergeMapperPluginAPI::GetMergeMapperInterface001();  // Request interface
			if (g_mergeMapperInterface) {                        // Use Interface
				const auto version = g_mergeMapperInterface->GetBuildNumber();
				logger::info("Got MergeMapper interface buildnumber {}", version);
			} else
				logger::info("MergeMapper not detected");
		}
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		{
			if (shouldLookupForms) {
				if (kid == nullptr) {
					logger::info("{:*^50}", "LOOKUP");
					DoDistribute();
				}
				logger::info("{:*^50}", "EVENTS");
				PCLevelMult::Manager::GetSingleton()->Register();
				logger::info("{:*^50}", "SAVES");
			}
			if (shouldLogErrors) {
				const auto error = fmt::format("[SPID] Errors found when reading configs. Check {}.log in {} for more info\n", Version::PROJECT, SKSE::log::log_directory()->string());
				RE::ConsoleLog::GetSingleton()->Print(error.c_str());
			}
		}
		break;
	case SKSE::MessagingInterface::kNewGame:
		{
			if (shouldDistribute) {
				PCLevelMult::Manager::GetSingleton()->SetNewGameStarted();
			}
		}
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
		{
			if (shouldDistribute) {
				const std::string savePath{ static_cast<char*>(a_message->data), a_message->dataLen };
				PCLevelMult::Manager::GetSingleton()->GetPlayerIDFromSave(savePath);
			}
		}
		break;
	default:
		break;
	}
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("Spell Perk Item Distributor");
	v.AuthorName("powerofthree");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver <
#ifndef SKYRIMVR
		SKSE::RUNTIME_1_5_39
#else
		SKSE::RUNTIME_VR_1_4_15
#endif
	) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}
#endif

void InitializeLog()
{
	auto path = SKSE::log::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S:%e] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();

	logger::info("Game version : {}", a_skse->RuntimeVersion().string());

	SKSE::Init(a_skse);

	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

	return true;
}
