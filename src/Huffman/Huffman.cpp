#include "Huffman.h"

#include <queue>
#include <unordered_map>
#include <stdexcept>

struct Node
{
    uint8_t symbol;
    uint64_t frequency;

    Node* left;
    Node* right;

    Node(
        uint8_t s,
        uint64_t f)
        :
        symbol(s),
        frequency(f),
        left(nullptr),
        right(nullptr)
    {
    }

    Node(
        Node* l,
        Node* r)
        :
        symbol(0),
        frequency(l->frequency + r->frequency),
        left(l),
        right(r)
    {
    }

    bool isLeaf() const
    {
        return left == nullptr &&
               right == nullptr;
    }
};

struct Compare
{
    bool operator()(
        Node* a,
        Node* b)
    {
        return a->frequency >
               b->frequency;
    }
};

static void freeTree(Node* node)
{
    if (!node)
        return;

    freeTree(node->left);
    freeTree(node->right);

    delete node;
}

static Node* buildTree(
    const std::array<uint64_t,256>& frequencies)
{
    std::priority_queue<
        Node*,
        std::vector<Node*>,
        Compare
    > pq;

    for (int i = 0; i < 256; i++)
    {
        if (frequencies[i] > 0)
        {
            pq.push(
                new Node(
                    static_cast<uint8_t>(i),
                    frequencies[i]));
        }
    }

    if (pq.empty())
        return nullptr;

    if (pq.size() == 1)
        return pq.top();

    while (pq.size() > 1)
    {
        Node* left = pq.top();
        pq.pop();

        Node* right = pq.top();
        pq.pop();

        pq.push(
            new Node(
                left,
                right));
    }

    return pq.top();
}

static void buildCodes(
    Node* node,
    std::vector<bool>& current,
    std::array<std::vector<bool>,256>& codes)
{
    if (!node)
        return;

    if (node->isLeaf())
    {
        if (current.empty())
        {
            current.push_back(false);
        }

        codes[node->symbol] = current;
        return;
    }

    current.push_back(false);

    buildCodes(
        node->left,
        current,
        codes);

    current.pop_back();

    current.push_back(true);

    buildCodes(
        node->right,
        current,
        codes);

    current.pop_back();
}

std::vector<uint8_t> Huffman::compress(
    const std::vector<uint8_t>& input,
    std::array<uint64_t,256>& frequencies)
{
    frequencies.fill(0);

    for (uint8_t byte : input)
    {
        frequencies[byte]++;
    }

    Node* root =
        buildTree(frequencies);

    if (!root)
        return {};

    std::array<std::vector<bool>,256> codes;

    std::vector<bool> current;

    buildCodes(
        root,
        current,
        codes);

    std::vector<uint8_t> output;

    uint8_t currentByte = 0;
    int bitCount = 0;

    for (uint8_t byte : input)
    {
        const auto& code =
            codes[byte];

        for (bool bit : code)
        {
            currentByte <<= 1;

            if (bit)
            {
                currentByte |= 1;
            }

            bitCount++;

            if (bitCount == 8)
            {
                output.push_back(
                    currentByte);

                currentByte = 0;
                bitCount = 0;
            }
        }
    }

    if (bitCount > 0)
    {
        currentByte <<= (8 - bitCount);

        output.push_back(
            currentByte);
    }

    freeTree(root);

    return output;
}

std::vector<uint8_t> Huffman::decompress(
    const std::vector<uint8_t>& compressed,
    const std::array<uint64_t,256>& frequencies,
    uint64_t originalSize)
{
    Node* root = buildTree(frequencies);

    if (!root)
        return {};

    std::vector<uint8_t> output;
    output.reserve(originalSize);

    if (root->isLeaf())
    {
        return std::vector<uint8_t>(
            originalSize,
            root->symbol);
    }

    Node* current = root;

    for (size_t i = 0; i < compressed.size() && output.size() < originalSize; i++)
    {
        uint8_t byte = compressed[i];

        for (int bit = 7; bit >= 0 && output.size() < originalSize; bit--)
        {
            bool value = (byte >> bit) & 1;

            current = value ? current->right : current->left;

            if (!current)
            {
                freeTree(root);
                return {};
            }

            if (current->isLeaf())
            {
                output.push_back(current->symbol);
                current = root;
            }
        }
    }

    freeTree(root);
    return output;
}