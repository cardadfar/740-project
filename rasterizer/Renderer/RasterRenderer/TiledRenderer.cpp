#include "RendererImplBase.h"
#include "CommonTraceCollection.h"
#include <algorithm>
#include <immintrin.h>
#include <queue>
#include <cassert>

using namespace std;

namespace RasterRenderer
{
    class TiledRendererAlgorithm
    {
    private:

        // starter code uses 32x32 tiles, feel free to change
        static const int Log2TileSize = 5;
        static const int TileSize = 1 << Log2TileSize;

        // 15-869 hint: a compact representation of a frame buffer tile
        // Use if you wish (optional).  (What other data-structures might be necessary?)

        vector<FrameBuffer> frameBufferTiles;
        vector<vector<queue<int>>> frameBufferQueues;

        // render target is grid of tiles: see SetFrameBuffer
        int gridWidth, gridHeight;
        FrameBuffer * frameBuffer;
    public:
        inline void Init()
        {

        }

        inline void Clear(const Vec4 & clearColor, bool color, bool depth)
        {
            for (auto & fb : frameBufferTiles)
                fb.Clear(clearColor, color, depth);

            frameBuffer->Clear(clearColor, color, depth);

        }

        inline void SetFrameBuffer(FrameBuffer * frameBuffer)
        {
            this->frameBuffer = frameBuffer;

            // compute number of necessary bins
            gridWidth = frameBuffer->GetWidth() >> Log2TileSize;
            gridHeight = frameBuffer->GetHeight() >> Log2TileSize;
            if (frameBuffer->GetWidth() & (TileSize - 1))
                gridWidth++;
            if (frameBuffer->GetHeight() & (TileSize - 1))
                gridHeight++;


            for(int y = 0; y < frameBuffer->GetHeight(); y++) {
                for(int x = 0; x < frameBuffer->GetWidth(); x++) {
                    frameBuffer->SetZ(x,y,0,FLT_MAX);
                }
            }


            // 15-869 note: Implementations may want to allocation/initialize any
            // necessary data-structures here.  See definitions
            // labeled "optional 15-869 hint" above.

            for(int h = 0; h < gridHeight; h++) {
                for(int w = 0; w < gridWidth; w++) {
                    FrameBuffer *f = new FrameBuffer(TileSize, TileSize);
                    frameBufferTiles.push_back(*f);
                }
            }
            
            for(int i = 0; i < gridWidth*gridHeight; i++) {
                vector<queue<int>> vq;
                frameBufferQueues.push_back(vq);
                for(int c = 0; c < Cores; c++) {
                    queue<int> q;
                    frameBufferQueues[i].push_back(q);
                }
            }




        }

        inline void Finish()
        {
            // 15-869 note: Finish() is called at the end of the frame. If it hasn't done so, your
            // implementation should flush local per-tile framebuffer contents to the global
            // frame buffer (this->frameBuffer) here.

        }

        int time = 0;

        inline void BinTriangles(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize, int threadId)
        {

            // 15-869 note: Keep in mind BinTriangles is called once per worker thread.
            // Each thread grabs the list of triangles is it responsible
            // for, and should bin them here.

            auto & triangles = input.triangleBuffer[threadId];

            // for the C++11 inclined:
            // the code below could have also iterated like this: for (auto & tri : triangles) { }
            for (int i = 0; i < triangles.Count(); i++)
            {
                auto & tri = triangles[i];
                // process tri into bins here

                int id = (i << 4) | threadId;
                
                int TminX = min(min(tri.X0, tri.X1), tri.X2);
                int TminY = min(min(tri.Y0, tri.Y1), tri.Y2);
                int TmaxX = max(max(tri.X0, tri.X1), tri.X2);
                int TmaxY = max(max(tri.Y0, tri.Y1), tri.Y2);

                TminX = (TminX >> 4) - 1;
                TminY = (TminY >> 4) - 1;
                TmaxX = (TmaxX >> 4) + 1;
                TmaxY = (TmaxY >> 4) + 1;
                
                int x_lo = floor((float)TminX/(float)TileSize);
                int y_lo = floor((float)TminY/(float)TileSize);
                int x_hi =  ceil((float)TmaxX/(float)TileSize);
                int y_hi =  ceil((float)TmaxY/(float)TileSize);

                if(x_lo < 0) { x_lo = 0; }
                if(y_lo < 0) { y_lo = 0; }
                if(x_hi > gridWidth-1)  { x_hi = gridWidth-1; }
                if(y_hi > gridHeight-1) { y_hi = gridHeight-1; }

                for(int y = y_lo; y <= y_hi; y++) {
                    for(int x = x_lo; x <= x_hi; x++) {
                        frameBufferQueues[y*gridWidth+x][threadId].push(id);
                    }
                }

            }



        }

        inline void ProcessBin(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize, int tileId)
        {
            // 15-869 note: this thread should process the bin of triangles corresponding to
            // render target tile tileId

            // Keep in mind that in a tiled renderer, rasterization and fragment processing
            // is parallelized across tiles, not fragments within a tile.  Processing within a tile
            // is carried out sequentially. Thus your implementation will likely call ShadeFragment()
            // to shade a single quad fragment at a time, rather than call ShadeFragments()
            // as was done on the full fragment buffer in the non-tiled reference
            // implementation. (both functions are defined in RendererImplBase.h).
            //
            // Some example code is given below:
            //
            // let 'tri' be a triangle
            // let 'triangleId' be its position in the original input list
            // let 'triSIMD' be the triangle info loaded into SIMD registers (see reference impl)

            // // get triangle barycentric coordinates
            // __m128 gamma, beta, alpha;
            // triSIMD.GetCoordinates(gamma, alpha, beta, coordX_center, coordY_center);
            // auto z = triSIMD.GetZ(coordX_center, coordY_center);
            //
            // // call ShadeFragment to shade a single quad fragment, storing the output colors (4 float4's) in shadeResult
            // CORE_LIB_ALIGN_16(float shadeResult[16]);
            //ShadeFragment(state, shadeResult, beta, gamma, alpha, triangleId, tri.ConstantId, VERTEX_BUFFER_FOR_TRIANGLE, vertexOutputSize,
            //              INDEX_BUFFER_FOR_TRIANGLE);

            
            vector<queue<int>> queues = frameBufferQueues[tileId];

            FrameBuffer fb = frameBufferTiles[tileId];

            int wOffset = (tileId % gridWidth) * TileSize;
            int hOffset = (tileId / gridWidth) * TileSize;
            int width  = min(TileSize, frameBuffer->GetWidth() - wOffset);
            int height = min(TileSize, frameBuffer->GetHeight() - hOffset);
            
            // note: set_epi32 works in reverse order: (v3,v2,v1,v0)
            static __m128i xOffset = _mm_set_epi32(24, 8, 24, 8);
            static __m128i yOffset = _mm_set_epi32(24, 24, 8, 8);

            for(int i = 0; i < frameBufferQueues[tileId].size(); i++) {
                
                while(!frameBufferQueues[tileId][i].empty()) {

                    int id = frameBufferQueues[tileId][i].front();
                    frameBufferQueues[tileId][i].pop();

                    int i = id >> 4;
                    int t = id & 0xF;

                    auto tri = input.triangleBuffer[t][i];
                    int triangleId = tri.Id;

                    TriangleSIMD triSIMD;
                    triSIMD.Load(tri);
                    
                    RasterizeTriangle(wOffset, hOffset, width, height, tri, triSIMD, [&](int qfx, int qfy, bool trivialAccept)
                    {
   
                        __m128i coordX_center, coordY_center;
                        coordX_center = _mm_add_epi32(_mm_set1_epi32(qfx << 4), xOffset);
                        coordY_center = _mm_add_epi32(_mm_set1_epi32(qfy << 4), yOffset);

                        int x = qfx;
                        int y = qfy;

                        int xf = x % TileSize;
                        int yf = y % TileSize;


                        // perform coverage test for all samples, if necessary
                        int coverageMask = trivialAccept ? 0xFFFF : triSIMD.TestQuadFragment(coordX_center, coordY_center);

                        // evaluate Z for each sample point
                        auto zValues = triSIMD.GetZ(coordX_center, coordY_center);

                        // copy z values out of SIMD register
                        CORE_LIB_ALIGN_16(float zStore[4]);
                        _mm_store_ps(zStore, zValues);

                        FragmentCoverageMask visibility;

                        if (coverageMask & 0x0008)
                        {
                            if(xf < 0 || xf >= TileSize || yf < 0 || yf >= TileSize) { /* out of bounds */ }
                            else {
                                if (fb.GetZ(xf, yf, 0) > zStore[0]) {
                                    visibility.SetBit(0);
                                    fb.SetZ(xf, yf, 0, zStore[0]);
                                }
                            }
                        }
                        if (coverageMask & 0x0080)
                        {
                            if(xf+1 < 0 || xf+1 >= TileSize || yf < 0 || yf >= TileSize) { /* out of bounds */ }
                            else {
                                if (fb.GetZ(xf + 1, yf, 0) > zStore[1]) {
                                    visibility.SetBit(1);
                                    fb.SetZ(xf + 1, yf, 0, zStore[1]);
                                }
                            }
                        }
                        if (coverageMask & 0x0800)
                        {
                            if(xf < 0 || xf >= TileSize || yf+1 < 0 || yf+1 >= TileSize) { /* out of bounds */ }
                            else {
                                if (fb.GetZ(xf, yf + 1, 0) > zStore[2]) {
                                    visibility.SetBit(2);
                                    fb.SetZ(xf, yf + 1, 0, zStore[2]);
                                }
                            }
                        }
                        if (coverageMask & 0x8000)
                        {
                            if(xf+1 < 0 || xf+1 >= TileSize || yf+1 < 0 || yf+1 >= TileSize) { /* out of bounds */ }
                            else {
                                if (fb.GetZ(xf + 1, yf + 1, 0) > zStore[3]) {
                                    visibility.SetBit(3);
                                    fb.SetZ(xf + 1, yf + 1, 0, zStore[3]);
                                }
                            }
                        }
                        
                        if (visibility.Any())
                        {
                            
                            __m128 gamma, beta, alpha;
                            triSIMD.GetCoordinates(gamma, alpha, beta, coordX_center, coordY_center);
                            
                            CORE_LIB_ALIGN_16(float shadeResult[16]);

                            ShadeFragment(state, shadeResult, beta, gamma, alpha, i, tri.ConstantId, input.vertexOutputBuffer[t].Buffer(), vertexOutputSize, input.indexOutputBuffer[t].Buffer());
                            
                            if(visibility.GetBit(0)) {
                                float depth = fb.GetZ(xf, yf, 0);
                                if(frameBuffer->GetZ(x, y, 0) > depth) {
                                    frameBuffer->SetPixel(x, y, 0, Vec4(shadeResult[0], shadeResult[4], shadeResult[8], shadeResult[12]));
                                    frameBuffer->SetZ(x, y, 0, depth);
                                }
                            }
                            if(visibility.GetBit(1)) {
                                float depth = fb.GetZ(xf+1, yf, 0);
                                if(frameBuffer->GetZ(x+1, y, 0) > depth) {
                                    frameBuffer->SetPixel(x+1, y  , 0, Vec4(shadeResult[1], shadeResult[5], shadeResult[9], shadeResult[13]));
                                    frameBuffer->SetZ(x+1, y, 0, depth);
                                }
                            }
                            if(visibility.GetBit(2)) {
                                float depth = fb.GetZ(xf, yf+1, 0);
                                if(frameBuffer->GetZ(x, y+1, 0) > depth) {
                                    frameBuffer->SetPixel(x  , y+1, 0, Vec4(shadeResult[2], shadeResult[6], shadeResult[10], shadeResult[14]));
                                    frameBuffer->SetZ(x, y+1, 0, depth);
                                }
                            }
                            if(visibility.GetBit(3)) {
                                float depth = fb.GetZ(xf+1, yf+1, 0);
                                if(frameBuffer->GetZ(x+1, y+1, 0) > depth) {
                                    frameBuffer->SetPixel(x+1, y+1, 0, Vec4(shadeResult[3], shadeResult[7], shadeResult[11], shadeResult[15]));
                                    frameBuffer->SetZ(x+1, y+1, 0, depth);
                                }
                            }
                            
                        }
                        
                        
                    });



                }
            }
            

            /*
            for(int i = 0; i < frameBufferQueues[tileId].size(); i++) {
                if(frameBufferQueues[tileId][i].size() != 0)
                    printf("%d\n", frameBufferQueues[tileId][i].size());
            }
            */

        }


        inline void RenderProjectedBatch(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize)
        {
            // Pass 1:
            //
            // The renderer is structured so that input a set of triangle lists
            // (exactly one list of triangles for each core to process).
            // As shown in BinTriangles() above, each thread bins the triangles in
            // input.triangleBuffer[threadId]
            //
            // Below we create one task per core (i.e., one thread per
            // core).  That task should bin all the triangles in the
            // list it is provided into bins: via a call to BinTriangles

            time++;


            Parallel::For(0, Cores, 1, [&](int threadId)
            {
                BinTriangles(state, input, vertexOutputSize, threadId);
            });

            // Pass 2:
            //
            // process all the tiles created in pass 1. Create one task per
            // tile (not one per core), and distribute all the tasks among the cores.  The
            // third parameter to the Parallel::For call is the work
            // distribution granularity.  Increasing its value might reduce
            // scheduling overhead (consecutive tiles go to the same core),
            // but could increase load imbalance.  (You can probably leave it
            // at one.)
            Parallel::For(0, gridWidth*gridHeight, 1, [&](int tileId)
            {
                ProcessBin(state, input, vertexOutputSize, tileId);
            });

        }
    };

    IRasterRenderer * CreateTiledRenderer()
    {
        return new RendererImplBase<TiledRendererAlgorithm>();
    }
}
