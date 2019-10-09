#ifndef FLATGEOBUF_PACKEDRTREE_H_
#define FLATGEOBUF_PACKEDRTREE_H_

#include <cmath>
#include <numeric>

#include "flatbuffers/flatbuffers.h"

namespace FlatGeobuf {

struct Rect {
    double minX;
    double minY;
    double maxX;
    double maxY;
    double width() const { return maxX - minX; }
    double height() const { return maxY - minY; }
    static Rect sum(Rect a, const Rect& b) {
        a.expand(b);
        return a;
    }
    static Rect createInvertedInfiniteRect();
    void expand(const Rect& r);
    bool intersects(const Rect& r) const;
    std::vector<double> toVector();
};

struct Item {
    Rect rect;
};

std::ostream& operator << (std::ostream& os, Rect const& value);

uint32_t hilbert(uint32_t x, uint32_t y);
uint32_t hilbert(const Rect& r, uint32_t hilbertMax, const Rect& extent);
void hilbertSort(std::vector<std::shared_ptr<Item>> &items);
void hilbertSort(std::vector<Rect> &items);
Rect calcExtent(const std::vector<std::shared_ptr<Item>> &rectitems);
Rect calcExtent(const std::vector<Rect> &rects);

/**
 * Packed R-Tree
 * Based on https://github.com/mourner/flatbush
 */
class PackedRTree {
    Rect _extent;
    std::vector<Rect> _rects;
    std::vector<uint32_t> _indices;
    uint64_t _numItems;
    uint64_t _numNodes;
    uint32_t _numNonLeafNodes;
    uint32_t _minAlign;
    uint16_t _nodeSize;
    std::vector<uint64_t> _levelBounds;
    void init(const uint16_t nodeSize);
    static std::vector<uint64_t> generateLevelBounds(const uint64_t numItems, const uint16_t nodeSize);
    void generateNodes();
    void fromData(const void *data);
public:
    PackedRTree(const std::vector<std::shared_ptr<Item>> &items, const Rect& extent, const uint16_t nodeSize = 16);
    PackedRTree(const std::vector<Rect> &rects, const Rect& extent, const uint16_t nodeSize = 16);
    PackedRTree(const void *data, const uint64_t numItems, const uint16_t nodeSize = 16);
    std::vector<uint64_t> search(double minX, double minY, double maxX, double maxY) const;
    static std::vector<uint64_t> streamSearch(
        const uint64_t numItems, const uint16_t nodeSize, const Rect& r,
        const std::function<void(uint8_t *, size_t, size_t)> &readNode);
    uint64_t size() const;
    static uint64_t size(const uint64_t numItems, const uint16_t nodeSize = 16);
    uint8_t *toData() const;
    Rect getExtent() const;
    void streamWrite(const std::function<void(uint8_t *, size_t)> &writeData);
};

}

#endif
