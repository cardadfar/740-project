#include "cache.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <unistd.h>
#include <cstdlib>
#include <unordered_map>
using namespace std;

template<class S>
void printLookup(TCache<S> &cache, string name, int offset)
{
    cout << "Looking up texture " << name << " at offset " << offset << ": ";
    int index = cache.index(name, offset);
    bool hit = cache.lookup(name, offset);
    cout << (hit ? "Hit" : "Miss") << endl;
    cout << "  index = " << index << ", set = " << index % cache.nWays << endl;
}

template<class S>
void print_cache_settings(const TCache<S> &cache)
{
    cout << "Cache Settings:" << endl;
    cout << "  " << cache.nSets << " sets, ";
    cout << cache.nWays << " ways per set, ";
    cout << cache.bWidth << "x" << cache.bHeight << " block size" << endl;
}

void print_usage()
{
    cerr << "Usage: tcache [-h] [-p] [-l] [-f FREQ] ";
    cerr << "access nsets nways bw bh" << endl;
}

int parse_int(char **argv)
{
    char *arg = argv[optind++];
    int x = strtol(arg, NULL, 10);
    if (x <= 0)
    {
        cerr << "Invalid argument `" << arg << "'." << endl;
        print_usage();
        exit(1);
    }
    return x;
}

int main(int argc, char **argv)
{
    char *freq_fname = NULL;
    bool pretty = false;
    bool log = false;

    int c;
    while ((c = getopt(argc, argv, "hplf:w:")) != -1)
    {
        switch (c)
        {
        case 'h':
            print_usage();
            exit(0);
        case 'p':
            pretty = true;
            break;
        case 'l':
            log = true;
            break;
        case 'f':
            freq_fname = optarg;
            break;
        case 'w':
            weight = strtod(optarg, NULL);
            break;
        case '?':
            break;
        default:
            if (optopt == 'f')
                cerr << "Option -f requires an argument." << endl;
            else if (isprint(optopt))
                cerr << "Unknown option `-" << (char)optopt << "'." << endl;
            else
                cerr << "Unknown option character " << optopt << "." << endl;
            print_usage();
            exit(1);
        }
    }

    if (argc - optind < 5)
    {
        cerr << "Missing arguments(s)." << endl;
        print_usage();
        exit(1);
    }

    if (argc - optind > 5)
    {
        cerr << "Too many arguments." << endl;
        print_usage();
        exit(1);
    }

    char *access_fname = argv[optind++];
    int nSets = parse_int(argv);
    int nWays = parse_int(argv);
    int bWidth = parse_int(argv);
    int bHeight = parse_int(argv);

    TCache<FPQSet<score_weighted> > cache(nSets, nWays, bWidth, bHeight);
    //TCache<FPQSet<score2> > cache(nSets, nWays, bWidth, bHeight);

    unordered_map<string, int> freq_map;

    if (freq_fname)
    {
        ifstream fin(freq_fname);
        if (fin.fail())
        {
            cerr << "Unable to open file `" << freq_fname << "'." << endl;
            abort();
        }
        string line;
        string name;
        int freq;
        while (getline(fin, line))
        {
            istringstream iss(line);
            if (iss >> name >> freq)
                freq_map[name] = freq;
        }
    }

    ifstream ain(access_fname);
    if (ain.fail())
    {
        cerr << "Unable to open file `" << access_fname << "'." << endl;
        abort();
    }

    if (log)
    {
        cout << nSets << " " << nWays << endl;
        cache.print();
    }

    chrono::steady_clock::time_point begin = chrono::steady_clock::now();

    string line;
    int x, y, tid, q, w, h, ox, oy;
    int total = 0;
    int misses = 0;
    while (getline(ain, line))
    {
        istringstream iss(line);
        if (iss >> x >> y >> tid >> q >> w >> h >> ox >> oy)
        {
            if (iss >> x)
            {
                cerr << total << " " << line << endl;
                abort();
            }
            ostringstream oss;
            oss << tid << "-" << q;
            string name = oss.str();
            int offset = oy * w + ox;
            cache.addTexture(name, w, h, freq_fname ? freq_map[name] : 0);
            if (!cache.lookup(name, offset))
                misses++;
            if (log)
            {
                cout << line << endl;
                cache.print();
            }
        }
        total++;
    }

    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    chrono::milliseconds elapsed = chrono::duration_cast<chrono::milliseconds>(end - begin);

    ain.close();
    
    if (log)
        return 0;

    if (pretty)
    {
        print_cache_settings(cache);
        cout << "Hits:   " << total - misses << endl;
        cout << "Misses: " << misses << endl;
        cout << "Total:  " << total << endl;
        cout << "MRate:  " << (double)misses / total << endl;
        cout << endl << "Elapsed time: ";
        cout << elapsed.count() << endl;
    }
    else
    {
        cout << "nSets " << nSets << endl;
        cout << "nWays " << nWays << endl;
        cout << "bWidth " << bWidth << endl;
        cout << "bHeight " << bHeight << endl;
        cout << "misses " << misses << endl;
        cout << "total " << total << endl;
        cout << "elapsed " << elapsed.count() << endl;
    }

    return 0;
}