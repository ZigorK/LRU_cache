/**
 * @file main.cpp
 * @author Игорь Заикин (ikz3@tpu.ru)
 * @brief Тестироваание LRU-кэша
 * @version 0.1
 * @date 2026-02-25
 * 
 * @details Набор тестов для проверки работы LRU-кэша:
 *          модульные тесты базовых операций, тест с разными типами данных, многопоточный стресс-тест
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <iostream>
#include <string>
#include "lru_cache.h"
#include <thread>
#include <vector>
#include <chrono>
#include <random>

/**
 * @brief Многопоточный стресс-тест кэша
 * 
 * @details Запускает 10 потоков (8 читателей и 2 писателя) на 5 секунд.
 *          Размер кэша - 10000 элементов, ключи в диапазоне 0-20000.
 *          Считает количество операций и ошибок.
 * 
 * @note При успешном выполнении other_errors должно быть 0.
 */
void stress_test() {
    std::cout << "СТРЕСС-ТЕСТ МНОГОПОТОЧНОСТИ (10K ЭЛЕМЕНТОВ)" << std::endl;


    LRUCache<int, std::string> cache(10000);
    std::vector<std::thread> threads;
    std::atomic<bool> stop_flag{false};
    std::atomic<int> reads{0};
    std::atomic<int> writes{0};
    
    std::atomic<int> out_of_range_errors{0};
    std::atomic<int> bad_alloc_errors{0};
    std::atomic<int> other_errors{0};

    // Заполняем начальными данными (половина кэша)
    for (int i = 0; i < 5000; ++i) {
        cache.insert(i, "value_" + std::to_string(i));
    }

    /**
     * @brief Генератор случайной задержки
     * @param min минимальное значение в мс
     * @param max максимальное значение в мс
     * @return std::chrono:miilliseconds случайная задержка
     * 
     */
    auto random_ms = [](int min, int max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(min, max);
        return std::chrono::milliseconds(dist(gen));
    };

    // Писатели, 2 потока
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(0, 20000);
            std::uniform_int_distribution<> val_dist(0, 999999);

            while (!stop_flag) {
                try {
                    int key = key_dist(gen);
                    std::string value = "val_" + std::to_string(val_dist(gen));
                    
                    if (key % 3 == 0) {
                        cache.remove(key);
                    } else {
                        cache.insert(key, value);
                    }
                    writes++;
                } catch (const std::out_of_range&) {
                    out_of_range_errors++;
                } catch (const std::exception& e) {
                    other_errors++;
                    std::cerr << "Исключение в писателе: " << e.what() << "\n";
                } catch (...) {
                    other_errors++;
                    std::cerr << "Неизвестное исключение в писателе\n";
                }
                std::this_thread::sleep_for(random_ms(1, 5));
            }
        });
    }

    // Читатели 8 потоков
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(0, 20000);

            while (!stop_flag) {
                try {
                    int key = key_dist(gen);
                    std::string value = cache.get(key);
                    reads++;
                } catch (const std::out_of_range&) {
                    out_of_range_errors++;
                } catch (const std::exception& e) {
                    other_errors++;
                    std::cerr << "Исключение в читателе: " << e.what() << "\n";
                } catch (...) {
                    other_errors++;
                    std::cerr << "Неизвестное исключение в читателе\n";
                }
                std::this_thread::sleep_for(random_ms(0, 2));
            }
        });
    }

    std::cout << "Запущено 10 потоков (8 читателей и 2 писателя)\n";
    std::cout << "Кэш: 10 000 элементов, ключи: 0-20000\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));
    stop_flag = true;

    for (auto& t : threads) {
        t.join();
    }

    // Вывод результатов
    std::cout << "\nРЕЗУЛЬТАТЫ:\n";
    std::cout << "  Чтений:          " << reads << "\n";
    std::cout << "  Записей:         " << writes << "\n";
    std::cout << "  Всего операций:  " << (reads + writes) << "\n\n";
    
    std::cout << "  out_of_range:     " << out_of_range_errors << "\n";
    std::cout << "  bad_alloc:        " << bad_alloc_errors << "\n";
    std::cout << "  other_errors:     " << other_errors << "\n";
    std::cout << "  Итоговый размер:  " << cache.size() << "/10000\n";

    if (bad_alloc_errors == 0 && other_errors == 0) {
        std::cout << "\nМНОГОПОТОЧНОСТЬ РАБОТАЕТ\n";
    } else {
        std::cout << "\nОБНАРУЖЕНЫ ОШИБКИ\n";
    }
}

/**
 * @brief Точка входа в программу
 * 
 * @details Последовательно выполняет:
 *          1) Проверку ограничений размера
 *          2) Базовые операции (вставка, получение, вытеснение)
 *          3) Обновление элемента
 *          4) Удаление элемента
 *          5) Очистку кэша
 *          6) Работу с другими типами данных
 *          7) Многопоточный стресс-тест
 * 
 * @return int 
 */
int main()
{
    std::cout << "     ТЕСТИРОВАНИЕ LRU-КЭША\n";

    std::cout << "[1] Проверка ограничений размера:\n";

    try {
        LRUCache<int, std::string> cache(0);
        std::cout << "Ошибка: размер 0 должен вызывать исключение\n";
    } catch(const std::exception& e) {
        std::cout << "size=0: " << e.what() << "\n";
    }

    try {
        LRUCache<int, std::string> cache(10001);
        std::cout << "Ошибка: размер 10001 должен вызывать исключение\n";
    } catch(const std::exception& e) {
        std::cout << "size=10001: " << e.what() << "\n\n";
    }


    std::cout << "[2] Создание кэша на 3 элемента\n";


    LRUCache<int, std::string> cache(3);

    cache.insert(1, "один");
    cache.insert(2, "два");
    cache.insert(3, "три");

    std::cout << "Размер: " << cache.size() << " (ожидается 3)\n\n";


    std::cout << "[3] Получение элемента\n";


    std::cout << "get(2) = " << cache.get(2) << " (ожидается \"два\")\n\n";


    std::cout << "[4] Проверка вытеснения\n";


    cache.insert(4, "четыре");

    std::cout << "Размер после вставки 4: "
              << cache.size() << " (ожидается 3)\n";

    std::cout << "Проверяем ключ 1: ";
    try {
        std::cout << cache.get(1) << "\n";
        std::cout << "Не вытеснен\n";
    }
    catch (...) {
        std::cout << "вытеснен\n";
    }

    std::cout << "\n";

    std::cout << "[5] Обновление элемента\n";

    std::cout << "До обновления: " << cache.get(2) << "\n";
    cache.insert(2, "ДВА");
    std::cout << "После обновления: " << cache.get(2)
              << " (ожидается \"ДВА\")\n\n";


    std::cout << "[6] Удаление элемента\n";

    size_t before = cache.size();
    cache.remove(2);

    std::cout << "Размер был: " << before
              << ", стал: " << cache.size() << "\n";

    std::cout << "Проверяем ключ 2: ";
    try {
        cache.get(2);
        std::cout << "не удалился\n";
    }
    catch (...) {
        std::cout << "удален\n";
    }

    std::cout << "\n";

    std::cout << "[7] Очистка кэша\n";

    cache.clear();
    std::cout << "Размер после clear(): "
              << cache.size() << " (ожидается 0)\n\n";


    std::cout << "[8] Работа с другими типами (string -> double)\n";

    LRUCache<std::string, double> cache2(2);

    cache2.insert("pi", 3.14159);
    cache2.insert("e", 2.71828);

    std::cout << "pi = " << cache2.get("pi") << "\n";
    std::cout << "e  = " << cache2.get("e") << "\n";

    std::cout << "\nДобавляем tau\n";
    cache2.insert("tau", 6.28318);

    std::cout << "Размер: " << cache2.size() << " (ожидается 2)\n";

    std::cout << "Проверка элементов:\n";

    for (const auto& key : {"pi", "e", "tau"})
    {
        std::cout << "  " << key << ": ";
        try {
            std::cout << cache2.get(key) << "\n";
        }
        catch (...) {
            std::cout << "вытеснен\n";
        }
    }

    std::cout << "ЗАПУСК МНОГОПОТОЧНОГО ТЕСТА\n";


    stress_test();

    return 0;
}