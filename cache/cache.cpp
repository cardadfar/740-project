#include "cache.h"
#include <cassert>
using namespace std;

inline int ceilDiv(const int &num, const int &den)
{
    return (num + den - 1) / den;
}

TCache::TCache(int nSets, int nWays, int bWidth, int bHeight) :
    nSets(nSets), nWays(nWays), bWidth(bWidth), bHeight(bHeight)
{
    assert(nSets > 0 && nWays > 0 && bWidth > 0 && bHeight > 0);
    nTextures = 0;
    nBlocks = 0;
    texStartIdx.push_back(nBlocks);

    sets = new int*[nSets];
    for (int i = 0; i < nSets; i++)
    {
        sets[i] = new int[nWays];
        for (int j = 0; j < nWays; j++)
            sets[i][j] = -1;
    }
}

void TCache::addTexture(const string &name, int width, int height)
{
    assert(width > 0 && height > 0);
    if (texIdMap.count(name) > 0)
        return;

    int bRows = ceilDiv(height, bHeight);
    int bCols = ceilDiv(width, bWidth);

    texIdMap[name] = nTextures;
    nTextures++;
    
    nBlocks += bRows * bCols;
    texStartIdx.push_back(nBlocks);

    textures.push_back((Texture){name, width, height});
}

int TCache::index(const string &name, int offset) const
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

bool TCache::lookup(const string &name, int offset)
{
    int idx = index(name, offset);
    if (idx == -1)
        return false;
    int setIdx = idx % nSets;
    int *set = sets[setIdx];

    // Check for a hit, and make it MRU if it is a hit
    for (int i = 0; i < nWays; i++)
    {
        if (set[i] == idx)
        {
            for (int j = 1; j < i; j++)
            {
                set[j] = set[j - 1];
            }
            set[0] = idx;
            return true;
        }
    }

    // Otherwise, it is a miss, and insert it as MRU
    for (int i = 1; i < nWays; i++)
    {
        set[i] = set[i - 1];
    }
    set[0] = idx;
    return false;
}