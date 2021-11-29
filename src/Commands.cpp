#include "Commands.h"
#include "Print.h"
#include "storage/WhyDevice.h"

namespace Thurisaz {
	std::map<std::string, Command> commands;

	bool runCommand(Context &context, const std::vector<std::string> &pieces) {
		if (!pieces.empty()) {
			if (commands.count(pieces.front()) == 0)
				return false;
			commands.at(pieces.front())(context, pieces);
			return true;
		} else
			return false;
	}

	void addCommands() {
		commands.try_emplace("drives", 0, 0, [](Context &, const std::vector<std::string> &) {
			printf("Number of drives: %lu\n", WhyDevice::count());
		});

		commands.emplace("count", commands.at("drives"));

		commands.try_emplace("drive", 0, 0, [](Context &context, const std::vector<std::string> &) {
			if (context.selectedDrive == -1)
				strprint("No drive selected.\n");
			else
				printf("Selected drive: %ld\n", context.selectedDrive);
		});

		// commands.try_emplace("
	}
}
