#include "RendererImplBase.h"
#include "CommonTraceCollection.h"

namespace RasterRenderer
{
    class ForwardNonTiledRendererAlgorithm
    {
    private:
        static const int FragmentBufferSize = 65536;
        List<Fragment, AlignedAllocator<16>> fragmentBuffer;
        FrameBuffer * frameBuffer;
    public:
        inline void Init()
        {
            fragmentBuffer.Reserve(FragmentBufferSize);
        }

        inline void Clear(const Vec4 & clearColor, bool color, bool depth)
        {
            this->frameBuffer->Clear(clearColor, color, depth);
        }

        inline void SetFrameBuffer(FrameBuffer * frameBuffer)
        {
            this->frameBuffer = frameBuffer;
        }

        inline void Finish()
        {
        }

        // RenderProjectedBatch --
        //
        // input: a list of post-clipping projected triangles to rasterize.
        //
        // This method is the workhorse routine of the back-end of the
        // renderer. It will iterate over all triangles in the batch
        // represented by 'input'.  For each triangle, it will
        // generate quad fragments and place them in a buffer.  When
        // the fragment buffer fills, the contents of the fragment
        // buffer are shaded and the resulting fragments are blended
        // into the frame buffer.  This process repeats until all
        // triangles have been rasterized from 'input'.
        inline void RenderProjectedBatch(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize)
        {

            // Note: Some of the code below uses SSE intrinsic
            // operations for performance.  When packing quad-fragment
            // values into SSE registers, the packing is done in
            // counter-clockwise order: lane 0 is the bottom-left
            // pixel in the quad. lane 1 is the bottom-right pixel.
            // Lane 2 is the top-left, etc.
            __m128 one = _mm_set_ps1(1.0f);

            // set quad-fragment visibility sample locations. Sample
            // locations are encoded relative to the bottom left of
            // quad, and stored in N.4 fixed-point format: So
            // (x=8,y=8) is the middle of the bottom-left pixel,
            // (x=24, y=8) is the middle of the bottom-right pixel.

            // note: set_epi32 works in reverse order: (v3,v2,v1,v0)
            static __m128i xOffset = _mm_set_epi32(24, 8, 24, 8);
            static __m128i yOffset = _mm_set_epi32(24, 24, 8, 8);


            // the starter codebase does not support multi-sampling,
            // so for now GetSampleCount() will return 1.
            int sampleCount = this->frameBuffer->GetSampleCount();
            int multiSampleLevel = this->frameBuffer->GetSampleCountLog2();

            ProjectedTriangleInput::Iterator triIter(input);

            // rasterize each triangle sequentially :-(
            while (triIter.Valid())
            {
                // quad fragments generated by rasterization will be placed
                // into this buffer
                fragmentBuffer.Clear();

                while (triIter.Valid())
                {
                    // get the next triangle from input stream to rasterize
                    auto tri = triIter.GetProjectedTriangle();

                    // load triangle equations to SIMD registers (needed by
                    // rasterization helper functions)
                    TriangleSIMD triSIMD;
                    triSIMD.Load(tri);

                    // start rasterization
                    RasterizeTriangle(0, 0, frameBuffer->GetWidth(), frameBuffer->GetHeight(), tri, triSIMD, [&](int qfx, int qfy, bool trivialAccept)
                    {
                        // BruteForceRasterizer invokes this lambda once per
                        // "potentially covered" quad fragment.

                        // The quad fragment's base pixel coordinate
                        // is (qfx,qfy).  These are in units of pixels.

                        // trivialAccept is true if the rasterizer determined this
                        // quad fragment is a trivial accept case for the current
                        // triangle.

                        // coordX_center and coordY_center hold the
                        // pixel-center coordinates for the four pixels in this
                        // quad fragment. Coordinates are stored in an N.4
                        // fixed-point representation. (note shift-left by 4 of qfx and qfy,
                        // which convertes these values from pixel values to
                        // fixed-point values in N.4 format)
                        __m128i coordX_center, coordY_center;
                        coordX_center = _mm_add_epi32(_mm_set1_epi32(qfx << 4), xOffset);
                        coordY_center = _mm_add_epi32(_mm_set1_epi32(qfy << 4), yOffset);

                        int x = qfx;
                        int y = qfy;

                        // perform coverage test for all samples, if necessary
                        int coverageMask = trivialAccept ? 0xFFFF : triSIMD.TestQuadFragment(coordX_center, coordY_center);

                        // evaluate Z for each sample point
                        auto zValues = triSIMD.GetZ(coordX_center, coordY_center);

                        // copy z values out of SIMD register
                        CORE_LIB_ALIGN_16(float zStore[4]);
                        _mm_store_ps(zStore, zValues);

                        FragmentCoverageMask visibility;

                        // This is the early-Z check:
                        //
                        // As a result of early-Z the fragment's coverage mask is
                        // updated to have bits set only for covered samples that
                        // passed Z.  Later in the pipeline this mask is needed to
                        // know what frame buffer color samples to update.

                        if (coverageMask & 0x0008)
                        {
                            if (frameBuffer->GetZ(x, y, 0) > zStore[0])
                            {
                                visibility.SetBit(0);
                                frameBuffer->SetZ(x, y, 0, zStore[0]);
                            }
                        }
                        if (coverageMask & 0x0080)
                        {
                            if (frameBuffer->GetZ(x + 1, y, 0) > zStore[1])
                            {
                                visibility.SetBit(1);
                                frameBuffer->SetZ(x + 1, y, 0, zStore[1]);
                            }
                        }
                        if (coverageMask & 0x0800)
                        {
                            if (frameBuffer->GetZ(x, y + 1, 0) > zStore[2])
                            {
                                visibility.SetBit(2);
                                frameBuffer->SetZ(x, y + 1, 0, zStore[2]);
                            }
                        }
                        if (coverageMask & 0x8000)
                        {
                            if (frameBuffer->GetZ(x + 1, y + 1, 0) > zStore[3])
                            {
                                visibility.SetBit(3);
                                frameBuffer->SetZ(x + 1, y + 1, 0, zStore[3]);
                            }
                        }

                        // Add this quad fragment to the list of fragments to shade if
                        // any sample is visible
                        if (visibility.Any())
                        {
                            // retrieve triangle barycentric coordinates at each
                            // sample point in stick them into the quad fragment.
                            // Renderer uses these coordinates to evaluate
                            // triangle attributes at the shading sample point
                            // during shading (e.g., to sample texture coordinates uv)
                            __m128 gamma, beta, alpha;
                            triSIMD.GetCoordinates(gamma, alpha, beta, coordX_center, coordY_center);

                            // push new fragment into list of fragments to shade
                            fragmentBuffer.GrowToSize(fragmentBuffer.Count() + 1);
                            fragmentBuffer.Last().Set(x, y, gamma, beta, triIter.GetCoreId(), triIter.GetPtr(), tri.ConstantId, visibility);
                        }
                    });

                    // move to next triangle
                    triIter.MoveNext();

                    // if fragment buffer is full. We need to drain it before
                    // rasterizing more triangles.
                    if (fragmentBuffer.Count() >= FragmentBufferSize)
                        break;
                }

                // shade all quad fragments stored in the fragments-to-shade
                // list (this call will perform all the shading, and it will process fragments in parallel)
                ShadeFragments(state, fragmentBuffer.Count(), fragmentBuffer.Buffer(), input, vertexOutputSize);

                // write shading results to framebuffer in serial :-(
                // notice no depth-test before here since it was already done
                // as part of early-z.
                for (auto frag : fragmentBuffer)
                {
                    // Note: RGBA color results are stored interleaved in the
                    // fragment structure: r0r1r2r3g0g1g2g3 ...  This layout allows for
                    // SIMD shading in ShadeFragments().


                    if (frag.BitMask.GetBit(0)) {
                        frameBuffer->SetPixel(frag.x, frag.y, 0, Vec4(frag.ShadeResult[0], frag.ShadeResult[4], frag.ShadeResult[8], frag.ShadeResult[12]));
                    }
                    if (frag.BitMask.GetBit(1)) {
                        frameBuffer->SetPixel(frag.x + 1, frag.y, 0, Vec4(frag.ShadeResult[1], frag.ShadeResult[5], frag.ShadeResult[9], frag.ShadeResult[13]));
                    }
                    if (frag.BitMask.GetBit(2)) {
                        frameBuffer->SetPixel(frag.x, frag.y + 1, 0, Vec4(frag.ShadeResult[2], frag.ShadeResult[6], frag.ShadeResult[10], frag.ShadeResult[14]));
                    }
                    if (frag.BitMask.GetBit(3)) {
                        frameBuffer->SetPixel(frag.x + 1, frag.y + 1, 0, Vec4(frag.ShadeResult[3], frag.ShadeResult[7], frag.ShadeResult[11], frag.ShadeResult[15]));
                    }
                }
            }
        }
    };

    IRasterRenderer * CreateForwardNonTiledRenderer()
    {
        return new RendererImplBase<ForwardNonTiledRendererAlgorithm>();
    }
}
