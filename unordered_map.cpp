#include <cmath>
#include <iostream>
#include <iterator>
#include <vector>


template < typename U, typename Alloc = std::allocator<std::pair<size_t, U>> >
class List {
    using T = std::pair<size_t, U>;

    template <typename Key, typename Value, typename Hash, typename Equal, typename MapAlloc>
    friend class UnorderedMap;

    struct BaseNode {
        BaseNode* prev;
        BaseNode* next;

        BaseNode(BaseNode* pr, BaseNode* nx) : prev(pr), next(nx) {}
    };

    struct Node: public BaseNode {
        T value;

        Node(BaseNode* pr, BaseNode* nx, const T& val) : BaseNode(pr, nx), value(val) {}

        Node(BaseNode* pr, BaseNode* nx) : BaseNode(pr, nx) {}

        template <typename Key, typename Value>
        Node(BaseNode* pr, BaseNode* nx, std::pair<size_t, std::pair<Key, Value>>&& val) :
                BaseNode(pr, nx), value(std::move(val)) {}
    };

    size_t sz;
    BaseNode fake_node;
    using Allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
    [[no_unique_address]] Allocator alloc;

    template <typename... Args>
    Node* create_node(Args&&... args) {
        Node* new_ptr = std::allocator_traits<Allocator>::allocate(alloc, 1);
        try {
            std::allocator_traits<Allocator>::construct(alloc, new_ptr, std::forward<Args>(args)...);
        } catch(...) {
            std::allocator_traits<Allocator>::deallocate(alloc, new_ptr, 1);
            throw;
        }
        ++sz;
        return new_ptr;
    }

    Node* erase_node(BaseNode* del) {
        using PairAlloc = std::allocator_traits<Allocator>::template rebind_alloc<U>;
        PairAlloc pair_alloc = alloc;

        BaseNode* pr = del->prev;
        BaseNode* nx = del->next;
        std::allocator_traits<PairAlloc>::destroy(pair_alloc, &static_cast<Node*>(del)->value.second);
        std::allocator_traits<Allocator>::deallocate(alloc, static_cast<Node*>(del), 1);
        pr->next = nx;
        nx->prev = pr;
        --sz;
        return static_cast<Node*>(nx);
    }

    void clear() {
        while (sz) pop_back();
    }


    template <bool IsConst>
    class BaseIterator {
        template <typename Key, typename Value, typename Hash, typename Equal, typename MapAlloc>
        friend class UnorderedMap;

        template <typename R, typename ListAlloc>
        friend class List;

    public:
        using pointer = std::conditional_t<IsConst, const U*, U*>;
        using reference = std::conditional_t<IsConst, const U&, U&>;
        using value_type = U;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = int;

        using base_node_pointer = std::conditional_t<IsConst, const BaseNode*, BaseNode*>;
        using node_pointer = std::conditional_t<IsConst, const Node*, Node*>;

    private:
        base_node_pointer ptr;

        size_t get_hash() const {
            return static_cast<node_pointer>(ptr)->value.first;
        }

        using HashAndObj = std::conditional_t<IsConst, const T&, T&>;
        HashAndObj get_hash_and_obj() {
            return static_cast<node_pointer>(ptr)->value;
        }

        base_node_pointer get_ptr() const {
            return ptr;
        }

    public:

    	BaseIterator() : ptr(nullptr) {}

        BaseIterator(base_node_pointer ptr) : ptr(ptr) {}

        BaseIterator(const BaseIterator& other) = default;

        BaseIterator& operator=(const BaseIterator& other) = default;

        reference operator*() const {
            return static_cast<node_pointer>(ptr)->value.second;
        }

        pointer operator->() const {
            return &static_cast<node_pointer>(ptr)->value.second;
        }

        BaseIterator& operator++() {
            ptr = ptr->next;
            return *this;
        }

        BaseIterator operator++(int) {
            BaseIterator copy = *this;
            ptr = ptr->next;
            return copy;
        }

        BaseIterator& operator--() {
            ptr = ptr->prev;
            return *this;
        }

        BaseIterator operator--(int) {
            BaseIterator copy = *this;
            ptr = ptr->prev;
            return copy;
        }

        template <bool OtherIsConst>
        bool operator==(const BaseIterator<OtherIsConst>& other) const {
            return ptr == other.get_ptr();
        }

        template <bool OtherIsConst>
        bool operator!=(const BaseIterator<OtherIsConst>& other) const {
            return ptr != other.get_ptr();
        }

        operator BaseIterator<true>() const {
            return BaseIterator<true>(ptr);
        }
    };

    void swap_fake_nodes(BaseNode& first, BaseNode& second) {
        bool first_empty = (first.prev == &first && first.next == &first);
        bool second_empty = (second.prev == &second && second.next == &second);
        BaseNode* first_pr = !first_empty ? first.prev : &second;
        BaseNode* first_nx = !first_empty ? first.next : &second;
        BaseNode* second_pr = !second_empty ? second.prev : &first;
        BaseNode* second_nx = !second_empty ? second.next : &first;
        first_pr->next = first_nx->prev = &second;
        second = BaseNode(first_pr, first_nx);
        second_pr->next = second_nx->prev = &first;
        first = BaseNode(second_pr, second_nx);
    }

    List(Allocator alloc, const List& other) : List(alloc) {
        for (auto it = other.begin(); it != other.end(); ++it) {
            push_back(it.get_hash_and_obj());
        }
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        Node* new_ptr = create_node(fake_node.prev, &fake_node, std::forward<Args>(args)...);
        fake_node.prev->next = new_ptr;
        fake_node.prev = new_ptr;
    }

    bool need_update(List& other) const {
        return !(alloc == other.alloc || std::allocator_traits<Allocator>::is_always_equal::value
        || std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value);
    }

public:
    using iterator = BaseIterator<false>;
    using const_iterator = BaseIterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;


    List(): sz(0), fake_node(&fake_node, &fake_node) {}

    List(size_t new_sz): List() {
        for (size_t i = 0; i < new_sz; ++i) {
            emplace_back();
        }
    }

    List(size_t sz, const T& obj): List() {
        for (size_t i = 0; i < sz; ++i) {
            push_back(obj);
        }
    }

    List(Alloc alloc): sz(0), fake_node(&fake_node, &fake_node), alloc(alloc) {}

    List(size_t new_sz, Alloc alloc): List(alloc) {
        for (size_t i = 0; i < new_sz; ++i) {
            emplace_back();
        }
    }

    List(size_t sz, const T& obj, Alloc alloc): List(alloc) {
        for (size_t i = 0; i < sz; ++i) {
            push_back(obj);
        }
    }

    List(const List& other): List(std::allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc)) {
        for (auto it = other.begin(); it != other.end(); ++it) {
            push_back(it.get_hash_and_obj());
        }
    }

    List(List&& other) : List(std::move(other.alloc)) {
        BaseNode* other_pr = (other.sz != 0) ? other.fake_node.prev : &fake_node;
        BaseNode* other_nx = (other.sz != 0) ? other.fake_node.next : &fake_node;
        other_pr->next = other_nx->prev = &fake_node;
        fake_node = BaseNode(other_pr, other_nx);
        sz = other.sz;

        other.fake_node = BaseNode(&other.fake_node, &other.fake_node);
        other.sz = 0;
    }

    List& operator=(const List& other) {
        if (&other == this) return *this;

        Allocator new_alloc = std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value
                              ? other.alloc : alloc;

        List other_copy(new_alloc, other);
        clear();
        sz = other.sz;
        alloc = new_alloc;
        if (sz > 0) {
            fake_node.next = other_copy.fake_node.next;
            fake_node.prev = other_copy.fake_node.prev;
            fake_node.prev->next = fake_node.next->prev = &fake_node;

            other_copy.fake_node = {&other_copy.fake_node, &other_copy.fake_node};
            other_copy.sz = 0;
        }
        return *this;
    }

    List& operator=(List&& other) noexcept {
        if (&other == this) return *this;

        clear();
        if (alloc == other.alloc) {
            swap_fake_nodes(fake_node, other.fake_node);
            sz = other.sz;
            other.sz = 0;
            return *this;
        }
        if constexpr (std::allocator_traits<Allocator>::is_always_equal::value) {
            swap_fake_nodes(fake_node, other.fake_node);
            sz = other.sz;
        } else if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
            swap_fake_nodes(fake_node, other.fake_node);
            sz = other.sz;
            alloc = std::move(other.alloc);
        } else {
            for (auto it = other.begin(); it != other.end(); ++it) {
                push_back(std::move(it.get_hash_and_obj()));
            }
            other.clear();
        }
        other.sz = 0;
        return *this;
    }


    Alloc get_allocator() const {
        return alloc;
    }

    size_t size() const {
        return sz;
    }

    void push_back(const T& obj) {
        insert(end(), obj);
    }

    void push_back(T&& obj) {
        insert(end(), std::move(obj));
    }

    void push_front(const T& obj) {
        insert(begin(), obj);
    }

    void push_front(T&& obj) {
        insert(begin(), std::move(obj));
    }

    void pop_back() {
        erase(--end());
    }

    void pop_front() {
        erase(begin());
    }


    iterator begin() {
        return iterator(fake_node.next);
    }

    const_iterator begin() const {
        return const_iterator(fake_node.next);
    }

    iterator end() {
        return iterator(&fake_node);
    }

    const_iterator end() const {
        return const_iterator(&fake_node);
    }

    const_iterator cbegin() const {
        return const_iterator(fake_node.next);
    }

    const_iterator cend() const {
        return const_iterator(&fake_node);
    }

    reverse_iterator rbegin() {
        return reverse_iterator(&fake_node);
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(&fake_node);
    }

    reverse_iterator rend() {
        return reverse_iterator(fake_node.next);
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(fake_node.next);
    }

    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(&fake_node);
    }

    const_reverse_iterator crend() const {
        return const_reverse_iterator(fake_node.next);
    }

    iterator erase(const_iterator iter) {
        return iterator(erase_node(const_cast<BaseNode*>(iter.get_ptr())));
    }

    void swap(List& other) {
        if (alloc == other.alloc) {
            swap_fake_nodes(fake_node, other.fake_node);
            std::swap(sz, other.sz);
            return;
        }
        if constexpr (std::allocator_traits<Allocator>::is_always_equal::value) {
            swap_fake_nodes(fake_node, other.fake_node);
            std::swap(sz, other.sz);
        } else if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
            swap_fake_nodes(fake_node, other.fake_node);
            std::swap(sz, other.sz);
            std::swap(alloc, other.alloc);
        } else {
            BaseNode copy = {&copy, &copy};
            size_t copy_sz = 0;
            swap_fake_nodes(fake_node, copy);
            std::swap(copy_sz, sz);
            for (auto it = other.begin(); it != other.end(); ++it) {
                push_back(std::move(it.get_hash_and_obj()));
            }
            other.clear();

            swap_fake_nodes(fake_node, copy);
            std::swap(copy_sz, sz);
            for (auto it = begin(); it != end(); ++it) {
                other.push_back(std::move(it.get_hash_and_obj()));
            }
            clear();
            swap_fake_nodes(fake_node, copy);
            std::swap(copy_sz, sz);
        }
    }

    iterator insert(const_iterator iter, const T& obj) {
        BaseNode* nx = const_cast<BaseNode*>(iter.get_ptr());
        BaseNode* pr = nx->prev;

        Node* new_ptr = create_node(pr, nx, obj);
        pr->next = new_ptr;
        nx->prev = new_ptr;
        return iterator(new_ptr);
    }

    iterator insert(const_iterator iter, T&& obj) {
        BaseNode* nx = const_cast<BaseNode*>(iter.get_ptr());
        BaseNode* pr = nx->prev;

        Node* new_ptr = create_node(pr, nx, std::move(obj));
        pr->next = new_ptr;
        nx->prev = new_ptr;
        return iterator(new_ptr);
    }

    template <typename PairType>
    iterator insert(const_iterator iter, size_t sz, PairType&& key_val) {
        BaseNode* nx = const_cast<BaseNode*>(iter.get_ptr());
        BaseNode* pr = nx->prev;

        Node* new_ptr = create_node(pr, nx, std::make_pair(sz, std::forward<PairType>(key_val)));
        pr->next = new_ptr;
        nx->prev = new_ptr;
        return iterator(new_ptr);
    }

    iterator put(const_iterator iter, Node* new_ptr) {
        BaseNode* nx = const_cast<BaseNode*>(iter.get_ptr());
        BaseNode* pr = nx->prev;

        new_ptr->prev = pr;
        new_ptr->next = nx;
        pr->next = new_ptr;
        nx->prev = new_ptr;
        ++sz;
        return iterator(new_ptr);
    }

    ~List() {
        clear();
    }
};





template <typename Key, typename Value, typename Hash = std::hash<Key>,
        typename Equal = std::equal_to<Key>, typename Alloc = std::allocator<std::pair<const Key, Value>>>
class UnorderedMap {
public:
    using NodeType = std::pair<const Key, Value>;

private:
    using ListAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<std::pair<size_t, NodeType>>;

public:
    using iterator = typename List<NodeType, ListAlloc>::iterator;
    using const_iterator = typename List<NodeType, ListAlloc>::const_iterator;

private:
    using VectorAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<iterator>;
    using BaseNode = typename List<NodeType, ListAlloc>::BaseNode;
    using Node = typename List<NodeType, ListAlloc>::Node;

    std::vector<iterator, VectorAlloc> hash_table;
    List<NodeType, ListAlloc> lst;
    [[no_unique_address]] Hash hash;
    [[no_unique_address]] Equal eq;
    double ml;


    void update_hash_table() {
        for (size_t i = 0; i < hash_table.size(); ++i) {
            hash_table[i] = end();
        }
        size_t last_hash = 0;
        bool start = true;
        for (auto it = begin(); it != end(); ++it) {
            size_t curr_hash = it.get_hash();
            if (start || last_hash % hash_table.size() != curr_hash % hash_table.size()) {
                hash_table[curr_hash % hash_table.size()] = it;
                last_hash = curr_hash;
                start = false;
            }
        }
    }

    void rehash(size_t new_sz) {
        for (size_t i = 0; i < std::min(new_sz, hash_table.size()); ++i) {
            hash_table[i] = end();
        }
        hash_table.resize(new_sz, end());

        BaseNode* ptr = lst.fake_node.next;
        lst.fake_node.prev = lst.fake_node.next = &lst.fake_node;
        while (ptr != &lst.fake_node) {
            BaseNode* next_ptr = ptr->next;
            BaseNode* prev_ptr = hash_table[static_cast<Node*>(ptr)->value.first % hash_table.size()].get_ptr();
            if (prev_ptr == &lst.fake_node) {
                hash_table[static_cast<Node*>(ptr)->value.first % hash_table.size()] = iterator(ptr);
            }
            prev_ptr->next->prev = ptr;
            ptr->next = prev_ptr->next;
            prev_ptr->next = ptr;
            ptr->prev = prev_ptr;
            ptr = next_ptr;
        }
    }

public:
    UnorderedMap() {
        hash_table.resize(1, end());
        ml = 1.0;
    }

    UnorderedMap(const UnorderedMap& other) : hash_table(), lst(other.lst),
                                              hash(other.hash), eq(other.eq), ml(other.ml) {
        hash_table.resize(other.hash_table.size(), end());
        update_hash_table();
    }

    UnorderedMap(UnorderedMap&& other) noexcept : hash_table(std::move(other.hash_table)), lst(std::move(other.lst)),
                                                  hash(std::move(other.hash)), eq(std::move(other.eq)),
                                                  ml(std::move(other.ml)) {
        for (iterator& it : hash_table) {
            if (it == other.end()) it = end();
        }
        other.hash_table.resize(1, other.end());
        other.ml = 1.0;
    }

    ~UnorderedMap() = default;

    UnorderedMap& operator=(const UnorderedMap& other) {
        if (&other == this) return *this;

        lst = other.lst;
        hash_table.clear();
        hash_table.resize(other.hash_table.size(), end());
        update_hash_table();
        return *this;
    }

    UnorderedMap& operator=(UnorderedMap&& other) noexcept {
        if (&other == this) return *this;

        for (size_t i = 0; i < other.hash_table.size(); ++i) {
            if (other.hash_table[i] == other.end()) {
                other.hash_table[i] = end();
            }
        }
        hash_table = std::move(other.hash_table);
        other.hash_table.resize(1, other.end());

        bool upd = lst.need_update(other.lst);
        lst = std::move(other.lst);
        hash = std::move(other.hash);
        eq = std::move(other.eq);
        ml = std::move(other.ml);
        if (upd) update_hash_table();
        other.ml = 1.0;
        return *this;
    }


    size_t size() const {
        return lst.size();
    }

    double max_load_factor() const {
        return ml;
    }

    void max_load_factor(double new_load_factor) {
        ml = new_load_factor;
        if (size() > ml * hash_table.size()) {
            rehash(std::ceil(static_cast<double>(size()) / ml));
        }
    }

    double load_factor() const {
        return static_cast<double>(lst.size()) / static_cast<double>(hash_table.size());
    }

    template <typename PairType>
    std::pair<iterator, bool> insert(PairType&& node) {
        size_t key_hash = hash(node.first);
        
        if (1 + size() > ml * hash_table.size()) {
            rehash(2 * hash_table.size() + 1);
        }

        iterator it = hash_table[key_hash % hash_table.size()];

        if (it == end()) {
            iterator iter = lst.insert(lst.end(), key_hash, std::forward<PairType>(node));
            hash_table[key_hash % hash_table.size()] = iter;
            return std::make_pair(iter, true);
        }
        while (it != end() && it.get_hash() % hash_table.size() == key_hash % hash_table.size()) {
            if (eq(it->first, node.first)) {
                return std::make_pair(it, false);
            }
            ++it;
        }
        return std::make_pair(lst.insert(it, key_hash, std::forward<PairType>(node)), true);
    }

    std::pair<iterator, bool> insert(const NodeType& node) {
        return insert<const NodeType&>(node);
    }

    template <typename InputIt>
    void insert(InputIt begin, InputIt end) {
        while (begin != end) {
            insert(std::forward<decltype(*begin)>(*begin));
            ++begin;
        }
    }


    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        using ListAlloc = typename List<NodeType, ListAlloc>::Allocator;
        using PairAlloc = std::allocator_traits<ListAlloc>::template 
                rebind_alloc<NodeType>;
        PairAlloc pair_alloc = lst.alloc;
        Node* new_ptr = std::allocator_traits<ListAlloc>::allocate(lst.alloc, 1);
        try {
            std::allocator_traits<PairAlloc>::construct(pair_alloc, &new_ptr->value.second, 
                        std::forward<Args>(args)...);
        } catch(...) {
            std::allocator_traits<ListAlloc>::deallocate(lst.alloc, new_ptr, 1);
            throw;
        }

        try {
            new_ptr->value.first = hash(new_ptr->value.second.first);
        } catch(...) {
            std::allocator_traits<ListAlloc>::deallocate(lst.alloc, new_ptr, 1);
            throw;
        }
        
        if (1 + size() > ml * hash_table.size()) {
            rehash(2 * hash_table.size() + 1);
        }

        iterator it = hash_table[new_ptr->value.first % hash_table.size()];

        if (it == end()) {
            iterator iter = lst.put(lst.end(), new_ptr);
            hash_table[new_ptr->value.first % hash_table.size()] = iter;
            return std::make_pair(iter, true);
        }
        while (it != end() && it.get_hash() % hash_table.size() == new_ptr->value.first % hash_table.size()) {
            if (eq(it->first, new_ptr->value.second.first)) {
                std::allocator_traits<ListAlloc>::deallocate(lst.alloc, new_ptr, 1);
                return std::make_pair(it, false);
            }
            ++it;
        }
        return std::make_pair(lst.put(it, new_ptr), true);
    }

    void reserve(size_t count) {
        rehash(std::ceil(count / max_load_factor()));
    }

    void swap(UnorderedMap& other) {
        hash_table.swap(other.hash_table);
        lst.swap(other.lst);
        for (int i = 0; i < hash_table.size(); ++i) {
            if (hash_table[i] == other.end()) hash_table[i] = end();
        }
        for (int i = 0; i < other.hash_table.size(); ++i) {
            if (other.hash_table[i] == end()) other.hash_table[i] = other.end();
        }
        std::swap(hash, other.hash);
        std::swap(eq, other.eq);
        std::swap(ml, other.ml);
    }

    iterator erase(const_iterator it) {
        size_t key_hash = it.get_hash();
        if (hash_table[key_hash % hash_table.size()] == it) {
            iterator next = iterator(const_cast<BaseNode*>(it.get_ptr()));
            ++next;
            if (next == end() || key_hash % hash_table.size() != next.get_hash() % hash_table.size()) {
                hash_table[key_hash % hash_table.size()] = end();
            } else {
                hash_table[key_hash % hash_table.size()] = next;
            }
        }
        return lst.erase(it);
    }

    iterator erase(const_iterator begin, const_iterator end) {
        const_iterator last = begin;
        for (; last != end;) {
            last = erase(last);
        }
        return iterator(const_cast<BaseNode*>(last.get_ptr()));
    }


    template <typename Self>
    auto find(this Self&& self, const Key& key) {
    	using iterator_type = decltype(self.end());
        size_t key_hash = self.hash(key);
        iterator_type iter = self.hash_table[key_hash % self.hash_table.size()];
        while (iter != self.end() && iter.get_hash() % self.hash_table.size() == key_hash % self.hash_table.size()) {
            if (self.eq(iter->first, key)) return iter;
            ++iter;
        }
        return self.end();
    }

    Value& operator[](const Key& key) {
        iterator it = find(key);
        if (it == end()) {
            return emplace(key, Value()).first->second;
        } else {
            return it->second;
        }
    }

    Value& operator[](Key&& key) {
        iterator it = find(key);
        if (it == end()) {
            return emplace(std::move(key), Value()).first->second;
        } else {
            return it->second;
        }
    }

    template <typename Self>
    auto& at(this Self&& self, const Key& key) {
        auto it = self.find(key);
        if (it == self.end()) {
            throw std::out_of_range("key does not exist");
        } else {
            return it->second;
        }
    }

    iterator begin() {
        return lst.begin();
    }

    const_iterator begin() const {
        return cbegin();
    }

    iterator end() {
        return lst.end();
    }

    const_iterator end() const {
        return cend();
    }

    const_iterator cbegin() const {
        return lst.cbegin();
    }

    const_iterator cend() const {
        return lst.cend();
    }

    ListAlloc get_allocator() const {
        return lst.alloc;
    }
};

