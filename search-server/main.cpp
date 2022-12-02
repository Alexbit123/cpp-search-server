/*
Скажите пожалуйста, я понимаю что в авторском решении скорее всего хороший код и по памяти и по времени, почему не стоит после каждого решения просто брать код оттуда? :)
А ещё к примеру у меня count в подсчёте TF был не очень (word_to_document_freqs_[i].insert({ document_id, (double)count(words.begin(), words.end(), i) / words.size() });),
если было бы 8 одинаковых слов, то он на каждой итерации перезаписывал одно и то же слово и его TF.
Но если бы я добавлял каждое слово в set и сравнивал бы, встречалось ли слово, если встречалось пропустить итерацию и идти к след.слову.
Так получается и решение моё, и позволяет не ходить по тем же словам несколько раз. Но я не понимаю, что было бы быстрее, пройти просто по всем словам и суммировать TF.
Либо каждый раз вызывать операцию count, а к примеру слов миллион, и каждый раз он рыщет в поисках вхождений слов. А так же к примеру все миллион слов одинаковые,
count безусловно быстрее обычного линейного подсчета, или не быстрее?
O(n) - сложность алгоритма подсчета TF сейчас. (где n - количество итераций цикла for)
O(n*m) - сложность алгоритма с count. (где n - количество итераций цикла for, а m количество итераций пробежки по вектору операцией count)
Правильно? Получается не быстрее через count в любом случае :)
*/


#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <math.h>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {													//Функция чтение строки
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber() {											//Функция чтения количества строк
	int result = 0;
	cin >> result;
	ReadLine();
	return result;
}

vector<string> SplitIntoWords(const string& text) {					//Функция разделения строки на слова
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
	int id;
	double relevance;
};



class SearchServer {
public:
	void get(const int document_count) {									//Метод изменения поля document_count_
		document_count_ = document_count;
	}
	void SetStopWords(const string& text) {									//Метод изменения поля stop_words_ (добавление в контейнер set стоп-слов)
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	void AddDocument(int document_id, const string& document) {				//Метод изменения поля word_to_document_freqs_ (добавление в контейнер map документов)
		const vector<string> words = SplitIntoWordsNoStop(document);
		const double inv_word_count = 1.0 / words.size();
		for (const auto& word : words) {
			word_to_document_freqs_[word][document_id] += inv_word_count;   //Добавляю и суммирую TF одинаковых слов, вот и нашли TF
		}

	}

	vector<Document> FindTopDocuments(const string& raw_query) const {      //Константный метод возврата топ 5 запросов по релевантности
		const Query query_words = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query_words);

		sort(matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				return lhs.relevance > rhs.relevance;
			});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}

private:

	int document_count_ = 0;
	set<string> stop_words_;
	map<string, map<int, double>> word_to_document_freqs_;

	struct Query {															//Структура множеств плюс-слов и минус-слов
		set<string> plus_words;
		set<string> minus_words;
	};

	bool IsStopWord(const string& word) const {								//Константный метод проверки на стоп слово
		return stop_words_.count(word) > 0;
	}

	double ComputeWordInverseDocumentFreq(const string& word) const {
		return log(static_cast<double>(document_count_) / word_to_document_freqs_.at(word).size());
	}

	vector<string> SplitIntoWordsNoStop(const string& text) const {			//Константный метод возвращающий вектор слов без стоп-слов
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return words;
	}

	Query ParseQuery(const string& text) const {							//Константный метод заполнения структуры множеств плюс-слов и минус-слов
		Query query_words;
		for (const string& word : SplitIntoWordsNoStop(text)) {
			if (word[0] == '-') {
				query_words.minus_words.insert(word.substr(1));
			}
			else query_words.plus_words.insert(word);
		}
		return query_words;
	}

	vector<Document> FindAllDocuments(const Query& query_words) const {		//
		vector<Document> matched_documents;
		map<int, double> document_to_relevance;
		for (const auto& word : query_words.plus_words) {       //Берём плюс-слово из запроса
			if (word_to_document_freqs_.count(word)) {			//Ищем его в базе документов
				double IDF = ComputeWordInverseDocumentFreq(word);    //Если нашли считаем IDF слова (Количество док-тов/количество встречаемости слова в документах)
				for (const auto& [doc_id, tf] : word_to_document_freqs_.at(word)) {         //Бегаем по id документов и добавляем в document_to_relevance [id] = relevance
					document_to_relevance[doc_id] += IDF * tf;
				}
			}
		}
		for (const auto& word : query_words.minus_words) {		//Берём минус-слово
			if (!word.empty()) {
				if (word_to_document_freqs_.count(word)) {
					for (const auto& [doc_id, tf] : word_to_document_freqs_.at(word)) {		//Удаляем документ где встречалось слово
						document_to_relevance.erase(doc_id);
					}
				}
			}
		}
		for (const auto& [doc_id, relevance] : document_to_relevance) {
			matched_documents.push_back({ doc_id, relevance });
		}
		return matched_documents;
	}
};

SearchServer CreateSearchServer() {							//Функция инициализации класса
	SearchServer search_server;
	search_server.SetStopWords(ReadLine());
	const int document_count = ReadLineWithNumber();
	search_server.get(document_count);
	for (int document_id = 0; document_id < document_count; ++document_id) {
		search_server.AddDocument(document_id, ReadLine());
	}
	return search_server;
}

int main() {
	setlocale(LC_ALL, "RUS");
	const SearchServer search_server = CreateSearchServer();

	const string query = ReadLine();
	for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
		cout << "{ document_id = "s << document_id << ", "
			<< "relevance = "s << relevance << " }"s << endl;
	}
}