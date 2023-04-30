#include "search_server.h"

using namespace std::string_literals;

SearchServer::SearchServer(std::string stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(std::string_view stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
	const std::vector<int>& ratings) {
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw std::invalid_argument("Invalid document_id"s);
	}

	words.push_back(static_cast<std::string>(document));

	const double inv_word_count = 1.0 / SplitIntoWordsNoStop(words.back()).size();

	for (std::string_view word : SplitIntoWordsNoStop(words.back())) {
		word_to_document_freqs_[word][document_id] += inv_word_count;
		document_ids_freqs_[document_id][word] += inv_word_count;
	}
	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status});
	document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
	return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(
		std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status;
		});
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query,
	int document_id) const {
	const auto query = ParseQuery(raw_query, true);
	
	std::vector<std::string_view> matched_words(query.plus_words.size());

	if (std::any_of(query.minus_words.begin(), query.minus_words.end(),
		[&](auto& word) {
			return word_to_document_freqs_.at(word).count(document_id);
		})) {
		matched_words.clear();
	}
	else {
		auto it = std::copy_if(query.plus_words.begin(), query.plus_words.end(),
			matched_words.begin(), [&](auto& word) {
				return word_to_document_freqs_.at(word).count(document_id);
			});
		matched_words.erase(it, matched_words.end());
	}
	
	return std::tuple(matched_words, documents_.at(document_id).status);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy parallel,
	std::string_view raw_query, int document_id) const {
	return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy parallel,
	std::string_view raw_query, int document_id) const {
	const auto query = ParseQuery(raw_query, false);
	std::vector<std::string_view> matched_words(query.plus_words.size());

	if (std::any_of(parallel, query.minus_words.begin(), query.minus_words.end(),
		[&](auto& word) {
			return word_to_document_freqs_.at(word).count(document_id);
		})) {
		matched_words.clear();
	}
	else {
		auto it = std::copy_if(parallel, query.plus_words.begin(), query.plus_words.end(),
			matched_words.begin(), [&](auto& word) {
				return word_to_document_freqs_.at(word).count(document_id);
			});
		std::sort(parallel, matched_words.begin(), it);
		auto last = std::unique(parallel, matched_words.begin(), it);
		matched_words.erase(last, matched_words.end());
	}

	return std::tuple(matched_words, documents_.at(document_id).status);
}

bool SearchServer::IsStopWord(std::string_view word) const {
	return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
	// A valid word must not contain special characters
	return std::none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
		});
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
	std::vector<std::string_view> words;
	for (std::string_view word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {
			throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
	return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
	if (text.empty()) {
		throw std::invalid_argument("Query word is empty"s);
	}
	//std::string word = text;
	bool is_minus = false;
	if (text[0] == '-') {
		is_minus = true;
		text = text.substr(1);
	}
	if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
		throw std::invalid_argument("Query word "s + static_cast<std::string>(text) + " is invalid");
	}
	return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool remove_duplicates) const {
	Query result;
	for (std::string_view word : SplitIntoWords(text)) {
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				result.minus_words.push_back(query_word.data);
			}
			else {
				result.plus_words.push_back(query_word.data);
			}
		}
	}

	if (remove_duplicates) {
		std::sort(result.minus_words.begin(), result.minus_words.end());
		result.minus_words.erase(std::unique(result.minus_words.begin(),
			result.minus_words.end()), result.minus_words.end());

		std::sort(result.plus_words.begin(), result.plus_words.end());
		result.plus_words.erase(std::unique(result.plus_words.begin(),
			result.plus_words.end()), result.plus_words.end());

	}

	return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
	return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

std::set<int, std::map<std::string, double>>::const_iterator SearchServer::begin() const {
	return document_ids_.begin();
}

std::set<int, std::map<std::string, double>>::const_iterator SearchServer::end() const {
	return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
	static std::map<std::string_view, double> word_frequencies;
	if (document_ids_freqs_.count(document_id)) {
		return document_ids_freqs_.at(document_id);
	}
	return word_frequencies;
}

void SearchServer::RemoveDocument(int document_id) {
	for (auto& [i, container] : word_to_document_freqs_) {
		container.erase(document_id);
	}
	document_ids_freqs_.erase(document_id);
	documents_.erase(document_id);
	document_ids_.erase(document_id);

}

void SearchServer::RemoveDocument(std::execution::sequenced_policy parallel, int document_id) {
	RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy parallel, int document_id) {
	std::vector<const std::string_view*> words_;
	words_.reserve(document_ids_freqs_.count(document_id));
	//std::transform(parallel, document_ids_freqs_.at(document_id).begin(),
	//	document_ids_freqs_.at(document_id).end(), words.begin(), [](auto& buf) {
	//		return &buf.first;
	//	});

	for (auto& [word, _] : document_ids_freqs_.at(document_id)) {
		words_.push_back(&word);
	}
	std::for_each(parallel, words_.begin(), words_.end(),
		[&](auto& word) {
			if (word_to_document_freqs_.count(*word) > 0) {
				word_to_document_freqs_.at(*word).erase(document_id);
			}
		});
	document_ids_freqs_.erase(document_id);
	documents_.erase(document_id);
	document_ids_.erase(document_id);

}

void AddDocument(SearchServer& search_server, int document_id, std::string_view document,
	DocumentStatus status, const std::vector<int>& ratings) {
	try {
		search_server.AddDocument(document_id, document, status, ratings);
	}
	catch (const std::exception& e) {
		std::cout << "Error in adding document "s << document_id << ": "s << e.what() << std::endl;
	}
}

void FindTopDocuments(const SearchServer& search_server, std::string_view raw_query) {
	std::cout << "Results for request: "s << raw_query << std::endl;
	try {
		for (const Document& document : search_server.FindTopDocuments(raw_query)) {
			PrintDocument(document);
		}
	}
	catch (const std::exception& e) {
		std::cout << "Error is seaching: "s << e.what() << std::endl;
	}
}

void MatchDocuments(const SearchServer& search_server, std::string_view query) {
	try {
		std::cout << "Matching for request: "s << query << std::endl;
		for (const int id : search_server) {
			const auto [words, status] = search_server.MatchDocument(query, id);
			PrintMatchDocumentResult(id, words, status);
		}
	}
	catch (const std::exception& e) {
		std::cout << "Error in matchig request "s << static_cast<std::string>(query) << ": "s << e.what() << std::endl;
	}
}
