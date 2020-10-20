#include "CoreLib/Basic.h"
#include "IRasterRenderer.h"
#include "TestScene.h"
#include "ViewSettings.h"
#include "CoreLib/PerformanceCounter.h"
#include "CoreLib/LibIO.h"
#include <string>

using namespace CoreLib::Basic;
using namespace CoreLib::Diagnostics;
using namespace RasterRenderer;

namespace RasterRenderer
{
    namespace Testing
    {
        class TestDriver
        {
        private:
            String baseDir;
            String testName;
            String outputFileName;
            IRasterRenderer * renderer;
            FrameBuffer frameBuffer;
            ViewSettings viewSettings;
        public:
            TestDriver(int Width, int Height, bool tiled, const String& test, const String& output, const String & baseDir)
                :testName(test), outputFileName(output), frameBuffer(Width, Height), baseDir(baseDir)
            {
                if (tiled)
                    renderer = CreateTiledRenderer();
                else
                    renderer = CreateForwardNonTiledRenderer();

                renderer->SetFrameBuffer(&frameBuffer);
            }

            ~TestDriver()
            {
                DestroyRenderer(renderer);
            }

            void Run()
            {
                RefPtr<TestScene> scene;

                printf("Loading scene...\n");

                if (testName == L"triangle")
                    scene = CreateTestScene0(viewSettings);
                else if (testName == L"square")
                    scene = CreateTestScene1(viewSettings, baseDir);
                else if (testName == L"sibenik")
                    scene = CreateTestScene2(viewSettings, baseDir);
                else if (testName == L"bunny")
                    scene = CreateTestScene3(viewSettings, baseDir);
                else if (testName == L"sponza")
                    scene = CreateTestScene4(viewSettings, baseDir);
                else if (testName == L"warehouse")
                    scene = CreateTestScene5(viewSettings, baseDir);
                else if (testName == L"alphablend")
                    scene = CreateTestScene6(viewSettings);
				else if (testName == L"station")
					scene = CreateTestScene7(viewSettings, baseDir);
				else if (testName == L"alpha_order")
					scene = CreateTestScene8(viewSettings);
                else
                {
                    printf("Unknown scene \"%s\".\n", testName.ToMultiByteString());
                    return;
                }

                printf("Rendering scene: %s (%dx%d)\n", testName.ToMultiByteString(), frameBuffer.GetWidth(), frameBuffer.GetHeight());

                // prime things with a render before starting the timer
                renderer->Clear(Vec4(0.0f, 0.0f, 0.0f, 0.0f));
                scene->Draw(renderer);
                renderer->Finish();

                // now render a few frames with timing, report the best time
                const int frameCount = 6;
                double minTime = 10e10;
                for (int i = 0; i < frameCount; i++)
                {
                    auto counter = PerformanceCounter::Start();
                    renderer->Clear(scene->ClearColor);
                    scene->Draw(renderer);
                    renderer->Finish();
                    auto elapsed = PerformanceCounter::ToSeconds(PerformanceCounter::End(counter));
                    minTime = (elapsed < minTime) ? elapsed : minTime;
                }

                printf("Frame render time: %lf ms\n", 1000.0 * minTime, frameCount);

                frameBuffer.SaveColorBuffer(outputFileName);
            }
        };
    }
}

void Usage(char* binaryName)
{
    printf("Renderer Test Driver\n"
           "Usage:\n"
           "   %s testname [-w imagewidth] [-h imageweight] [-tiled] [-mediadir dir]\n\n"
           "   testname can be: triangle, square, sibenik, bunny, sponza, warehouse, alphablend, station, alpha_order\n\n"
           "   mediadir: base directory without trialing slash: e.g., ../../Media\n\n",
           binaryName);
}

int main(int argc, char* argv[])
{
    int width = 1024;
    int height = 768;
    String testName = L"sponza";
    String testOutput = L"";
    String baseDir = L"./Media";
    bool tiled = false;
    // parse commandline
    int ptr = 1;
    if (argc >= 2)
        testName = argv[1];
    while (ptr < argc)
    {
        if (ptr < argc - 1)
        {
            if (String(argv[ptr]) == L"-o")
            {
                ptr++;
                testOutput = argv[ptr];
            }
            else if (String(argv[ptr]) == L"-w")
            {
                ptr++;
                width = StringToInt(argv[ptr]);
            }
            else if (String(argv[ptr]) == L"-h")
            {
                ptr++;
                height = StringToInt(String(argv[ptr]));
            }
            else if (String(argv[ptr]) == L"-mediadir")
            {
                ptr++;
                baseDir = argv[ptr];
            }
        }
        if (String(argv[ptr]) == L"-tiled")
        {
            tiled = true;
        }
        else if (String(argv[ptr]) == L"-help" ||
                 String(argv[ptr]) == L"--help" ||
                 String(argv[ptr]) == L"-?")
        {
            Usage(argv[0]);
            exit(0);
        }
        ptr++;
    }
    if (width <= 0 || width > 4096 || height <= 0 || height > 4096)
    {
        printf("Invalid resolution.\n");
        return -1;
    }

    if (testOutput == L"")
    {
        testOutput = testName + L"_output.bmp";
    }

    try
    {
        if (!tiled)
            printf("*** Running REFERENCE non-tiled renderer implementation ***\n");
        else
            printf("*** Running TILED renderer implementation ***\n");
        Testing::TestDriver driver(width, height, tiled, testName, testOutput, baseDir);
        driver.Run();
    }
    catch (const CoreLib::IO::IOException & ex)
    {
        printf("%s\n", ex.Message.ToMultiByteString());
        printf("Make sure the 'Media' directory is placed in the current working dir, or basepath set using -mediadir\n");
    }
    catch (const Exception & ex)
    {
        printf("%s", ex.Message.ToMultiByteString());
    }
#ifdef WIN32
    _CrtDumpMemoryLeaks();
#endif
    return 0;
}

