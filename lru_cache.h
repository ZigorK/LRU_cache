/**
 * @file lru_cache.h
 * @author Игорь Заикин (ikz3@tpu.ru)
 * @brief Реализация приблизительного LRU-кэша с поддержкой многопоточности
 * @version 0.1
 * @date 2026-02-25
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include <unordered_map>
#include <list>
#include <shared_mutex>
#include <atomic>
#include <stdexcept>
#include <algorithm>
#include <mutex>

/**
 * @brief LRU-кэш с приблизительным вытеснением 
 * 
 * @tparam Key тип ключа, может быть любым
 * @tparam Value тип значения, может быть любым
 * 
 * @details Реализует алгоритм приблизительного LRU: 
 *          чтение под shared_lock (атомарное обновление времени),
 *          время измеряется в операциях,
 *          список не перестраивается при чтении.
 *          Вытеснение: проверка последних 4 элементов, удаление самого старого из них.
 */
template<typename Key, typename Value>
class LRUCache
{
private:
    struct Entry {
        Value value; ///< хранимые данные 
        std::atomic<uint64_t> last_access; ///< время последнего доступа (атомарное)

        /**
         * @brief Конструктор с перемещением значения
         * 
         * @param v значение
         * @param ts временная метка
         */
        Entry(Value v, uint64_t ts) : value(std::move(v)), last_access(ts) {}

        // Запрет копирования из-за std::atomic
        Entry(const Entry&) = delete; 
        Entry& operator=(const Entry&) = delete;

        /**
         * @brief Конструктор перемещения
         * 
         * @param other 
         */
        Entry(Entry&& other) noexcept : value(std::move(other.value)),
              last_access(other.last_access.load()) {}

        /**
         * @brief Оператор перемещающего присваивания
         * 
         * @param other 
         * @return Entry& 
         */
        Entry& operator=(Entry&& other) noexcept 
        {
            value = std::move(other.value);
            last_access.store(other.last_access.load());
            return *this;
        }
    };

    using ListType = std::list<std::pair<Key, Entry>>; ///< тип списка элементов
    using Iterator = typename ListType::iterator; ///< итератор на элемент списка

    size_t m_capacity; ///< максимальный размер кэша
    ListType m_list; ///< список элементов в порядке вставки
    std::unordered_map<Key, Iterator> m_map; ///< хэщ-таблица для быстрого доступа
    mutable std::shared_mutex m_mutex; ///< мьтекс для синхронизации
    std::atomic<uint64_t> m_clock{0}; ///< глобальный счетчик времени

    static constexpr size_t TAIL_LENGTH = 4; ///< сколько последних элементов проверяем

    /**
     * @brief Получение текущего "времени", увеличение счетчика
     * 
     * @return uint64_t новое значение счетчика
     */
    uint64_t now() noexcept
    {
        return ++m_clock;
    }

    /**
     * @brief Проталкивание свежих элементов из хвоста в голову
     * 
     * @details Проверяет до TAIL_LENGHT элементов в хвосте.
     *          Если элемент сввежий, то перемещает его в голову,
     *          если встретил старый - останавливается.
     */
    void maintenance()
    {
        if (m_list.empty())
            return;

        size_t check_count = std::min(TAIL_LENGTH, m_list.size());
        uint64_t threshold = m_clock.load(std::memory_order_relaxed) - 1000;

        for (size_t i = 0; i < check_count; ++i)
        {
            auto tail_it = std::prev(m_list.end());

            uint64_t tail_time =
                tail_it->second.last_access.load(std::memory_order_relaxed);

            if (tail_time > threshold)
            {
                m_list.splice(m_list.begin(), m_list, tail_it);
            }
            else
            {
                break;
            }
        }
    }

    /**
     * @brief Поиск самого старого элемента среди последних TAIL_LENGHT
     * 
     * @return Iterator на самый старый элемент
     */
    Iterator find_oldest_in_tail()
    {
        size_t check_count = std::min(TAIL_LENGTH, m_list.size());
        auto it = m_list.end();
        std::advance(it, -static_cast<long>(check_count));

        Iterator oldest = it;
        uint64_t oldest_time =
            it->second.last_access.load(std::memory_order_relaxed);

        for (; it != m_list.end(); ++it)
        {
            uint64_t t =
                it->second.last_access.load(std::memory_order_relaxed);

            if (t < oldest_time)
            {
                oldest_time = t;
                oldest = it;
            }
        }

        return oldest;
    }

public:
    /**
     * @brief Конструктор кэша
     * 
     * @param capacity максимальный размер (1-10000)
     * @throws std::invalid_argument 
     */
    explicit LRUCache(size_t capacity)
        : m_capacity(capacity)
    {
        if (capacity == 0)
            throw std::invalid_argument("Размер кэша не может быть 0");
        if (capacity > 10000)
            throw std::invalid_argument("Размер кэша не может быть больше 10000");
    }

    /**
     * @brief Вставка и обновление элемента
     * 
     * @param key ключ
     * @param value значение
     * 
     * @details Если ключ существует - обновляет значение и перемещает в голову.
     *          Если ключа нет и кэш полон - запускает maintenance,
     *          находит самый старый элемент и удаляет. Новый элемент вставляется в голову.
     */
    void insert(Key key, Value value)
    {
        std::unique_lock lock(m_mutex);

        auto it = m_map.find(key);

        if (it != m_map.end())
        {
            it->second->second.value = std::move(value);
            it->second->second.last_access.store(
                now(), std::memory_order_relaxed);
            m_list.splice(m_list.begin(), m_list, it->second);
            return;
        }

        if (m_list.size() >= m_capacity)
        {
            maintenance();
            auto victim = find_oldest_in_tail();

            m_map.erase(victim->first);
            m_list.erase(victim);
        }

        m_list.emplace_front(
            std::move(key),
            Entry(std::move(value), now()));

        m_map[m_list.front().first] = m_list.begin();
    }

    /**
     * @brief Получение элемента по ключу
     * 
     * @param key ключ
     * @return const Value& ссылка на значение, без копирования
     * @throws std::out_of_range если ключ не найден
     * 
     * @details Работает под shared_lock - множество читателей. 
     *          Атомарно обновляет время доступа, список не перестраивает.
     */
    const Value& get(const Key& key)
    {
        std::shared_lock lock(m_mutex);

        auto it = m_map.find(key);
        if (it == m_map.end())
            throw std::out_of_range("В кэше нет элемента с таким ключом");

        it->second->second.last_access.store(
            now(), std::memory_order_relaxed);

        return it->second->second.value;
    }

    /**
     * @brief Удаление элемента по ключу
     * 
     * @param key ключ
     * @throws std::out_of_range если ключ не найдет
     */
    void remove(const Key& key)
    {
        std::unique_lock lock(m_mutex);

        auto it = m_map.find(key);
        if (it == m_map.end())
            throw std::out_of_range("В кэше нет элемента с таким ключом");

        m_list.erase(it->second);
        m_map.erase(it);
    }

    /**
     * @brief Текущий размер кэша
     * 
     * @return size_t количество элементов
     */
    size_t size() const
    {
        std::shared_lock lock(m_mutex);
        return m_list.size();
    }

    /**
     * @brief Полная очистка кэша
     * 
     */
    void clear()
    {
        std::unique_lock lock(m_mutex);
        m_list.clear();
        m_map.clear();
    }
};