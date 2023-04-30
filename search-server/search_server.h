#pragma once
#include "string_processing.h"
#include "concurrent_map.h"
#include <utility>
#include <algorithm>
#include <tuple>
#include <map>
#include <cmath>
#include <numeric>
#include <execution>
#include <list>
#include <future>
#include <deque>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr auto EPS = 1e-6;
constexpr size_t THREAD_COUNT = 6;

using TapleWordsStatus = std::tuple<std::vector<std::string_view>, DocumentStatus>;

class SearchServer {
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);
	explicit SearchServer(std::string stop_words_text);
	explicit SearchServer(std::string_view stop_words_text);

	void AddDocument(int document_id, std::string_view document, DocumentStatus status,
		const std::vector<int>& ratings);

	void RemoveDocument(int document_id);
	void RemoveDocument(std::execution::sequenced_policy parallel, int document_id);
	void RemoveDocument(std::execution::parallel_policy parallel, int document_id);

	template <typename DocumentPredicate, typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query,
		DocumentPredicate document_predicate) const;
	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query,
		DocumentPredicate document_predicate) const;
	template <typename ExecutionPolicy>

	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const;
	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

	template <typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const;
	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

	int GetDocumentCount() const;

	std::set<int, std::map<std::string, double>>::const_iterator begin() const;

	std::set<int, std::map<std::string, double>>::const_iterator end() const;

	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

	TapleWordsStatus MatchDocument(std::string_view raw_query,
		int document_id) const;
	TapleWordsStatus MatchDocument(std::execution::sequenced_policy parallel,
		std::string_view raw_query,
		int document_id) const;
	TapleWordsStatus MatchDocument(std::execution::parallel_policy parallel,
		std::string_view raw_query,
		int document_id) const;
private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
		
	};
	
	const std::set<std::string, std::less<>> stop_words_;
	std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
	std::map<int, DocumentData> documents_;
	std::map<int, std::map<std::string_view, double>> document_ids_freqs_;
	std::set<int> document_ids_;
	std::deque<std::string> words;

	bool IsStopWord(std::string_view word) const;

	static bool IsValidWord(std::string_view word);

	std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(std::string_view text) const;


	struct Query {
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	Query ParseQuery(std::string_view text, bool remove_duplicates) const;

	// Existence required
	double ComputeWordInverseDocumentFreq(std::string_view word) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query,
		DocumentPredicate document_predicate) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const Query& query,
		DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
	: stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
	using namespace std::string_literals;
	if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
		throw std::invalid_argument("Some of stop words are invalid"s);
	}
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
	DocumentPredicate document_predicate) const {
	return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query,
	DocumentPredicate document_predicate) const {
	const auto query = ParseQuery(raw_query, true);

	auto matched_documents = FindAllDocuments(policy, query, document_predicate);

	sort(policy, matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
			return lhs.relevance > rhs.relevance
				|| (std::abs(lhs.relevance - rhs.relevance) < EPS && lhs.rating > rhs.rating);
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}
template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const {
	return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(
		policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status;
		});
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query,
	DocumentPredicate document_predicate) const {
	ConcurrentMap<int, double> document_to_relevance(THREAD_COUNT);
	for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
		[&](auto& word) {
			if (word_to_document_freqs_.count(word) != 0) {
				const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
				for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
					const auto& document_data = documents_.at(document_id);
					if (document_predicate(document_id, document_data.status, document_data.rating)) {
						document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
					}
				}
			}
		});
	for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
		[&](auto& word) {
			if (word_to_document_freqs_.count(word) != 0) {
				for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
					document_to_relevance.erase(document_id);
				}
			}
		});

	auto result = document_to_relevance.BuildOrdinaryMap();

	std::vector<Document> matched_documents;
	for (const auto [document_id, relevance] : result) {
		matched_documents.push_back(
			{ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const Query& query,
	DocumentPredicate document_predicate) const {
	std::map<int, double> document_to_relevance;
	for (auto word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
		for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
			const auto& document_data = documents_.at(document_id);
			if (document_predicate(document_id, document_data.status, document_data.rating)) {
				document_to_relevance[document_id] += term_freq * inverse_document_freq;
			}
		}
	}
	for (auto word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
			document_to_relevance.erase(document_id);
		}
	}

	std::vector<Document> matched_documents;
	for (const auto [document_id, relevance] : document_to_relevance) {
		matched_documents.push_back(
			{ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}

void AddDocument(SearchServer& search_server, int document_id, std::string_view document,
	DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, std::string_view raw_query);

void MatchDocuments(const SearchServer& search_server, std::string_view query);
