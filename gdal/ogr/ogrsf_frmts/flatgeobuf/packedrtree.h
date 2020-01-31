#ifndef FLATGEOBUF_PACKEDRTREE_H_
#define FLATGEOBUF_PACKEDRTREE_H_

#include <cmath>
#include <numeric>

#include "flatbuffers/flatbuffers.h"

namespace FlatGeobuf {

struct NodeItem {
    double minX;
    double minY;
    double maxX;
    double maxY;
    uint64_t offset = 0;
    double width() const { return maxX - minX; }
    double height() const { return maxY - minY; }
    static NodeItem sum(NodeItem a, const NodeItem& b) {
        a.expand(b);
        return a;
    }
    static NodeItem create(uint64_t offset = 0);
    void expand(const NodeItem& r);
    bool intersects(const NodeItem& r) const;
    std::vector<double> toVector();
};

struct Item {
    NodeItem nodeItem;
};

struct SearchResultItem {
    NodeItem nodeItem;
    uint64_t index;
};

std::ostream& operator << (std::ostream& os, NodeItem const& value);

uint32_t hilbert(uint32_t x, uint32_t y);
uint32_t hilbert(const NodeItem& n, uint32_t hilbertMax, const NodeItem& extent);
void hilbertSort(std::vector<std::shared_ptr<Item>> &items);
void hilbertSort(std::vector<NodeItem> &items);
NodeItem calcExtent(const std::vector<std::shared_ptr<Item>> &items);
NodeItem calcExtent(const std::vector<NodeItem> &rects);

/**
 * Packed R-Tree
 * Based on https://github.com/mourner/flatbush
 */
class PackedRTree {
    NodeItem _extent;
    NodeItem *_nodeItems = nullptr;
    uint64_t _numItems;
    uint64_t _numNodes;
    uint16_t _nodeSize;
    std::vector<std::pair<uint64_t, uint64_t>> _levelBounds;
    void init(const uint16_t nodeSize);
    void generateNodes();
    void fromData(const void *data);
public:
    ~PackedRTree() {
        if (_nodeItems != nullptr)
            delete[] _nodeItems;
    }
    PackedRTree(const std::vector<std::shared_ptr<Item>> &items, const NodeItem& extent, const uint16_t nodeSize = 16);
    PackedRTree(const std::vector<NodeItem> &nodes, const NodeItem& extent, const uint16_t nodeSize = 16);
    PackedRTree(const void *data, const uint64_t numItems, const uint16_t nodeSize = 16);
    std::vector<SearchResultItem> search(double minX, double minY, double maxX, double maxY) const;
    static std::vector<SearchResultItem> streamSearch(
        const uint64_t numItems, const uint16_t nodeSize, const NodeItem& item,
        const std::function<void(uint8_t *, size_t, size_t)> &readNode);
    static std::vector<std::pair<uint64_t, uint64_t>> generateLevelBounds(const uint64_t numItems, const uint16_t nodeSize);
    uint64_t size() const;
    static uint64_t size(const uint64_t numItems, const uint16_t nodeSize = 16);
    NodeItem getExtent() const;
    void streamWrite(const std::function<void(uint8_t *, size_t)> &writeData);
};

}

#endif
