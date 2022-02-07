/*------------------------------------------------------------
 * Filename: reader.hpp
 * Author: Joshua Dahl
 * Email: joshuadahl@nevada.unr.edu
 * Created: 2/7/22
 * Modified: 2/7/22
 * Description: C++ wrapper around linenoise (a console reader and history provider)
 *------------------------------------------------------------*/

#ifndef LINENOISE_HPP
#define LINENOISE_HPP

#include <filesystem>
#include <sstream>
extern "C" {
#	include <linenoise.h>
}

// Console input reader with support for history
class Reader {
	// Macro defining the default maximum length of a history entry
	constexpr static size_t defaultHistoryLength = 2048;

	// Path to where history is saved
	const std::filesystem::path historyPath = absolute(std::filesystem::temp_directory_path() / std::filesystem::path(std::tmpnam(nullptr)));

	// Input prompt
	std::string prompt;
	// Whether input should be masked
	bool maskMode = false;
	// Weather input should be in multiline mode
	bool multilineMode = false;

	// Length of a history entry
	size_t historyLength = 0;

public:
	// Set the prompt
	Reader& setPrompt(const std::string_view p){ prompt = p; return *this; }

	// Set the masking mode
	enum MaskMode { Plain = 0, Default = 0, Masked };
	Reader& setMaskMode(MaskMode mode) { maskMode = (mode == Masked); return *this; }
	Reader& setMaskMode(bool enable) { maskMode = enable; return *this; }

	// Set multiline mode
	Reader& setMultilineMode(bool enable) { multilineMode = enable; return *this; }

	// Constructor will set a default history length if
	Reader(bool enableHistory = false) { if(enableHistory) setHistoryEntryLength(defaultHistoryLength); }

	// Read a line from the console, appending it to the history if appropriate
	std::string read(bool addToHistory = true){
		// Enable or disable masking
		if(maskMode) linenoiseMaskModeEnable();
		else linenoiseMaskModeDisable();
		// Enable or disable multiline mode
		linenoiseSetMultiLine(multilineMode ? 1 : 0);

		// Read a line from the console and add it to the history
		char* input = linenoise(prompt.c_str());
		if(addToHistory) appendToHistory(input);

		// Return a copy of the string and free the original buffer
		auto out = std::string(input);
		linenoiseFree(input);
		return out;
	}

	// Check if history is currently enabled
	bool historyEnabled() { return historyLength; }

	// Add a line to the history
	Reader& appendToHistory(const std::string_view line) {
		if(!historyEnabled()) return *this;

		// Load the current history from file, add the new line, and save it
		if(exists(historyPath)) linenoiseHistoryLoad(historyPath.c_str());
		linenoiseHistoryAdd(line.data());
		linenoiseHistorySave(historyPath.c_str());
		return *this;
	}

	// Save the history to a file
	Reader& saveHistory(std::filesystem::path path) {
		if(!historyEnabled()) return *this;

		// Load our history, and save it to the specified file
		if(exists(historyPath)) linenoiseHistoryLoad(historyPath.c_str());
		linenoiseHistorySave(path.c_str());
		return *this;
	}

	// Load the history from the file
	Reader& loadHistory(std::filesystem::path path) {
		if(!historyEnabled()) return *this;

		// Load the new history, and save it to our temp file
		if(exists(path)) linenoiseHistoryLoad(path.c_str());
		linenoiseHistorySave(historyPath.c_str());
		return *this;
	}

	// Set the length of a history entry (0 will disable history)
	Reader& setHistoryEntryLength(size_t length = defaultHistoryLength) {
		historyLength = length;
		linenoiseHistorySetMaxLen(length);
		return *this;
	}

	// Clear the screen
	static void clearScreen() { linenoiseClearScreen(); }
};

#endif // LINENOISE_HPP