#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct WhyDevice;
struct MBR;
struct Partition;

namespace FS {
	class Driver;
}

namespace ThornFAT {
	class ThornFATDriver;
}

namespace Thurisaz {
	struct Context {
		std::unique_ptr<WhyDevice> device;
		std::unique_ptr<MBR> mbr;
		std::unique_ptr<Partition> partition;
		std::unique_ptr<ThornFAT::ThornFATDriver> driver;
		std::string cwd = "/";
		long selectedDrive;
	};

	struct Command {
		int minArgs; // -1 = no minimum
		int maxArgs; // -1 = no maximum
		std::function<void(Context &, const std::vector<std::string> &)> action;
		const char *usage = nullptr;

		Command(int min_args, int max_args, const decltype(action) &action_, const char *usage_ = nullptr):
			minArgs(min_args), maxArgs(max_args), action(action_), usage(usage_) {}
		
		bool valid(int arg_count) const {
			return (minArgs == -1 || minArgs <= arg_count) && (maxArgs == -1 || arg_count <= maxArgs);
		}

		void operator()(Context &context, const std::vector<std::string> &pieces) {
			action(context, pieces);
		}

		Command & setUsage(const char *usage_) {
			usage = usage_;
			return *this;
		}
	};

	extern std::map<std::string, Command> commands;

	bool runCommand(Context &, const std::vector<std::string> &);
	void addCommands();
}
