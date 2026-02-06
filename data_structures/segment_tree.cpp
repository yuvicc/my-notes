#include <array>
#include <cstddef>
#include <iostream>
#include <vector>

typedef long long ll;

struct SegmentTree {
    std::size_t size;
    std::vector<ll> tree;
    
    SegmentTree(const std::vector<ll>& arr) {
        size_t n = arr.size();
        tree.assign(4 * n, 0);
        build(arr, 1, 0, n - 1);
    }

    void build(const std::vector<ll>& arr, int v, int tl, int tr) {
        if (tl == tr) {
            tree[v] = arr[tl];
        } else {
            int tm = tl + (tr - tl) / 2;
            build(arr, v * 2, tl, tm);
            build(arr, v * 2 + 1, tm + 1, tr);
            tree[v] = tree[2 * v] + tree[2 * v + 1];
        }
    }

    ll sum(int v, int tl, int tr, int l, int r) {
        if (l > r) return 0;
        if (l == tl && r == tr) return tree[v];
        int tm = tl + (tr - tl) / 2;
        return sum(2 * v, tl, tm, l, std::min(r, tm)) + sum(2 * v + 1, tm + 1, tr, std::max(l, tm + 1), r);
    }

    void update(int v, int tl, int tr, int pos, int val) {
        if(tl == tr) {
            tree[v] = val;
        } else {
            int tm = tl + (tr - tl) / 2;
            if (pos <= tm) {
                update(2 * v, tl, tm, pos, val);
            } else {
                update(2 * v + 1, tm + 1, tr, pos, val);
            }
            tree[v] = tree[2 * v] + tree[2 * v + 1];
        }
    }

    void update(int pos, int val) { return update(1, 0, n - 1, pos, val); }
};
