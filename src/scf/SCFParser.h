// src/SCF/SCFParser.h

#pragma once
#include "SCFNode.h"
#include <fstream>
#include <iostream>

class SCFParser {

public:
	std::shared_ptr<SCFNode> parse(const std::string& filename) {

		std::ifstream file(filename);
		if (!file.is_open()) {
			std::cerr << "SCF: failed to open " << filename << "\n";
			return nullptr;
		}

		auto root = std::make_shared<SCFNode>();
		root->name = "root";

		std::vector<SCFNode*>	stack	= { root.get() };
		std::vector<int>		indents = { -1 };

		std::string line;
		
		
		while (std::getline(file, line)) {
			
			if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments
			
			// Count leading spaces
			int indent = 0;
			while (indent < (int)line.size() && (line[indent] == ' ' || line[indent] == '\t'))
				indent++;
			// Trim line
			line = line.substr(indent);


			if (line.empty())
				continue; // Skip lines that are empty after trimming


			size_t spacePos = line.find_first_of(" \t");
			std::string nodeName = line.substr(0, spacePos);
			if (nodeName.empty()) continue;
			std::string rest = (spacePos != std::string::npos) ? line.substr(spacePos + 1) : "";

			size_t start = rest.find_first_not_of(" \t");
			rest = (start != std::string::npos) ? rest.substr(start) : "";

			SCFValue val;
			val.freePass = rest;
			if (!rest.empty() && rest != "none") {
				std::stringstream ss(rest);
				std::string part;
				bool allNumeric = true;
				std::vector<float> nums;

				while (std::getline(ss, part, ',')) {

					size_t s = part.find_first_not_of(" \t");
					size_t e = part.find_last_not_of(" \t");
					part = (s != std::string::npos && e != std::string::npos) ? part.substr(s, e - s + 1) : "";

					try {
						nums.push_back(std::stof(part));
					}
					catch (...) {
						allNumeric = false;
						break;
					}
				}

				if (allNumeric && !nums.empty()) {
					val.fixedPass = nums;
				}
			}

			while (indents.size() > 1 && indent <= indents.back()) {
				indents.pop_back();
				stack.pop_back();
			}

			// create + attach node
			auto node = std::make_shared<SCFNode>();
			node->name = nodeName;
			node->value = val;
			stack.back()->children.push_back(node);

			// push
			stack.push_back(node.get());
			indents.push_back(indent);
		}

		return root;
			
	
	}

};