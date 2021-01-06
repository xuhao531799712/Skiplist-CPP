#include <iostream>
#include <string>
#include <mutex>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <random>

#define STORE_FILE "../store/dumpFile"    // 注意此处，使用不同的IDE或者使用终端直接生成可执行文件，要注意可执行文件生成的位置来判定此处的路径，否则会找不到文件
std::string delimiter = ":"; // NOLINT

// 首先编写单线程数据库，再加入多线程并发支持
// SkipList 类，类里的节点为 Node 类，Node 有多个链表头尾

template<typename K, typename V>
class Node {
public:
    Node(K, V, int);
    ~Node();

    // 接口
    K get_key() const;
    V get_value() const;
    bool set_value(V);

    // 线性表，存不同深度指向下一个节点的指针
    Node<K, V> **forward;
    int node_level; // 存本节点所在的深度
private:
    K key;
    V value;
};

template<typename K, typename V>
Node<K, V>::Node(const K k, const V v, int level) : key(k), value(v), node_level(level) {
    forward = new Node<K, V>*[level + 1]; // 此处可改为 vector 实现, level 索引值从 0 开始
    memset(forward, 0, sizeof(Node<K, V>*) * (level+1));
}

template<typename K, typename V>
Node<K, V>::~Node() {
    // 自己 new 的要 delete
    delete []forward; // 注意只删这个数组，可千万不能删里面的指针，那些不是它分配的
}

template<typename K, typename V>
K Node<K, V>::get_key() const {
    return key;
}

template<typename K, typename V>
V Node<K, V>::get_value() const {
    return value;
}

template<typename K, typename V>
bool Node<K, V>::set_value(const V val) {
    value = val;
    return true;
}

template<typename K, typename V>
class SkipList {
public:
    // 构造函数与析构函数
    explicit SkipList(int);
    ~SkipList();

    // 接口定义
        // 下层接口
    int get_random_level(); // 获取随机层数，使用概率值来模拟理想跳表形式
    Node<K, V>* create_node(K, V, int);
        // 增删改查
    int insert_element(K, V);
    int delete_element(K);
    int change_element(K, V);
    bool search_element(K);
        // 其他
    void display_list();
    int size();

        // 持久化接口，文件存取
    void dump_file();
    void load_file();
    
    
private:
    // 成员变量定义
    int _max_level;
    int _skip_list_level;
    Node<K, V> *_header; // 链表中的头结点
    int _element_count;

    std::default_random_engine e;
    std::mutex mtx;     // mutex for critical section

    // 文件持久化句柄
    std::ofstream _file_writer;
    std::ifstream _file_reader;
private:
    // 持久化操作需要的子方法
    void get_key_value_from_string(const std::string& str, std::string& key, std::string& value);
    bool is_valid_string(const std::string& str);
};


template<typename K, typename V>    
int SkipList<K, V>::size() {
    return _element_count;
}

template<typename K, typename V>
int SkipList<K, V>::get_random_level() {
    int n = 0; // 此处应该为 0，第 0 层是最低层，各有 50% 的概率向上     // 提醒作者修改本处 *******************
    std::bernoulli_distribution u(0.5);
    while (u(e)) {
        n++;
    }
    return n > _max_level ? _max_level : n;
}

template<typename K, typename V>
Node<K, V>* SkipList<K, V>::create_node(const K k, const V v, int level) {
    auto* node = new Node<K, V>(k, v, level);
    return node;
}

template<typename K, typename V>
SkipList<K, V>::SkipList(int max_level) : _max_level(max_level), _skip_list_level(0), _element_count(0) {
    K k;
    V v;
    _header = new Node<K, V>(k, v, _max_level);
    e.seed(time(nullptr));
}

template<typename K, typename V>
SkipList<K, V>::~SkipList() {
    if (_file_writer.is_open()) {
        _file_writer.close();
    }
    if (_file_reader.is_open()) {
        _file_reader.close();
    }
    // 循环删除所有节点   // 提醒作者修改本处 *******************
    Node<K, V>* current = _header->forward[0];
    while(current) {
        Node<K, V>* tmp = current->forward[0];
        delete current;
        current = tmp;
    }
    delete _header;
}

template<typename K, typename V>
int SkipList<K, V>::insert_element(K key, V val) {
    mtx.lock();
    Node<K, V> *current = _header;
    Node<K, V> *update[_max_level + 1];
    memset(update, 0, (_max_level + 1) * sizeof(Node<K, V>*));

    for (int i = _skip_list_level; i >= 0; --i) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    // current 即为小于 key 的最大节点，其下一个节点为大于等于 key 的第一个结点

    // 接下来对 key 的存在做判定
    current = current->forward[0];
    if (current && current->get_key() == key) {
        std::cout<<"Key: "<<key<<" has existed."<<std::endl;
        mtx.unlock();
        return 1;
    }

    int random_level = get_random_level();
    if (random_level > _skip_list_level) {
        // 深度大于当前列表的深度，则更新深度，并更新 update 数组指向 head
        for (int i = _skip_list_level + 1; i <= random_level; ++i) {
            update[i] = _header;
        }
        _skip_list_level = random_level;
    }

    Node<K, V>* inserted_node = create_node(key, val, random_level);
    for (int i = random_level; i >= 0; --i) {
        inserted_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = inserted_node;
    }

    std::cout << "Successfully inserted key:" << key << ", value:" << val << std::endl;
    _element_count ++;

    mtx.unlock();
    return 0;
}

template<typename K, typename V>
int SkipList<K, V>::delete_element(K key) {
    mtx.lock();
    Node<K, V> *current = _header;
    Node<K, V> *update[_max_level + 1];
    memset(update, 0, (_max_level + 1) * sizeof(Node<K, V>*));

    for (int i = _skip_list_level; i >= 0; --i) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    // current 即为小于 key 的最大节点，其下一个节点为大于等于 key 的第一个结点

    // 接下来对 key 的存在做判定
    current = current->forward[0];
    if (!current || current->get_key() != key) {
        std::cout<<"Key: "<<key<<" do not existed."<<std::endl;
        mtx.unlock();
        return 1;
    }

    // 本循环原代码中可以如此优化   // 提醒作者修改本处 *******************
    for (int i = current->node_level; i >= 0; --i) {
        update[i]->forward[i] = current->forward[i];
    }

    while (_skip_list_level > 0 && _header->forward[_skip_list_level] == 0) {
        _skip_list_level --;
    }

    delete current;    // 提醒作者修改本处 *******************

    std::cout << "Successfully deleted key:" << key << "." << std::endl;
    _element_count --;

    mtx.unlock();
    return 0;
}

template<typename K, typename V>
int SkipList<K, V>::change_element(K key, V val) {
    mtx.lock();

    Node<K, V> *current = _header;

    for (int i = _skip_list_level; i >= 0; --i) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }
    // current 即为小于 key 的最大节点，其下一个节点为大于等于 key 的第一个结点

    // 接下来对 key 的存在做判定
    current = current->forward[0];
    if (!current || current->get_key() != key) {
        std::cout<<"Key: "<<key<<" do not existed."<<std::endl;
        mtx.unlock();
        return 1;
    }

    if(current->set_value(val)) {
        std::cout<<"Key: "<<key<<" change value failed."<<std::endl;
        mtx.unlock();
        return 1;
    }

    std::cout << "Successfully changed key:" << key << ", value:" << val << std::endl;

    mtx.unlock();
    return 0;
}

template<typename K, typename V>
bool SkipList<K, V>::search_element(K key) {
    Node<K, V> *current = _header;

    for (int i = _skip_list_level; i >= 0; --i) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }
    // current 即为小于 key 的最大节点，其下一个节点为大于等于 key 的第一个结点

    // 接下来对 key 的存在做判定
    current = current->forward[0];

    if (current and current->get_key() == key) {
        std::cout << "Found key: " << key << ", value: " << current->get_value() << std::endl;
        return true;
    }

    std::cout << "Not Found Key:" << key << std::endl;
    return false;
}

template<typename K, typename V>
void SkipList<K, V>::display_list() {
    std::cout << "\n*****Skip List*****"<<"\n";
    for (int i = 0; i <= _skip_list_level; i++) {
        Node<K, V> *node = this->_header->forward[i];
        std::cout << "Level " << i << ": ";
        while (node != NULL) {
            std::cout << node->get_key() << ":" << node->get_value() << ";";
            node = node->forward[i];
        }
        std::cout << std::endl;
    }
}

template<typename K, typename V>
void SkipList<K, V>::dump_file() {
    std::cout << "\n*****Dump file*****"<<"\n";
    _file_writer.open(STORE_FILE);
    Node<K, V>* current = _header->forward[0];

    while (current) {
        _file_writer << current->get_key() << delimiter << current->get_value() << std::endl;
        std::cout << current->get_key() << ":" << current->get_value() << std::endl;
        current = current->forward[0];
    }

    _file_writer.flush();   // 由于前面使用的是 std::endl 而不是 ‘/n'，所以此处不需要再flush
    _file_writer.close();
}
                                    
template<typename K, typename V>    
void SkipList<K, V>::load_file() {
    _file_reader.open(STORE_FILE);
    std::cout << "load_file-----------------" << std::endl;
    std::string line, key, value;
    while (getline(_file_reader, line)) {
        get_key_value_from_string(line, key, value);
        if (key.empty() || value.empty()) {
            std::cout << "error data ——  key:" << key << "value:" << value << std::endl;
            continue;
        }
        insert_element(key, value);
        std::cout << "key:" << key << "value:" << value << std::endl;
    }
//    std::cout<<
    _file_reader.close();
}

template<typename K, typename V>
void SkipList<K, V>::get_key_value_from_string(const std::string& str, std::string& key, std::string& value) {
    if (!is_valid_string(str)) return;
    int index = str.find(delimiter);
    key = str.substr(0, index);
    value = str.substr(index + 1, str.size() - index - 1);
}

template<typename K, typename V>
bool SkipList<K, V>::is_valid_string(const std::string& str) {

    if (str.empty()) {
        return false;
    }
    return str.find(delimiter) != std::string::npos;
}






