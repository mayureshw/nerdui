#ifndef _ORDMAP_H
#define _ORDMAP_H

#include <unordered_map>
#include <vector>
#include <utility>
#include <iterator>

template<typename K, typename V>
class OrderedMap
{
    using Map = unordered_map<K,V>;
    using MapIter = typename Map::iterator;
    Map _map;
    vector<MapIter> _order;
    pair<MapIter,bool> try_emplace(const K& key, const V& value)
    {
        auto [it, inserted] = _map.try_emplace(key, value);
        if (inserted) _order.push_back(it);
        return {it, inserted};
    }
public:
    const vector<MapIter>& as_vec() const { return _order; }
    const Map& as_map() const { return _map; }
    template<typename... Args>
    pair<MapIter,bool> emplace(Args&&... args)
    {
        auto [it, inserted] = _map.emplace(forward<Args>(args)...);
        if (inserted) _order.push_back(it);
        return {it, inserted};
    }
    MapIter find(const K& key) { return _map.find(key); }
    auto find(const K& key) const { return _map.find(key); }
    bool contains(const K& key) const { return _map.find(key) != _map.end(); }
    size_t size() const { return _map.size(); }
    bool empty() const { return _map.empty(); }
    void clear() { _map.clear(); _order.clear(); }
};

#endif
