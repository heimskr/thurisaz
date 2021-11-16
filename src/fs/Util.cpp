#include <list>
#include <string>

#include "fs/FS.h"
#include "util.h"
#include "printf.h"

namespace FS {
	std::string simplifyPath(std::string cwd, std::string path) {
		if (cwd.empty())
			cwd = "/";
		
		if (path.empty())
			return cwd;
		
		if (path.front() != '/') {
			if (cwd.back() == '/')
				path = cwd + path;
			else
				path = cwd + '/' + path;
		}

		std::list<std::string> pieces = split<std::list>(path, "/", true);

		auto iter = pieces.begin();
		while (iter != pieces.end()) {
			const std::string &piece = *iter;
			if (piece.empty() || piece == "." || (piece == ".." && iter == pieces.begin())) {
				auto next = iter;
				++next;
				if (next == pieces.end()) {
					pieces.erase(iter);
					break;
				} else {
					pieces.erase(iter);
					iter = next;
				}
			} else if (piece == "..") {
				auto prev = iter, next = iter;
				--prev;
				++next;
				pieces.erase(prev);
				pieces.erase(iter);
				iter = next;
			} else
				++iter;
		}

		if (pieces.empty())
			return "/";

		path.clear();

		for (const std::string &piece: pieces)
			path += "/" + piece;

		return path;
	}

	std::string simplifyPath(std::string path) {
		if (path.empty())
			return "/";

		std::list<std::string> pieces = split<std::list>(path, "/", true);

		auto iter = pieces.begin();
		while (iter != pieces.end()) {
			const std::string &piece = *iter;
			if (piece.empty() || piece == "." || (piece == ".." && iter == pieces.begin())) {
				auto next = iter;
				++next;
				if (next == pieces.end()) {
					pieces.erase(iter);
					break;
				} else {
					pieces.erase(iter);
					iter = next;
				}
			} else if (piece == "..") {
				auto prev = iter, next = iter;
				--prev;
				++next;
				pieces.erase(prev);
				pieces.erase(iter);
				iter = next;
			} else
				++iter;
		}

		if (pieces.empty())
			return "/";

		path.clear();

		for (const std::string &piece: pieces)
			path += "/" + piece;

		return path;
	}
}
