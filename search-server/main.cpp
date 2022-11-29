// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <iostream>

const unsigned short int N = 1000;

int main()
{
    unsigned short int count = 0;
    for (unsigned short int i = 1; i <= N; ++i) {
        if (i < 10) {
            if (i == 3) {
                ++count;
            }
            continue;
        }
        if (i >= 10 && i < 100) {
            if (i % 10 == 3 || i / 10 == 3) {
                ++count;
            }
            continue;

        }
        if (i >= 100) {
            if (i / 100 == 3 || (i % 100) % 10 == 3 || (i % 100) / 10 == 3) {
                ++count;
            }
            continue;

        }
    }
    std::cout << count;
}
// Закомитьте изменения и отправьте их в свой репозиторий.
