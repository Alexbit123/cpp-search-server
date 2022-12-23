#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>

using namespace std;

constexpr auto EPS = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber() {
	int result;
	cin >> result;
	ReadLine();
	return result;
}

vector<string> SplitIntoWords(const string& text) {
	vector<string> words;
	string word;
	for (const char c : text) {
		if (c == ' ') {
			if (!word.empty()) {
				words.push_back(word);
				word.clear();
			}
		}
		else {
			word += c;
		}
	}
	if (!word.empty()) {
		words.push_back(word);
	}

	return words;
}

struct Document {
	Document() = default;

	Document(int id, double relevance, int rating)
		: id(id)
		, relevance(relevance)
		, rating(rating) {
	}

	int id = 0;
	double relevance = 0.0;
	int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
	set<string> non_empty_strings;
	for (const string& str : strings) {
		if (!str.empty()) {
			non_empty_strings.insert(str);
		}
	}
	return non_empty_strings;
}

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED,
};

class SearchServer {
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words)
		: stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
		for (const auto& word : stop_words_) {
			if (!IsValidWord(word)) {
				throw invalid_argument("Стоп слова не были добавлены, так как содержат спецсимволы");
			}
		}
	}

	explicit SearchServer(const string& stop_words_text)
		: SearchServer(
			SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
	{
	}

	void AddDocument(int document_id, const string& document, DocumentStatus status,
		const vector<int>& ratings) {
		if (document_id < 0) {
			throw invalid_argument("Попытка добавить документ с отрицательным id;");
		}
		if (GetDocumentId(document_id) >= 0) {
			if (documents_.count(document_id) != 0) {
				throw invalid_argument("Попытка добавить документ c id ранее добавленного документа");
			}
		}
		const vector<string> words = SplitIntoWordsNoStop(document);
		const double inv_word_count = 1.0 / words.size();
		for (const string& word : words) {
			if (IsValidWord(word)) {
				word_to_document_freqs_[word][document_id] += inv_word_count;
			}
			else throw invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа.");
		}
		documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
	}

	template <typename DocumentPredicate>
	vector<Document>  FindTopDocuments(const string& raw_query,
		DocumentPredicate document_predicate) const {
		if (IsValidWord(raw_query)) {
			for (size_t i = 0; i < raw_query.size() - 1; ++i) {
				if (raw_query[i] == '-' && raw_query[i + 1] == '-') {
					throw invalid_argument("Наличие более чем одного минуса перед словами, которых не должно быть в искомых документах");
				}
				if (raw_query[raw_query.size() - 1] == '-') {
					throw invalid_argument("Отсутствие текста после символа «минус» в поисковом запросе");
				}
			}
			const Query query = ParseQuery(raw_query);
			auto matched_documents = FindAllDocuments(query, document_predicate);

			sort(matched_documents.begin(), matched_documents.end(),
				[](const Document& lhs, const Document& rhs) {
					if (abs(lhs.relevance - rhs.relevance) < EPS) {
						return lhs.rating > rhs.rating;
					}
					else {
						return lhs.relevance > rhs.relevance;
					}
				});
			if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
				matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
			}
			return matched_documents;
		}
		else throw invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа.");

	}

	vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
		return FindTopDocuments(
			raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
				return document_status == status;
			});
	}

	vector<Document> FindTopDocuments(const string& raw_query) const {
		return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
	}

	int GetDocumentCount() const {
		return documents_.size();
	}

	int GetDocumentId(int index) const {
		if (GetDocumentCount() >= index && index >= 0) {
			return index;
		}
		else {
			throw out_of_range("Индекс переданного документа выходит за пределы допустимого диапазона");
		}
	}

	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
		int document_id) const {
		if (IsValidWord(raw_query)) {
			for (size_t i = 0; i < raw_query.size() - 1; ++i) {
				if (raw_query[i] == '-' && raw_query[i + 1] == '-') {
					throw invalid_argument("Наличие более чем одного минуса перед словами, которых не должно быть в искомых документах");
				}
				if (raw_query[raw_query.size() - 1] == '-') {
					throw invalid_argument("Отсутствие текста после символа «минус» в поисковом запросе");
				}
			}
			const Query query = ParseQuery(raw_query);
			vector<string> matched_words;
			for (const string& word : query.plus_words) {
				if (word_to_document_freqs_.count(word) == 0) {
					continue;
				}
				if (word_to_document_freqs_.at(word).count(document_id)) {
					matched_words.push_back(word);
				}
			}
			for (const string& word : query.minus_words) {
				if (word_to_document_freqs_.count(word) == 0) {
					continue;
				}
				if (word_to_document_freqs_.at(word).count(document_id)) {
					matched_words.clear();
					break;
				}
			}
			return tuple(matched_words, documents_.at(document_id).status);
		}
		else throw invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа.");

	}

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};
	const set<string> stop_words_;
	map<string, map<int, double>> word_to_document_freqs_;
	map<int, DocumentData> documents_;

	bool IsStopWord(const string& word) const {
		return stop_words_.count(word) > 0;
	}

	static bool IsValidWord(const string& word) {
		// A valid word must not contain special characters
		return none_of(word.begin(), word.end(), [](char c) {
			return c >= '\0' && c < ' ';
			});
	}

	vector<string> SplitIntoWordsNoStop(const string& text) const {
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return words;
	}

	static int ComputeAverageRating(const vector<int>& ratings) {
		if (ratings.empty()) {
			return 0;
		}
		int rating_sum = 0;
		for (const int rating : ratings) {
			rating_sum += rating;
		}
		return rating_sum / static_cast<int>(ratings.size());
	}

	struct QueryWord {
		string data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(string text) const {
		bool is_minus = false;
		// Word shouldn't be empty
		if (text[0] == '-') {
			is_minus = true;
			text = text.substr(1);
		}
		return { text, is_minus, IsStopWord(text) };
	}

	struct Query {
		set<string> plus_words;
		set<string> minus_words;
	};

	Query ParseQuery(const string& text) const {
		Query query;
		for (const string& word : SplitIntoWords(text)) {
			const QueryWord query_word = ParseQueryWord(word);
			if (!query_word.is_stop) {
				if (query_word.is_minus) {
					query.minus_words.insert(query_word.data);
				}
				else {
					query.plus_words.insert(query_word.data);
				}
			}
		}
		return query;
	}

	// Existence required
	double ComputeWordInverseDocumentFreq(const string& word) const {
		return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
	}

	template <typename DocumentPredicate>
	vector<Document> FindAllDocuments(const Query& query,
		DocumentPredicate document_predicate) const {
		map<int, double> document_to_relevance;
		for (const string& word : query.plus_words) {
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

		for (const string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}

		vector<Document> matched_documents;
		for (const auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back(
				{ document_id, relevance, documents_.at(document_id).rating });
		}
		return matched_documents;
	}
};

// ==================== для примера =========================

void PrintDocument(const Document& document) {
	cout << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating << " }"s << endl;
}
int main() {
	setlocale(LC_ALL, "RUS");
	try {
		SearchServer search_server("и в на"s);
		search_server.AddDocument(-3, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
		search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
		search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
		search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });

		search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, { 1, 3, 2 });


		const auto documents = search_server.FindTopDocuments("--пушистый"s);
		for (const Document& document : documents) {
			PrintDocument(document);
		}
	}
	catch (const invalid_argument& err) {
		cout << err.what() << endl;
	}
	catch (const out_of_range& err) {
		cout << err.what() << endl;
	}
	catch (...) {
		cout << "Неизвестное исключение" << endl;
	}
	return 0;
}