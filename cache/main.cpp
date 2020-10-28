#include "cache.h"
#include <iostream>
#include <string>
using namespace std;

void printLookup(TCache &cache, string name, int offset)
{
    cout << "Looking up texture " << name << " at offset " << offset << ": ";
    int index = cache.index(name, offset);
    bool hit = cache.lookup(name, offset);
    cout << (hit ? "Hit" : "Miss") << endl;
    cout << "  index = " << index << ", set = " << index % cache.nWays << endl;
}

int main()
{
    TCache cache(4, 2, 3, 3);
    cache.addTexture("A", 7, 9); // 3x3 blocks
    cache.addTexture("B", 13, 4); // 2x5 blocks

    // cout << cache.index("A", 10) << endl; // expect (0, 1) -> 1
    // cout << cache.index("A", 40) << endl; // expect (1, 1) -> 4
    // cout << cache.index("A", 62) << endl; // expect (2, 2) -> 8
    // cout << cache.index("B", 10) << endl; // expect (0, 3) -> 9 + 3 -> 12
    printLookup(cache, "A", 10);
    printLookup(cache, "A", 40);
    printLookup(cache, "A", 62);
    printLookup(cache, "A", 55);
    printLookup(cache, "A", 40);
    printLookup(cache, "B", 10);
    printLookup(cache, "A", 62);
}