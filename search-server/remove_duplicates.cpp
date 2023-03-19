#include <iostream>
#include <algorithm>
#include "remove_duplicates.h"

using namespace std::string_literals;

void RemoveDuplicates(SearchServer& search_server) {
	SearchServer& server = search_server;
	std::set<std::set<std::string>> unique_words;
	std::vector<int> delete_id;
	std::set<int> sort_id_;
	for (const int id : server) {
		sort_id_.insert(id);
	}

	for (const int id : sort_id_) {
		std::set<std::string> words;
		for (auto [word, _] : server.GetWordFrequencies(id)) {
			words.insert(word);
		}
		if (unique_words.count(words)) {
			delete_id.push_back(id);
		}
		else {
			unique_words.insert(words);
		}
		words.clear();
	}
	for (int delete_document_id : delete_id) {
		server.RemoveDocument(delete_document_id);
		std::cout << "Found duplicate document id "s << delete_document_id << std::endl;
	}
}