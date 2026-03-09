#include "config.hpp"

#include <optional>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cctype>
#include <regex>
#include <atomic>

extern std::atomic<INT> g_IdxAdapterWithMostOutputs;

void LogLine(
	const std::wstring& wsLine
) {
	static constexpr LPCWSTR LOG_FILE_PATH = L"madmaxcrashfix.log";

	FILE* pLogFile = nullptr;
	errno_t err = _wfopen_s(&pLogFile, LOG_FILE_PATH, L"a");
	if (err != 0 || pLogFile == nullptr) {
		wprintf(
			L"Failed to open log file %s for appending\n",
			LOG_FILE_PATH
		);
		return;
	}

	fwprintf(pLogFile, L"%s\n", wsLine.c_str());
	fclose(pLogFile);
}

namespace LocalConfig {
	namespace ParserUtils {
		/// <summary>
		///  Reads the entire contents of a file into a string.
		/// </summary>
		/// <param name="sFilePath"></param>
		/// <returns>
		///   File contents as string on success, empty string on failure.
		/// </returns>
		std::string ReadFile(
			const std::string& sFilePath
		) {
			std::ifstream ifStream(sFilePath, std::ios::binary);
			if (!ifStream) {
				return {};
			}

			std::ostringstream ossBuffer;
			ossBuffer << ifStream.rdbuf();
			return ossBuffer.str();
		}

		/// <summary>
		///  Parses a DWORD32 value from a JSON-like string based on the provided key.
		/// </summary>
		/// <param name="sSource"></param>
		/// <param name="sKey"></param>
		/// <returns>
		///   DWORD32 value on success, std::nullopt on failure.
		/// </returns>
		std::optional<DWORD32> ParseDword32(
			const std::string& sSource,
			const std::string& sKey
		) {
			const std::regex regExp("\"" + sKey + "\"\\s*:\\s*(\\d+)", std::regex::icase);
			std::smatch match;
			if (!std::regex_search(sSource, match, regExp) || match.size() < 2) {
				return std::nullopt;
			}

			try {
				return static_cast<DWORD32>(std::stoul(match[1].str()));
			} catch (const std::exception&) {
				return std::nullopt;
			}
		}
	} // namespace ParserUtils

	/// <summary>
	///  Creates a default config based on the current display settings. 
	///  If it fails to get the display settings, it falls back to 1920x1080.
	///  Also writes the default config to file.
	/// </summary>
	/// <param name=""></param>
	/// <returns>
	///  Initialized Config struct with display width and height.
	/// </returns>
	Config CreateDefaultConfig(void) {
		Config defaultConfig{};

		DEVMODEA devMode{};
		devMode.dmSize = sizeof(DEVMODEA);
		if (EnumDisplaySettingsA(
			nullptr,
			ENUM_CURRENT_SETTINGS,
			&devMode
		)) {
			defaultConfig.dwDisplayWidth = devMode.dmPelsWidth;
			defaultConfig.dwDisplayHeight = devMode.dmPelsHeight;
		} else {
			// fallback to 1080p
			defaultConfig.dwDisplayWidth = 1920;
			defaultConfig.dwDisplayHeight = 1080;
		}

		if (-1 == g_IdxAdapterWithMostOutputs.load()) {
			LogLine(L"Failed to determine adapter with most outputs, defaulting to 0");
            g_IdxAdapterWithMostOutputs.store(0);
		}

		// write it cuz we're nice
		std::ofstream ofs(LocalConfig::CONFIG_FILE_PATH, std::ios::trunc);
		if (ofs) {
			ofs << "{\n"
				<< "  \"displayWidth\": " << defaultConfig.dwDisplayWidth << ",\n"
				<< "  \"displayHeight\": " << defaultConfig.dwDisplayHeight << ",\n"
                << "  \"adapterIdx\": " << g_IdxAdapterWithMostOutputs.load() << "\n"
				<< "}\n";
			ofs.close();
		}

		return defaultConfig;
	}

	/// <summary>
	///  Loads resolution config file.
	/// </summary>
	/// <param name=""></param>
	/// <returns>
	///  Initialized Config struct with parsed data, or fallback values on failure.
	/// </returns>
	Config LoadConfig(void) {
		Config config{};
		const std::string sSource = ParserUtils::ReadFile(CONFIG_FILE_PATH);

		if (sSource.empty()) {
			LogLine(L"Failed to read config file, using default config");
			return CreateDefaultConfig();
		}

		std::optional<DWORD32> dwWidth = ParserUtils::ParseDword32(sSource, "displayWidth");
		std::optional<DWORD32> dwHeight = ParserUtils::ParseDword32(sSource, "displayHeight");
        std::optional<DWORD32> adapterIdx = ParserUtils::ParseDword32(sSource, "adapterIdx");

		if (!dwWidth.has_value() || !dwHeight.has_value() || !adapterIdx.has_value()) {
			LogLine(
				L"Failed to parse one of the required values from config file, using default config"
			);
			return CreateDefaultConfig();
		}

		config.dwDisplayWidth = dwWidth.value();
		config.dwDisplayHeight = dwHeight.value();
        config.dwAdapterIdx = adapterIdx.value();

		return config;
	}
} // namespace LocalConfig


