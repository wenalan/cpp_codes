#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <utility>

// Treap (tree + heap) with unique keys. Nodes are ordered by key (BST) and
// prioritized by a random heap key to keep expected logarithmic depth.
// Interface mirrors a minimal subset of std::map needed for benchmarking.
template <typename Key, typename T, typename Compare = std::less<Key>>
class TreapMap {
public:
    TreapMap() : rng_(std::random_device{}()), dist_(0, std::numeric_limits<std::uint32_t>::max()) {}
    ~TreapMap() { clear(); }

    TreapMap(const TreapMap&) = delete;
    TreapMap& operator=(const TreapMap&) = delete;

    TreapMap(TreapMap&& other) noexcept
        : root_(other.root_), size_(other.size_), comp_(std::move(other.comp_)), rng_(std::random_device{}()),
          dist_(0, std::numeric_limits<std::uint32_t>::max()) {
        other.root_ = nullptr;
        other.size_ = 0;
    }

    TreapMap& operator=(TreapMap&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        clear();
        root_ = other.root_;
        size_ = other.size_;
        comp_ = std::move(other.comp_);
        rng_ = std::mt19937(std::random_device{}());
        other.root_ = nullptr;
        other.size_ = 0;
        return *this;
    }

    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }

    template <typename K, typename V>
    bool insert(K&& key, V&& value) {
        bool inserted = false;
        root_ = insert_impl(root_, std::forward<K>(key), std::forward<V>(value), inserted, /*assign_on_match=*/false);
        if (inserted) {
            ++size_;
        }
        return inserted;
    }

    template <typename K, typename V>
    bool insert_or_assign(K&& key, V&& value) {
        bool inserted = false;
        root_ = insert_impl(root_, std::forward<K>(key), std::forward<V>(value), inserted, /*assign_on_match=*/true);
        if (inserted) {
            ++size_;
        }
        return inserted;
    }

    bool erase(const Key& key) {
        bool removed = false;
        root_ = erase_impl(root_, key, removed);
        if (removed) {
            --size_;
        }
        return removed;
    }

    T* find(const Key& key) { return const_cast<T*>(static_cast<const TreapMap*>(this)->find(key)); }

    const T* find(const Key& key) const {
        Node* node = root_;
        while (node) {
            if (comp_(key, node->key)) {
                node = node->left;
            } else if (comp_(node->key, key)) {
                node = node->right;
            } else {
                return &node->value;
            }
        }
        return nullptr;
    }

    bool contains(const Key& key) const { return find(key) != nullptr; }

    void clear() {
        destroy_subtree(root_);
        root_ = nullptr;
        size_ = 0;
    }

private:
    struct Node {
        template <typename K, typename V>
        Node(K&& k, V&& v, std::uint32_t p) : key(std::forward<K>(k)), value(std::forward<V>(v)), priority(p) {}

        Key key;
        T value;
        std::uint32_t priority;
        Node* left = nullptr;
        Node* right = nullptr;
    };

    Node* root_ = nullptr;
    std::size_t size_ = 0;
    Compare comp_{};
    std::mt19937 rng_;
    std::uniform_int_distribution<std::uint32_t> dist_;

    std::uint32_t random_priority() { return dist_(rng_); }

    static Node* rotate_left(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        y->left = x;
        return y;
    }

    static Node* rotate_right(Node* y) {
        Node* x = y->left;
        y->left = x->right;
        x->right = y;
        return x;
    }

    template <typename K, typename V>
    Node* insert_impl(Node* node, K&& key, V&& value, bool& inserted, bool assign_on_match) {
        if (!node) {
            inserted = true;
            return new Node(std::forward<K>(key), std::forward<V>(value), random_priority());
        }

        if (comp_(key, node->key)) {
            node->left = insert_impl(node->left, std::forward<K>(key), std::forward<V>(value), inserted, assign_on_match);
            if (node->left && node->left->priority > node->priority) {
                node = rotate_right(node);
            }
        } else if (comp_(node->key, key)) {
            node->right = insert_impl(node->right, std::forward<K>(key), std::forward<V>(value), inserted, assign_on_match);
            if (node->right && node->right->priority > node->priority) {
                node = rotate_left(node);
            }
        } else {
            if (assign_on_match) {
                node->value = std::forward<V>(value);
            }
            inserted = false;
        }
        return node;
    }

    Node* erase_impl(Node* node, const Key& key, bool& removed) {
        if (!node) {
            return nullptr;
        }

        if (comp_(key, node->key)) {
            node->left = erase_impl(node->left, key, removed);
        } else if (comp_(node->key, key)) {
            node->right = erase_impl(node->right, key, removed);
        } else {
            removed = true;
            Node* merged = merge(node->left, node->right);
            delete node;
            return merged;
        }
        return node;
    }

    static Node* merge(Node* a, Node* b) {
        if (!a) {
            return b;
        }
        if (!b) {
            return a;
        }

        if (a->priority > b->priority) {
            a->right = merge(a->right, b);
            return a;
        } else {
            b->left = merge(a, b->left);
            return b;
        }
    }

    static void destroy_subtree(Node* node) {
        if (!node) {
            return;
        }
        destroy_subtree(node->left);
        destroy_subtree(node->right);
        delete node;
    }
};
