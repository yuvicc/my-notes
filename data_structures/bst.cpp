#include <functional>
#include <iostream>
#include <stdexcept>
#include <queue>

template<typename T>
class BST {
public:

private:
    struct Node {
        T data;
        Node* left;
        Node* right;
        Node(T val)
            : data(val), left(nullptr), right(nullptr) {}
    };

    Node* root;

    Node* insert(Node* node, T val) {
        if (!node) return new Node(val);
        if (val < node->data)
            node->left = insert(node->left, val);
        else if (val > node->data)
            node->right = insert(node->right, val);

        return node;
    }

    Node* minNode(Node* node) const {
        while (node->left)
            node = node->left;
        return node;
    }

    Node* maxNode(Node* node) const {
        while (node->right)
            node = node->right;
        return node;
    }

    Node* remove(Node* node, T val) {
        if (!node) return nullptr;

        if (val < node->data) {
            node->left = remove(node->left, val);
        } else if (val > node->data) {
            node->right = remove(node->right, val);
        } else {
           // This one is trickiest - we want to delete this node, it might have left/right children node.

           // if it has only right child node
           if (!node->left) {
               Node* temp = node->right;
               delete node;
               return temp;
           }

           // if it has only left child node
           if (!node->right) {
               Node* temp = node->left;
               delete node;
               return temp;
           }

           // if it contains both left and right child
           Node* successor = minNode(node->right);
           node->data = successor->data;
           node->right = remove(node->right, successor->data);
        }

        return node;
    }

    bool search(Node* node, T val) const {
        if (!node) return false;
        if (val == node->data) return true;
        if (val < node->data) return search(node->left, val);
        return search(node->right, val);
    }

    int height(Node* node) const {
        if (!node) return -1;
        return 1 + std::max(height(node->left), height(node->right));
    }

    void inorder(Node* node, std::function<void(T)> visit) const {
        if (!node) return;
        inorder(node->left, visit);
        visit(node->data);
        inorder(node->right, visit);
    }

    void preorder(Node* node, std::function<void(T)> visit) const {
        if (!node) return;
        visit(node->data);
        preorder(node->left, visit);
        preorder(node->right, visit);
    }

    void postorder(Node* node, std::function<void(T)> visit) const {
        if (!node) return;
        postorder(node->left, visit);
        postorder(node->right, visit);
        visit(node->data);
    }

    void destroy(Node* node) {
        if (!node) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

    Node* clone(Node* node) const {
        if (!node) return nullptr;
        Node* newNode = new Node(node->data);
        newNode->left = clone(node->left);
        newNode->right = clone(node->right);
        return newNode;
    }

public:
    BST() : root(nullptr) {}

    // rule of 3
    BST(const BST& other) : root(clone(other.root)) {}

    BST& operator=(const BST& other) {
        if (this != nullptr) {
            destroy(root);
            root = clone(other);
        }
        return *this;
    }

    ~BST() { destroy(root); }

    void insert(T val) { root = insert(root, val); }
    void remove(T val) { root = remove(root, val); }
    void search(T val) const { return search(root, val); }
    bool empty() const { return root == nullptr; }
    int height() const { return height(root); }
    int size() const { return size(root); }

    T min() const {
        if (!root)
            throw std::runtime_error("BST is empty!");
        return minNode(root)->data;
    }

    T max() const {
        if (!root)
            throw std::runtime_error("BST is empty!");
        return maxNode(root)->data;
    }

    void inorder(std::function<void(T)> visit) const { inorder(root, visit); }
    void preorder(std::function<void(T)> visit) const { preorder(root, visit); }
    void postorder(std::function<void(T)> visit) const { postorder(root, visit); }

    void levelorder() const {
        if (!root) return;
        std::queue<Node*> q;
        q.push(root);
        while (!q.empty()) {
            Node* curr = q.front(); q.pop();
            visit(curr->data);
            if (curr->left)  q.push(curr->left);
            if (curr->right) q.push(curr->right);
        }
    }

    void print() const { print(root, "", false); }

private:
    void print(Node* node, std::string prefix, bool isLeft) const {
        if (!node) return;
        print(node->right, prefix + (isLeft ? "|   " : "    "), false);
        std::cout << prefix << (isLeft ? "└── " : "┌── ") << node->data << "\n";
        print(node->left, prefix + (isLeft ? "    " : "|    "), true);
    }
};

signed main() {
    BST<int> tree;
    for (int v : {50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45})
        tree.insert(v);

    std::cout << "Yuvicc's Tree: \n";
    tree.print();

    auto printer = [](int v) { std::cout << v << " "; };

    tree.inorder(printer);
    tree.preorder(printer);
    tree.postorder(printer);

    std::cout << "\n\n\n";

    tree.remove(80);

    tree.print();

    return 0;
}
