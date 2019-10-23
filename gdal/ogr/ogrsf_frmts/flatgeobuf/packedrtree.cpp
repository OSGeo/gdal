#ifdef GDAL_COMPILATION
#include "cpl_port.h"
#else
#define CPL_IS_LSB 1
#endif

#include "packedrtree.h"

namespace FlatGeobuf
{

void Rect::expand(const Rect& r)
{
    if (r.minX < minX) minX = r.minX;
    if (r.minY < minY) minY = r.minY;
    if (r.maxX > maxX) maxX = r.maxX;
    if (r.maxY > maxY) maxY = r.maxY;
}

Rect Rect::createInvertedInfiniteRect()
{
    return {
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        -1 * std::numeric_limits<double>::infinity(),
        -1 * std::numeric_limits<double>::infinity()
    };
}

bool Rect::intersects(const Rect& r) const
{
    if (maxX < r.minX) return false;
    if (maxY < r.minY) return false;
    if (minX > r.maxX) return false;
    if (minY > r.maxY) return false;
    return true;
}

std::vector<double> Rect::toVector()
{
    return std::vector<double> { minX, minY, maxX, maxY };
}

// Based on public domain code at https://github.com/rawrunprotected/hilbert_curves
uint32_t hilbert(uint32_t x, uint32_t y)
{
    uint32_t a = x ^ y;
    uint32_t b = 0xFFFF ^ a;
    uint32_t c = 0xFFFF ^ (x | y);
    uint32_t d = x & (y ^ 0xFFFF);

    uint32_t A = a | (b >> 1);
    uint32_t B = (a >> 1) ^ a;
    uint32_t C = ((c >> 1) ^ (b & (d >> 1))) ^ c;
    uint32_t D = ((a & (c >> 1)) ^ (d >> 1)) ^ d;

    a = A; b = B; c = C; d = D;
    A = ((a & (a >> 2)) ^ (b & (b >> 2)));
    B = ((a & (b >> 2)) ^ (b & ((a ^ b) >> 2)));
    C ^= ((a & (c >> 2)) ^ (b & (d >> 2)));
    D ^= ((b & (c >> 2)) ^ ((a ^ b) & (d >> 2)));

    a = A; b = B; c = C; d = D;
    A = ((a & (a >> 4)) ^ (b & (b >> 4)));
    B = ((a & (b >> 4)) ^ (b & ((a ^ b) >> 4)));
    C ^= ((a & (c >> 4)) ^ (b & (d >> 4)));
    D ^= ((b & (c >> 4)) ^ ((a ^ b) & (d >> 4)));

    a = A; b = B; c = C; d = D;
    C ^= ((a & (c >> 8)) ^ (b & (d >> 8)));
    D ^= ((b & (c >> 8)) ^ ((a ^ b) & (d >> 8)));

    a = C ^ (C >> 1);
    b = D ^ (D >> 1);

    uint32_t i0 = x ^ y;
    uint32_t i1 = b | (0xFFFF ^ (i0 | a));

    i0 = (i0 | (i0 << 8)) & 0x00FF00FF;
    i0 = (i0 | (i0 << 4)) & 0x0F0F0F0F;
    i0 = (i0 | (i0 << 2)) & 0x33333333;
    i0 = (i0 | (i0 << 1)) & 0x55555555;

    i1 = (i1 | (i1 << 8)) & 0x00FF00FF;
    i1 = (i1 | (i1 << 4)) & 0x0F0F0F0F;
    i1 = (i1 | (i1 << 2)) & 0x33333333;
    i1 = (i1 | (i1 << 1)) & 0x55555555;

    uint32_t value = ((i1 << 1) | i0);

    return value;
}

uint32_t hilbert(const Rect& r, uint32_t hilbertMax, const Rect& extent)
{
    uint32_t x = static_cast<uint32_t>(floor(hilbertMax * ((r.minX + r.maxX) / 2 - extent.minX) / extent.width()));
    uint32_t y = static_cast<uint32_t>(floor(hilbertMax * ((r.minY + r.maxY) / 2 - extent.minY) / extent.height()));
    uint32_t v = hilbert(x, y);
    return v;
}

const uint32_t hilbertMax = (1 << 16) - 1;

void hilbertSort(std::vector<std::shared_ptr<Item>> &items)
{
    Rect extent = std::accumulate(items.begin(), items.end(), Rect::createInvertedInfiniteRect(), [] (Rect a, std::shared_ptr<Item> b) {
        a.expand(b->rect);
        return a;
    });
    std::sort(items.begin(), items.end(), [&extent] (std::shared_ptr<Item> a, std::shared_ptr<Item> b) {
        uint32_t ha = hilbert(a->rect, hilbertMax, extent);
        uint32_t hb = hilbert(b->rect, hilbertMax, extent);
        return ha > hb;
    });
}

Rect calcExtent(const std::vector<Rect> &rects)
{
    Rect extent = std::accumulate(rects.begin(), rects.end(), Rect::createInvertedInfiniteRect(), [] (Rect a, const Rect& b) {
        a.expand(b);
        return a;
    });
    return extent;
}

Rect calcExtent(const std::vector<std::shared_ptr<Item>> &rectitems)
{
    Rect extent = std::accumulate(rectitems.begin(), rectitems.end(), Rect::createInvertedInfiniteRect(), [] (Rect a, std::shared_ptr<Item> b) {
        a.expand(b->rect);
        return a;
    });
    return extent;
}

void hilbertSort(std::vector<Rect> &items)
{
    Rect extent = calcExtent(items);
    std::sort(items.begin(), items.end(), [&extent] (const Rect& a, const Rect& b) {
        uint32_t ha = hilbert(a, hilbertMax, extent);
        uint32_t hb = hilbert(b, hilbertMax, extent);
        return ha > hb;
    });
}

void PackedRTree::init(const uint16_t nodeSize)
{
    if (nodeSize < 2)
        throw std::invalid_argument("Node size must be at least 2");
    if (_numItems == 0)
        throw std::invalid_argument("Cannot create empty tree");
    _nodeSize = std::min(std::max(nodeSize, static_cast<uint16_t>(2)), static_cast<uint16_t>(65535));
    _levelBounds = generateLevelBounds(_numItems, _nodeSize);
    _numNodes = _levelBounds.back();
    _numNonLeafNodes = static_cast<uint32_t>(_numNodes - _numItems);
    _minAlign = _numNonLeafNodes % 2;
    _rects.reserve(static_cast<size_t>(_numNodes));
    _indices.reserve(static_cast<size_t>(_numNonLeafNodes));
}

std::vector<uint64_t> PackedRTree::generateLevelBounds(const uint64_t numItems, const uint16_t nodeSize) {
    if (nodeSize < 2)
        throw std::invalid_argument("Node size must be at least 2");
    if (numItems == 0)
        throw std::invalid_argument("Number of items must be greater than 0");
    if (numItems > std::numeric_limits<uint64_t>::max() - ((numItems / nodeSize) * 2))
        throw std::overflow_error("Number of items too large");
    std::vector<uint64_t> levelBounds;
    uint64_t n = numItems;
    uint64_t numNodes = n;
    levelBounds.push_back(n);
    do {
        n = (n + nodeSize - 1) / nodeSize;
        numNodes += n;
        levelBounds.push_back(numNodes);
    } while (n != 1);
    return levelBounds;
}

void PackedRTree::generateNodes()
{
    for (uint32_t i = 0, pos = 0; i < _levelBounds.size() - 1; i++) {
        uint32_t end = static_cast<uint32_t>(_levelBounds[i]);
        while (pos < end) {
            Rect nodeRect = Rect::createInvertedInfiniteRect();
            uint32_t nodeIndex = pos;
            for (uint32_t j = 0; j < _nodeSize && pos < end; j++)
                nodeRect.expand(_rects[pos++]);
            _rects.push_back(nodeRect);
            _indices.push_back(nodeIndex);
        }
    }
}

void PackedRTree::fromData(const void *data)
{
    auto buf = reinterpret_cast<const uint8_t *>(data);
    const Rect *pr = reinterpret_cast<const Rect *>(buf);
    for (uint64_t i = 0; i < _numNodes; i++) {
        Rect r = *pr++;
        _rects.push_back(r);
        _extent.expand(r);
    }
    uint64_t rectsSize = _numNodes * sizeof(Rect);
    const uint32_t *pi = reinterpret_cast<const uint32_t *>(buf + rectsSize);
    for (uint32_t i = 0; i < _numNonLeafNodes; i++)
        _indices[i] = *pi++;
}

static std::vector<Rect> convert(const std::vector<std::shared_ptr<Item>> &items)
{
    std::vector<Rect> rects;
    for (const std::shared_ptr<Item> item: items)
        rects.push_back(item->rect);
    return rects;
}

PackedRTree::PackedRTree(const std::vector<std::shared_ptr<Item>> &items, const Rect& extent, const uint16_t nodeSize) :
    _extent(extent),
    _rects(convert(items)),
    _numItems(_rects.size())
{
    init(nodeSize);
    generateNodes();
}

PackedRTree::PackedRTree(const std::vector<Rect> &rects, const Rect& extent, const uint16_t nodeSize) :
    _extent(extent),
    _rects(rects),
    _numItems(_rects.size())
{
    init(nodeSize);
    generateNodes();
}

PackedRTree::PackedRTree(const void *data, const uint64_t numItems, const uint16_t nodeSize) :
    _extent(Rect::createInvertedInfiniteRect()),
    _numItems(numItems)
{
    init(nodeSize);
    fromData(data);
}

std::vector<uint64_t> PackedRTree::search(double minX, double minY, double maxX, double maxY) const
{
    Rect r { minX, minY, maxX, maxY };
    std::vector<uint64_t> queue;
    std::vector<uint64_t> results;
    queue.push_back(_rects.size() - 1);
    queue.push_back(_levelBounds.size() - 1);
    while(queue.size() != 0) {
        uint64_t nodeIndex = queue[queue.size() - 2];
        uint64_t level = queue[queue.size() - 1];
        queue.pop_back();
        queue.pop_back();
        // find the end index of the node
        uint64_t end = std::min(static_cast<uint64_t>(nodeIndex + _nodeSize), _levelBounds[static_cast<size_t>(level)]);
        // search through child nodes
        for (uint64_t pos = nodeIndex; pos < end; pos++) {
            if (!r.intersects(_rects[static_cast<size_t>(pos)]))
                continue;
            if (nodeIndex < _numItems) {
                results.push_back(pos); // leaf item
            } else {
                queue.push_back(_indices[static_cast<size_t>(pos - _numItems)]); // node; add it to the search queue
                queue.push_back(level - 1);
            }
        }
    }
    return results;
}

std::vector<uint64_t> PackedRTree::streamSearch(
    const uint64_t numItems, const uint16_t nodeSize, const Rect& r,
    const std::function<void(uint8_t *, size_t, size_t)> &readNode)
{
    auto levelBounds = generateLevelBounds(numItems, nodeSize);
    uint64_t numNodes = levelBounds.back();
    std::vector<uint32_t> nodeIndices;
    nodeIndices.reserve(nodeSize);
    uint8_t *nodeIndicesBuf = reinterpret_cast<uint8_t *>(nodeIndices.data());
    std::vector<Rect> nodeRects;
    nodeRects.reserve(nodeSize);
    uint8_t *nodeRectsBuf = reinterpret_cast<uint8_t *>(nodeRects.data());
    std::vector<uint64_t> queue;
    std::vector<uint64_t> results;
    queue.push_back(numNodes - 1);
    queue.push_back(levelBounds.size() - 1);
    while(queue.size() != 0) {
        uint64_t nodeIndex = queue[queue.size() - 2];
        bool isLeafNode = nodeIndex < numItems;
        uint64_t level = queue[queue.size() - 1];
        queue.pop_back();
        queue.pop_back();
        // find the end index of the node
        uint64_t end = std::min(static_cast<uint64_t>(nodeIndex + nodeSize), levelBounds[static_cast<size_t>(level)]);
        uint64_t length = end - nodeIndex;
        if (!isLeafNode) {
            auto offset = numNodes * sizeof(Rect) + (nodeIndex - numItems) * sizeof(uint32_t);
            readNode(nodeIndicesBuf, static_cast<size_t>(offset), static_cast<size_t>(length * sizeof(uint32_t)));
#if !CPL_IS_LSB
            for( size_t i = 0; i < static_cast<size_t>(length); i++ )
            {
                CPL_LSBPTR32(&nodeIndices[i]);
            }
#endif
        }
        readNode(nodeRectsBuf, static_cast<size_t>(nodeIndex * sizeof(Rect)), static_cast<size_t>(length * sizeof(Rect)));
#if !CPL_IS_LSB
        for( size_t i = 0; i < static_cast<size_t>(length); i++ )
        {
            CPL_LSBPTR64(&nodeRects[i].minX);
            CPL_LSBPTR64(&nodeRects[i].minY);
            CPL_LSBPTR64(&nodeRects[i].maxX);
            CPL_LSBPTR64(&nodeRects[i].maxY);
        }
#endif
        // search through child nodes
        for (uint64_t pos = nodeIndex; pos < end; pos++) {
            uint64_t nodePos = pos - nodeIndex;
            if (!r.intersects(nodeRects[static_cast<size_t>(nodePos)]))
                continue;
            if (isLeafNode) {
                results.push_back(pos); // leaf item
            } else {
                queue.push_back(nodeIndices[static_cast<size_t>(nodePos)]); // node; add it to the search queue
                queue.push_back(level - 1);
            }
        }
    }
    return results;
}

uint64_t PackedRTree::size() const { return _numNodes * sizeof(Rect) + (_numNonLeafNodes + _minAlign) * sizeof(uint32_t); }

uint64_t PackedRTree::size(const uint64_t numItems, const uint16_t nodeSize)
{
    if (nodeSize < 2)
        throw std::invalid_argument("Node size must be at least 2");
    if (numItems == 0)
        throw std::invalid_argument("Number of items must be greater than 0");
    const uint16_t nodeSizeMin = std::min(std::max(nodeSize, static_cast<uint16_t>(2)), static_cast<uint16_t>(65535));
    // limit so that resulting size in bytes can be represented by uint64_t
    if (numItems > static_cast<uint64_t>(1) << 56)
        throw std::overflow_error("Number of items must be less than 2^56");
    uint64_t n = numItems;
    uint64_t numNodes = n;
    do {
        n = (n + nodeSizeMin - 1) / nodeSizeMin;
        numNodes += n;
    } while (n != 1);
    const uint64_t numNonLeafNodes = numNodes - numItems;
    const uint32_t minAlign = numNonLeafNodes % 2;
    return numNodes * sizeof(Rect) + (numNonLeafNodes + minAlign) * sizeof(uint32_t);
}

void PackedRTree::streamWrite(const std::function<void(uint8_t *, size_t)> &writeData) {
#if !CPL_IS_LSB
    // Note: we should normally revert endianness after writing, but as we no longer
    // use the data structures this is not needed.
    for( size_t i = 0; i < _rects.size(); i++ )
    {
        CPL_LSBPTR64(&_rects[i].minX);
        CPL_LSBPTR64(&_rects[i].minY);
        CPL_LSBPTR64(&_rects[i].maxX);
        CPL_LSBPTR64(&_rects[i].maxY);
    }
    for( size_t i = 0; i < _indices.size(); i++ )
    {
        CPL_LSBPTR32(&_indices[i]);
    }
#endif
    writeData(reinterpret_cast<uint8_t *>(_rects.data()), _rects.size() * sizeof(Rect));
    writeData(reinterpret_cast<uint8_t *>(_indices.data()), _indices.size() * sizeof(uint32_t));
    writeData(reinterpret_cast<uint8_t *>(_indices.data()), (_numNonLeafNodes % 2) * sizeof(uint32_t));
}

Rect PackedRTree::getExtent() const { return _extent; }

}