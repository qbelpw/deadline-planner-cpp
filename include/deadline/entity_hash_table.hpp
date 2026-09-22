#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "deadline/common.hpp"

namespace deadline {

template<typename T>
concept CloneableEntity = requires(const T& value) {
    { value.id() } -> std::same_as<const std::string&>;
    { value.clone() } -> std::same_as<std::unique_ptr<T>>;
    { value.equals(value) } -> std::convertible_to<bool>;
};

template<CloneableEntity T>
class EntityHashTable final {
private:
    struct Node final {
        explicit Node(std::unique_ptr<T> item)
            : value(std::move(item)) {}

        std::unique_ptr<T> value;
        std::unique_ptr<Node> next;
    };

public:
    class ConstIterator final {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        ConstIterator() = default;

        reference operator*() const {
            return *node_->value;
        }

        pointer operator->() const {
            return node_->value.get();
        }

        ConstIterator& operator++() {
            if (node_ != nullptr && node_->next != nullptr) {
                node_ = node_->next.get();
                return *this;
            }
            ++bucket_;
            findNextNode();
            return *this;
        }

        ConstIterator operator++(int) {
            auto previous = *this;
            ++(*this);
            return previous;
        }

        bool operator==(const ConstIterator&) const noexcept = default;

    private:
        friend class EntityHashTable<T>;

        ConstIterator(
            const EntityHashTable* owner,
            const std::size_t bucket,
            const Node* node
        ) noexcept
            : owner_(owner),
              bucket_(bucket),
              node_(node) {}

        void findNextNode() noexcept {
            if (owner_ == nullptr) {
                node_ = nullptr;
                return;
            }
            while (bucket_ < owner_->bucketCount_) {
                node_ = owner_->buckets_[bucket_].get();
                if (node_ != nullptr) {
                    return;
                }
                ++bucket_;
            }
            node_ = nullptr;
        }

        const EntityHashTable* owner_ = nullptr;
        std::size_t bucket_ = 0;
        const Node* node_ = nullptr;
    };

    static constexpr std::size_t defaultBucketCount = 127;

    EntityHashTable()
        : EntityHashTable(defaultBucketCount) {}

    explicit EntityHashTable(const std::size_t bucketCount)
        : bucketCount_(bucketCount),
          buckets_(
              std::make_unique<std::unique_ptr<Node>[]>(bucketCount)
          ) {
        require(
            bucketCount_ > 0,
            "Количество корзин должно быть положительным"
        );
    }

    EntityHashTable(const EntityHashTable& other)
        : EntityHashTable(other.bucketCount_) {
        for (const auto& value : other) {
            add(value.clone());
        }
    }

    EntityHashTable(EntityHashTable&& other) noexcept
        : bucketCount_(std::exchange(other.bucketCount_, 0)),
          size_(std::exchange(other.size_, 0)),
          buckets_(std::move(other.buckets_)) {}

    EntityHashTable& operator=(EntityHashTable other) noexcept {
        swap(other);
        return *this;
    }

    ~EntityHashTable() {
        clear();
    }

    void swap(EntityHashTable& other) noexcept {
        std::swap(bucketCount_, other.bucketCount_);
        std::swap(size_, other.size_);
        buckets_.swap(other.buckets_);
    }

    void add(std::unique_ptr<T> value) {
        require(value != nullptr, "Нельзя добавить пустой объект");
        const auto key = value->id();
        require(
            !contains(key),
            "Объект с таким идентификатором уже существует"
        );
        const auto index = bucketIndex(key);
        auto node = std::make_unique<Node>(std::move(value));
        node->next = std::move(buckets_[index]);
        buckets_[index] = std::move(node);
        ++size_;
    }

    EntityHashTable& operator<<(std::unique_ptr<T> value) {
        add(std::move(value));
        return *this;
    }

    EntityHashTable& operator<<(const T& value) {
        add(value.clone());
        return *this;
    }

    bool remove(const std::string& key) noexcept {
        if (bucketCount_ == 0) {
            return false;
        }
        auto* link = &buckets_[bucketIndex(key)];
        while (*link != nullptr) {
            if ((*link)->value->id() == key) {
                auto removed = std::move(*link);
                *link = std::move(removed->next);
                --size_;
                return true;
            }
            link = &((*link)->next);
        }
        return false;
    }

    void clear() noexcept {
        if (buckets_ == nullptr) {
            size_ = 0;
            return;
        }
        for (std::size_t index = 0; index < bucketCount_; ++index) {
            while (buckets_[index] != nullptr) {
                auto current = std::move(buckets_[index]);
                buckets_[index] = std::move(current->next);
            }
        }
        size_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size_ == 0;
    }

    [[nodiscard]] bool contains(const std::string& key) const noexcept {
        return findNode(key) != nullptr;
    }

    T& at(const std::string& key) {
        auto* node = findNode(key);
        require(node != nullptr, "Объект не найден: " + key);
        return *node->value;
    }

    const T& at(const std::string& key) const {
        const auto* node = findNode(key);
        require(node != nullptr, "Объект не найден: " + key);
        return *node->value;
    }

    T& operator[](const std::string& key) {
        return at(key);
    }

    const T& operator[](const std::string& key) const {
        return at(key);
    }

    [[nodiscard]] ConstIterator begin() const noexcept {
        if (bucketCount_ == 0) {
            return end();
        }
        ConstIterator iterator(this, 0, buckets_[0].get());
        if (iterator.node_ == nullptr) {
            iterator.findNextNode();
        }
        return iterator;
    }

    [[nodiscard]] ConstIterator end() const noexcept {
        return ConstIterator(this, bucketCount_, nullptr);
    }

    [[nodiscard]] bool operator==(const EntityHashTable& other) const noexcept {
        if (size_ != other.size_) {
            return false;
        }
        for (const auto& value : *this) {
            if (!other.contains(value.id())) {
                return false;
            }
            if (!value.equals(other.at(value.id()))) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] EntityHashTable operator&&(
        const EntityHashTable& other
    ) const {
        EntityHashTable result(std::max(bucketCount_, other.bucketCount_));
        for (const auto& value : *this) {
            if (other.contains(value.id())
                && value.equals(other.at(value.id()))) {
                result.add(value.clone());
            }
        }
        return result;
    }

private:
    [[nodiscard]] std::size_t bucketIndex(
        const std::string& key
    ) const noexcept {
        return std::hash<std::string>{}(key) % bucketCount_;
    }

    Node* findNode(const std::string& key) noexcept {
        if (bucketCount_ == 0) {
            return nullptr;
        }
        auto* node = buckets_[bucketIndex(key)].get();
        while (node != nullptr) {
            if (node->value->id() == key) {
                return node;
            }
            node = node->next.get();
        }
        return nullptr;
    }

    const Node* findNode(const std::string& key) const noexcept {
        if (bucketCount_ == 0) {
            return nullptr;
        }
        const auto* node = buckets_[bucketIndex(key)].get();
        while (node != nullptr) {
            if (node->value->id() == key) {
                return node;
            }
            node = node->next.get();
        }
        return nullptr;
    }

    std::size_t bucketCount_ = 0;
    std::size_t size_ = 0;
    std::unique_ptr<std::unique_ptr<Node>[]> buckets_;
};

template<CloneableEntity T>
void swap(EntityHashTable<T>& left, EntityHashTable<T>& right) noexcept {
    left.swap(right);
}

}  // namespace deadline
