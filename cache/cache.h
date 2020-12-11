#include <unordered_map>
#include <string>
#include <vector>

/** Stores basic information about a texture. */
struct Texture
{
    std::string name;
    int width;
    int height;
};

/** Simulates an LRU texture cache. */
template<class S>
class TCache
{
public:
    const int nSets;    /** The number of sets in the cache. */
    const int nWays;    /** The number of ways in each set. */
    const int bWidth;   /** The width in pixels of a block. */
    const int bHeight;  /** The height in pixels of a block. */
    std::vector<double> blockFreq;

    /**
     * Instantiates a set-associative LRU texture cache.
     * @param nSets the number of sets in the cache
     * @param nWays the number of ways in each set
     * @param bWidth the width in pixels of a block
     * @param bHeight the height in pixels of a block
     */
    TCache(int nSets, int nWays, int bWidth, int bHeight);

    /**
     * Adds a texture for use in the cache.
     * @param name the name of the texture
     * @param width the width of the texture
     * @param height the height of the texture
     */
    void addTexture(const std::string &name, int width, int height, int freq);

    /**
     * Computes a linear index from a texture and an offset into the texture.
     * @param name the name of the texture
     * @param offset the row-major pixel offset into the texture
     * @return the linear index of the texture-offset pair
     */
    int index(const std::string &name, int offset) const;

    /**
     * Looks up an offset in the given texture, returning whether it was a hit.
     * @param name the name of the texture
     * @param offset the row-major pixel offset into the texture
     * @return whether the lookup was a hit.
     */
    bool lookup(const std::string &name, int offset);

    void print();

private:
    int nTextures, nBlocks;
    std::unordered_map<std::string, int> texIdMap;
    std::vector<int> texStartIdx;
    std::vector<Texture> textures;
    std::vector<S> sets;
};


class LRUSet
{
public:
    LRUSet(TCache<LRUSet> *cache, int nWays);
    bool lookup(int index);
    void print();
private:
    int nWays;
    int *set;
};

template<double (*S)(int, double)>
class FPQSet
{
public:
    FPQSet(TCache<FPQSet> *cache, int nWays);
    bool lookup(int index);
    void print();
private:
    TCache<FPQSet> *cache;
    int nWays;
    int *set;
};

double score_lru(int rank, double freq);
double score_freq(int rank, double freq);
double score_weighted(int rank, double freq);
double score2(int rank, double freq);

extern double weight;