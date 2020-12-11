#include "cache.h"
#include <limits>
#include <cassert>
#include <iostream>
#include <cmath>
using namespace std;

template class FPQSet<score_lru>;
template class FPQSet<score_freq>;
template class FPQSet<score_weighted>;
template class TCache<LRUSet>;
template class TCache<FPQSet<score_lru> >;
template class TCache<FPQSet<score_freq> >;
template class TCache<FPQSet<score_weighted> >;
template class TCache<FPQSet<score2> >;

double weight = 0.5;
bool verbose = false;

inline int ceilDiv(const int &num, const int &den)
{
    return (num + den - 1) / den;
}

unsigned int myHash(unsigned int x)
{
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

template<class S>
TCache<S>::TCache(int nSets, int nWays, int bWidth, int bHeight) :
    nSets(nSets), nWays(nWays), bWidth(bWidth), bHeight(bHeight)
{
    assert(nSets > 0 && nWays > 0 && bWidth > 0 && bHeight > 0);
    nTextures = 0;
    nBlocks = 0;
    texStartIdx.push_back(nBlocks);

    for (int i = 0; i < nSets; i++)
        sets.push_back(S(this, nWays));
}

template<class S>
void TCache<S>::addTexture(const string &name, int width, int height, int freq)
{
    assert(width >= 0 && height >= 0);
    if (texIdMap.count(name) > 0)
        return;

    int bRows = max(1, ceilDiv(height, bHeight));
    int bCols = max(1, ceilDiv(width, bWidth));
    int bCount = bRows * bCols;

    texIdMap[name] = nTextures;
    nTextures++;
    
    nBlocks += bCount;
    texStartIdx.push_back(nBlocks);
    textures.push_back((Texture){name, max(1, width), max(1, height)});
    double freqDiv = freq / (double)bCount;
    for (int i = 0; i < bCount; i++)
        blockFreq.push_back(freqDiv);
}

template<class S>
int TCache<S>::index(const string &name, int offset) const
{
    assert(offset >= 0);
    if (texIdMap.count(name) == 0)
        return -1;
    int tIndex = texIdMap.at(name);
    Texture tex = textures[tIndex];

    int row = offset / tex.width;
    int col = offset % tex.width;
    
    int bRow = row / bHeight;
    int bCol = col / bWidth;
    
    int bCols = ceilDiv(tex.width, bWidth);
    int bIndex = bRow * bCols + bCol;
    return texStartIdx[tIndex] + bIndex;
}

template<class S>
bool TCache<S>::lookup(const string &name, int offset)
{
    int idx = index(name, offset);
    if (idx == -1)
        return false;
    unsigned int setIdx = myHash(idx) % nSets;
    // int setIdx = idx % nSets;
    return sets[setIdx].lookup(idx);
}

template<class S>
void TCache<S>::print()
{
    for (int i = 0; i < nSets; i++)
    {
        sets[i].print();
    }
}

LRUSet::LRUSet(TCache<LRUSet> *cache, int nWays) : nWays(nWays)
{
    assert(nWays > 0);
    set = new int[nWays];
    for (int i = 0; i < nWays; i++)
        set[i] = -1;    
}

bool LRUSet::lookup(int index)
{
    // Check for a hit, and make it MRU if it is a hit
    for (int i = 0; i < nWays; i++)
    {
        if (set[i] == index)
        {
            for (int j = i; j > 0; j--)
                set[j] = set[j - 1];
            set[0] = index;
            return true;
        }
    }

    // Otherwise, it is a miss, and insert it as MRU
    for (int i = 1; i < nWays; i++)
        set[i] = set[i - 1];
    set[0] = index;
    return false;
}

void LRUSet::print()
{
    for (int i = 0; i < nWays; i++)
    {
        if (i > 0)
            cout << " ";
        cout << set[i];
    }
    cout << endl;
}

template<double (*S)(int, double)>
FPQSet<S>::FPQSet(TCache<FPQSet<S> > *cache, int nWays) :
    cache(cache), nWays(nWays)
{
    assert(cache != NULL && nWays > 0);
    set = new int[nWays];
    for (int i = 0; i < nWays; i++)
        set[i] = -1;
}

template<double (*S)(int, double)>
bool FPQSet<S>::lookup(int index)
{
    assert(index < cache->blockFreq.size());
    cache->blockFreq[index]--;

    // Check for a hit, and make it MRU if it is a hit
    for (int i = 0; i < nWays; i++)
    {
        if (set[i] == index)
        {
            for (int j = i; j > 0; j--)
                set[j] = set[j - 1];
            set[0] = index;
            return true;
        }
    }

    // Otherwise, it is a miss, and find the lowest-scoring item
    int min_i = -1;
    double min_score = numeric_limits<double>::max();
    for (int i = 0; i < nWays; i++)
    {
        if (set[i] == -1)
        {
            min_i = i;
            min_score = numeric_limits<double>::min();
            break;
        }
        double freq = cache->blockFreq[set[i]];
        double score = S(nWays - i - 1, freq);
        if (score < min_score)
        {
            min_i = i;
            min_score = score;
        }
    }
    assert(min_i >= 0);

    // Evict the lowest-scoring item and insert the new one as MRU
    double new_score = S(nWays, cache->blockFreq[index]);
    if (new_score > min_score)
    {
        for (int i = min_i; i > 0; i--)
            set[i] = set[i - 1];
        set[0] = index;
    }
    return false;
}

template<double (*S)(int, double)>
void FPQSet<S>::print()
{
    for (int i = 0; i < nWays; i++)
    {
        if (i > 0)
            cout << " ";
        cout << set[i];
    }
    cout << endl;
}

double score_lru(int rank, double freq)
{
    return rank;
}

double score_freq(int rank, double freq)
{
    return freq;
}

double score_weighted(int rank, double freq)
{
    return rank + weight * log(max(freq, 1.0));
}

double score2(int rank, double freq)
{
    return rank * freq;
}