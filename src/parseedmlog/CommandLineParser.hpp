#pragma once

#include <map>
#include <string>
#include <vector>
#include <iostream>

/*
Simple command line parser; works on Windows or Unix-based platforms.
Usage example:


#include "CommandLineParser.hpp"

int main(int argc, char* argv[]) {
    CommandLineParser parser;

    // Define options and flags
    parser.addOption('f', "file", true);
    parser.addOption('v', "verbose", false);
    parser.addOption('h', "help", false);

    if (!parser.parse(argc, argv)) {
        return 1; // parsing error
    }

    if (parser.hasFlag("help")) {
        std::cout << "Usage:\n"
                  << "  -f, --file <filename>    Input file\n"
                  << "  -v, --verbose            Verbose mode\n"
                  << "  -h, --help               Show help\n";
        return 0;
    }

    if (!parser.hasOption("file")) {
        std::cerr << "Error: --file or -f is required.\n";
        return 1;
    }

    std::string filename = parser.getOption("file");
    std::cout << "File: " << filename << "\n";

    if (parser.hasFlag("verbose")) {
        std::cout << "Verbose mode enabled\n";
    }

    return 0;
}
*/

class CommandLineParser {
public:
    CommandLineParser() = default;

    void addOption(char shortOpt, const std::string& longOpt, bool requiresValue = true) {
        shortToLong[shortOpt] = longOpt;
        optionRequiresValue[longOpt] = requiresValue;
    }

    bool parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg.rfind("--", 0) == 0) {
                // Long option
                size_t eq = arg.find('=');
                std::string key, value;
                if (eq != std::string::npos) {
                    key = arg.substr(2, eq - 2);
                    value = arg.substr(eq + 1);
                    options[key] = value;
                } else {
                    key = arg.substr(2);
                    if (optionRequiresValue[key]) {
                        if ((i + 1) < argc && argv[i + 1][0] != '-') {
                            options[key] = argv[++i];
                        } else {
                            std::cerr << "Missing value for option: --" << key << "\n";
                            return false;
                        }
                    } else {
                        flags[key] = true;
                    }
                }
            } else if (arg.rfind("-", 0) == 0 && arg.length() == 2) {
                // Short option
                char shortKey = arg[1];
                if (shortToLong.count(shortKey)) {
                    std::string key = shortToLong[shortKey];
                    if (optionRequiresValue[key]) {
                        if ((i + 1) < argc && argv[i + 1][0] != '-') {
                            options[key] = argv[++i];
                        } else {
                            std::cerr << "Missing value for option: -" << shortKey << "\n";
                            return false;
                        }
                    } else {
                        flags[key] = true;
                    }
                } else {
                    std::cerr << "Unknown option: -" << shortKey << "\n";
                    return false;
                }
            } else {
                std::cerr << "Unknown argument: " << arg << "\n";
                return false;
            }
        }

        return true;
    }

    bool hasFlag(const std::string& name) const {
        return flags.count(name) > 0;
    }

    bool hasOption(const std::string& name) const {
        return options.count(name) > 0;
    }

    std::string getOption(const std::string& name, const std::string& defaultValue = "") const {
        auto it = options.find(name);
        return it != options.end() ? it->second : defaultValue;
    }

private:
    std::map<char, std::string> shortToLong;
    std::map<std::string, bool> optionRequiresValue;
    std::map<std::string, std::string> options;
    std::map<std::string, bool> flags;
};

